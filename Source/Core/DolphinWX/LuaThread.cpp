// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <lua.hpp>

#include "Core/Core.h"
#include "DolphinWX/LuaScripting.h"
#include "Core/Movie.h"
#include "DolphinWX/Frame.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Lua
{

LuaThread::LuaThread(LuaScriptFrame* p, const wxString& file)
    : m_parent(p), m_file_path(file), wxThread()
{
  // Zero out controllers
	for (int i = 0; i < 4; i++)
	{
		//GC Pad
		m_pad_status[i].button = 0;
		m_pad_status[i].stickX = GCPadStatus::MAIN_STICK_CENTER_X;
		m_pad_status[i].stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
		m_pad_status[i].triggerLeft = 0;
		m_pad_status[i].triggerRight = 0;
		m_pad_status[i].substickX = GCPadStatus::C_STICK_CENTER_X;
		m_pad_status[i].substickY = GCPadStatus::C_STICK_CENTER_Y;
		
		// Wiimote
		m_Lua_Wiimote[i].m_padWii_status.hex = 0;
		//m_Lua_Wiimote[i].accelData.x = WiimoteEmu::ACCEL_ZERO_G; // hopefully this is right idk
		//m_Lua_Wiimote[i].accelData.y = WiimoteEmu::ACCEL_ZERO_G;
		//m_Lua_Wiimote[i].accelData.z = WiimoteEmu::ACCEL_ZERO_G;

		// Nunchuk
		m_Lua_Wiimote[i].m_nunchuk.bt.hex = 0;
		m_Lua_Wiimote[i].m_nunchuk.jx = WiimoteEmu::Nunchuk::STICK_CENTER;
		m_Lua_Wiimote[i].m_nunchuk.jy = WiimoteEmu::Nunchuk::STICK_CENTER;
		// Classic
		m_Lua_Wiimote[i].m_classic.bt.hex = 0;
		m_Lua_Wiimote[i].m_classic.regular_data.lx = WiimoteEmu::Classic::LEFT_STICK_CENTER_X;
		m_Lua_Wiimote[i].m_classic.regular_data.ly = WiimoteEmu::Classic::LEFT_STICK_CENTER_Y;
		m_Lua_Wiimote[i].m_classic.rx1 = WiimoteEmu::Classic::RIGHT_STICK_CENTER_X & 0x1;
		m_Lua_Wiimote[i].m_classic.rx2 = WiimoteEmu::Classic::RIGHT_STICK_CENTER_X & 0x3;
		m_Lua_Wiimote[i].m_classic.rx3 = WiimoteEmu::Classic::RIGHT_STICK_CENTER_X & 0x3;
		m_Lua_Wiimote[i].m_classic.ry = WiimoteEmu::Classic::RIGHT_STICK_CENTER_Y;
	}


  // Register GetValues()
  Movie::SetGCInputManip([this](GCPadStatus* status, int number)
  {
    GetValues(status, number);
  }, Movie::GCManipIndex::LuaGCManip);

  Movie::SetWiiInputManip([this](u8* data, WiimoteEmu::ReportFeatures rptf, int controllerID, int ext, const wiimote_key key)
  {
    GetWiiValues(data, rptf, controllerID, ext, key);
  }, Movie::WiiManipIndex::LuaWiiManip);

}

LuaThread::~LuaThread()
{
	// Nullify GC manipulator function to prevent crash when lua console is closed
	Movie::SetGCInputManip(nullptr, Movie::GCManipIndex::LuaGCManip);
	Movie::SetWiiInputManip(nullptr, Movie::WiiManipIndex::LuaWiiManip);
	m_parent->NullifyLuaThread();
}

wxThread::ExitCode LuaThread::Entry()
{
  std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), lua_close);

  // Register
  lua_sethook(state.get(), &HookFunction, LUA_MASKLINE, 0);

  // Make standard libraries available to loaded script
  luaL_openlibs(state.get());

  //Make custom libraries available to loaded script
  luaopen_libs(state.get());

  if (luaL_loadfile(state.get(), m_file_path) != LUA_OK)
  {
    m_parent->Log("Error opening file.\n");

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }

  // Pause emu
  Core::SetState(Core::CORE_PAUSE);

  if (lua_pcall(state.get(), 0, LUA_MULTRET, 0) != LUA_OK)
  {
    m_parent->Log(lua_tostring(state.get(), 1));

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }
  Exit();
  return reinterpret_cast<wxThread::ExitCode>(0);
}

void LuaThread::GetValues(GCPadStatus *PadStatus, int number)
{
  
  if (LuaThread::m_pad_status[number].stickX != GCPadStatus::MAIN_STICK_CENTER_X)
		PadStatus->stickX = LuaThread::m_pad_status[number].stickX;

  if (LuaThread::m_pad_status[number].stickY != GCPadStatus::MAIN_STICK_CENTER_Y)
	  PadStatus->stickY = LuaThread::m_pad_status[number].stickY;

  if (LuaThread::m_pad_status[number].triggerLeft != 0)
	  PadStatus->triggerLeft = LuaThread::m_pad_status[number].triggerLeft;

  if (LuaThread::m_pad_status[number].triggerRight != 0)
	  PadStatus->triggerRight = LuaThread::m_pad_status[number].triggerRight;

  if (LuaThread::m_pad_status[number].substickX != GCPadStatus::C_STICK_CENTER_X)
	  PadStatus->substickX = LuaThread::m_pad_status[number].substickX;

  if (LuaThread::m_pad_status[number].substickY != GCPadStatus::C_STICK_CENTER_Y)
	  PadStatus->substickY = LuaThread::m_pad_status[number].substickY;

  PadStatus->button |= LuaThread::m_pad_status[number].button;

  //Update internal gamepad representation with the same struct we're sending out
  m_last_pad_status[number] = *PadStatus;
}

