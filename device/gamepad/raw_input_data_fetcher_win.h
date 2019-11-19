// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_

#include <Unknwn.h>
#include <WinDef.h>
#include <hidsdi.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/win/message_window.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/hid_dll_functions_win.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/raw_input_gamepad_device_win.h"

namespace device {

class RawInputDataFetcher : public GamepadDataFetcher,
                            public base::SupportsWeakPtr<RawInputDataFetcher> {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<RawInputDataFetcher,
                                                GAMEPAD_SOURCE_WIN_RAW>;

  explicit RawInputDataFetcher();
  ~RawInputDataFetcher() override;

  GamepadSource source() override;

  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;
  void PlayEffect(int source_id,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;
  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

  bool DisconnectUnrecognizedGamepad(int source_id) override;

 private:
  void OnAddedToProvider() override;

  void StartMonitor();
  void StopMonitor();
  void EnumerateDevices();
  RawInputGamepadDeviceWin* DeviceFromSourceId(int source_id);
  // Handles WM_INPUT messages.
  LRESULT OnInput(HRAWINPUT input_handle);
  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);
  RAWINPUTDEVICE* GetRawInputDevices(DWORD flags);
  void ClearControllers();

  // The window to receive RawInput events.
  std::unique_ptr<base::win::MessageWindow> window_;

  // True if DLL loading succeeded and methods for enumerating and polling
  // RawInput devices are available.
  bool rawinput_available_ = false;

  // When true, XInput devices will not be enumerated by this data fetcher.
  // This should be enabled when the platform data fetcher is active to avoid
  // enumerating XInput gamepads twice.
  bool filter_xinput_ = true;

  // True if we are registered to receive RawInput events.
  bool events_monitored_ = false;

  // The last ID assigned to an enumerated device.
  int last_source_id_ = 0;

  // HID functions loaded from hid.dll.
  std::unique_ptr<HidDllFunctionsWin> hid_functions_;

  // Connected devices, keyed by device handle.
  std::unordered_map<HANDLE, std::unique_ptr<RawInputGamepadDeviceWin>>
      controllers_;

  DISALLOW_COPY_AND_ASSIGN(RawInputDataFetcher);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
