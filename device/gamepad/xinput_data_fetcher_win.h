// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XINPUT_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_XINPUT_DATA_FETCHER_WIN_H_

#include <memory>

#include "build/build_config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Unknwn.h>
#include <windows.h>

#include <WinDef.h>
#include <XInput.h>
#include <stdlib.h>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_native_library.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/xinput_haptic_gamepad_win.h"

namespace device {

// XInputGetStateEx uses a slightly larger struct than XInputGetState.
struct XInputGamepadEx {
  WORD wButtons;
  BYTE bLeftTrigger;
  BYTE bRightTrigger;
  SHORT sThumbLX;
  SHORT sThumbLY;
  SHORT sThumbRX;
  SHORT sThumbRY;
  DWORD dwPaddingReserved;
};

struct XInputStateEx {
  DWORD dwPacketNumber;
  XInputGamepadEx Gamepad;
};

class DEVICE_GAMEPAD_EXPORT XInputDataFetcherWin : public GamepadDataFetcher {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<XInputDataFetcherWin,
                                                GamepadSource::kWinXinput>;

  // The function types we use from XInput DLL (either xinput1_4.dll or
  // xinput9_1_0.dll).
  typedef void(WINAPI* XInputEnableFunc)(BOOL enable);
  typedef DWORD(WINAPI* XInputGetCapabilitiesFunc)(
      DWORD dwUserIndex,
      DWORD dwFlags,
      XINPUT_CAPABILITIES* pCapabilities);
  typedef DWORD(WINAPI* XInputGetStateFunc)(DWORD dwUserIndex,
                                            XINPUT_STATE* pState);
  typedef DWORD(WINAPI* XInputGetStateExFunc)(DWORD dwUserIndex,
                                              XInputStateEx* pState);

  XInputDataFetcherWin();

  XInputDataFetcherWin(const XInputDataFetcherWin&) = delete;
  XInputDataFetcherWin& operator=(const XInputDataFetcherWin&) = delete;

  ~XInputDataFetcherWin() override;

  GamepadSource source() override;

  // GamepadDataFetcher implementation.
  void GetGamepadData(bool devices_changed_hint) override;

  void PlayEffect(int pad_index,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;

  void ResetVibration(
      int pad_index,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

  // Hooks to set fake implementations of the XInput OS functions for testing
  using XInputGetCapabilitiesFunctionCallback =
      base::RepeatingCallback<XInputGetCapabilitiesFunc()>;
  static void OverrideXInputGetCapabilitiesFuncForTesting(
      XInputGetCapabilitiesFunctionCallback callback);

  using XInputGetStateExFunctionCallback =
      base::RepeatingCallback<XInputGetStateExFunc()>;
  static void OverrideXInputGetStateExFuncForTesting(
      XInputGetStateExFunctionCallback callback);

  void InitializeForWgiDataFetcher();

  bool IsAnyMetaButtonPressed();

 private:
  void OnAddedToProvider() override;

  // Get functions from dynamically loading the xinput dll.
  // Returns true if loading was successful.
  bool GetXInputDllFunctions();
  // Same as `GetXInputDllFunctions` but loads only the functions required by
  // WgiDataFetcher and also checks for test hooks.
  bool GetXInputDllFunctionsForWgiDataFetcher();

  // Scan for connected XInput and DirectInput gamepads.
  void EnumerateDevices();
  void GetXInputPadData(int i);

  static XInputGetCapabilitiesFunctionCallback&
  GetXInputGetCapabilitiesFunctionCallback();

  static XInputGetStateExFunctionCallback&
  GetXInputGetStateExFunctionCallback();

  base::ScopedNativeLibrary xinput_dll_;
  bool xinput_available_;

  // Function pointers to XInput functionality, retrieved in
  // |GetXInputDllFunctions|.
  XInputGetCapabilitiesFunc xinput_get_capabilities_;
  XInputGetStateFunc xinput_get_state_;
  XInputGetStateExFunc xinput_get_state_ex_;
  XInputHapticGamepadWin::XInputSetStateFunc xinput_set_state_;

  bool xinput_connected_[XUSER_MAX_COUNT];
  std::unique_ptr<XInputHapticGamepadWin> haptics_[XUSER_MAX_COUNT];
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_XINPUT_DATA_FETCHER_WIN_H_