void LuaThread::GetWiiValues(u8 *data, WiimoteEmu::ReportFeatures rptf, int controllerID, int ext, const wiimote_key key)
{
	u8 *const coreData = rptf.core ? (data + rptf.core) : nullptr;
	u8 *const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
	u8 *const irData = rptf.ir ? (data + rptf.ir) : nullptr;
	u8 *const extData = rptf.ext ? (data + rptf.ext) : nullptr;	

	if (ext != 2)
	{
		if (coreData)
			((wm_buttons *)coreData)->hex |= LuaThread::m_Lua_Wiimote[controllerID].m_padWii_status.hex;
		if (accelData)
		{
			//((wm_accel *)accelData)->x = LuaThread::m_Lua_Wiimote[controllerID].accelData.x;
			//((wm_accel *)accelData)->y = LuaThread::m_Lua_Wiimote[controllerID].accelData.y;
			//((wm_accel *)accelData)->z = LuaThread::m_Lua_Wiimote[controllerID].accelData.z;
		}
		if (irData)
		{ // Lets do this a different time

		}		
	}
	// Nunchuk
	if (extData && ext == 1)
	{
		WiimoteDecrypt(&key, (u8 *)(extData), 0, sizeof(wm_nc));
		// Buttons
		if (LuaThread::m_Lua_Wiimote[controllerID].m_nunchuk.bt.hex != 0)
		{
		((wm_nc *)(extData))->bt.hex ^= 3; // XOR with 0x3 to "normalize"
		((wm_nc *)(extData))->bt.hex |= (LuaThread::m_Lua_Wiimote[controllerID].m_nunchuk.bt.hex);
		((wm_nc *)(extData))->bt.hex ^= 3; // XOR with 0x3 again
		}

		// Analog stick
		if (LuaThread::m_Lua_Wiimote[controllerID].m_nunchuk.jx != WiimoteEmu::Nunchuk::STICK_CENTER)
		{
			((wm_nc *)(extData))->jx = LuaThread::m_Lua_Wiimote[controllerID].m_nunchuk.jx;
		}
			
		if (LuaThread::m_Lua_Wiimote[controllerID].m_nunchuk.jy != WiimoteEmu::Nunchuk::STICK_CENTER)
		{
			((wm_nc *)(extData))->jy = LuaThread::m_Lua_Wiimote[controllerID].m_nunchuk.jy;
		}

		m_last_Lua_Wiimote[controllerID].m_nunchuk = *((wm_nc *)extData);
		// Encrypt Data 
		WiimoteEncrypt(&key, ((u8 *)(extData)), 0, sizeof(wm_nc));		
	}
	// Classic
	else if (extData && ext == 2)
	{
		// Buttons
		((wm_classic_extension *)extData)->bt.hex |= LuaThread::m_Lua_Wiimote[controllerID].m_classic.bt.hex;
		// Left analog stick
		if (LuaThread::m_Lua_Wiimote[controllerID].m_classic.regular_data.lx != WiimoteEmu::Classic::LEFT_STICK_CENTER_X)
			((wm_classic_extension *)extData)->regular_data.lx = LuaThread::m_Lua_Wiimote[controllerID].m_classic.regular_data.lx;
		if (LuaThread::m_Lua_Wiimote[controllerID].m_classic.regular_data.ly != WiimoteEmu::Classic::LEFT_STICK_CENTER_Y)
			((wm_classic_extension *)extData)->regular_data.ly = LuaThread::m_Lua_Wiimote[controllerID].m_classic.regular_data.ly;
		// Right analog stick
		// for compiling sake just disable this, who's going to use this anyway
		/*if (LuaThread::m_Lua_Wiimote[controllerID].m_classic.rx1 != (WiimoteEmu::Classic::RIGHT_STICK_CENTER_X & 0x1))
			((wm_classic_extension *)extData)->rx1 = LuaThread::m_Lua_Wiimote[controllerID].m_classic.rx1;
		if (LuaThread::m_Lua_Wiimote[controllerID].m_classic.rx2 != WiimoteEmu::Classic::RIGHT_STICK_CENTER_X & 0x3)
			((wm_classic_extension *)extData)->rx2 = LuaThread::m_Lua_Wiimote[controllerID].m_classic.rx2;
		if (LuaThread::m_Lua_Wiimote[controllerID].m_classic.rx3 != WiimoteEmu::Classic::RIGHT_STICK_CENTER_X & 0x3)
			((wm_classic_extension *)extData)->rx3 = LuaThread::m_Lua_Wiimote[controllerID].m_classic.rx3;*/

		m_last_Lua_Wiimote[controllerID].m_classic = *((wm_classic_extension *)extData);

	}

	// Update internal gamepad representation with the same struct we're sending out
	m_last_Lua_Wiimote[controllerID].m_padWii_status = *((wm_buttons *)(rptf.core ? (data + rptf.core) : nullptr));
	
}

void HookFunction(lua_State* L, lua_Debug* ar)
{
  if (LuaScriptFrame::GetCurrentInstance()->GetLuaThread()->m_destruction_flag)
  {
    luaL_error(L, "Script exited.\n");
  }
}

}  // namespace Lua