// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_

#include <Unknwn.h>
#include <windows.h>

#include <WinDef.h>
#include <hidsdi.h>
#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <memory>

#include "base/containers/heap_array.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/message_window.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/raw_input_gamepad_device_win.h"

namespace device {

class RawInputDataFetcher : public GamepadDataFetcher {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<RawInputDataFetcher,
                                                GamepadSource::kWinRaw>;

  explicit RawInputDataFetcher();
  RawInputDataFetcher(const RawInputDataFetcher&) = delete;
  RawInputDataFetcher& operator=(const RawInputDataFetcher&) = delete;
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
  base::HeapArray<RAWINPUTDEVICE> GetRawInputDevices(DWORD flags);
  void ClearControllers();

  // The window to receive RawInput events.
  std::unique_ptr<base::win::MessageWindow> window_;

  // When true, XInput devices will not be enumerated by this data fetcher.
  // This should be enabled when the platform data fetcher is active to avoid
  // enumerating XInput gamepads twice.
  bool filter_xinput_ = true;

  // True if we are registered to receive RawInput events.
  bool events_monitored_ = false;

  // The last ID assigned to an enumerated device.
  int last_source_id_ = 0;

  // Connected devices, keyed by device handle.
  std::unordered_map<HANDLE, std::unique_ptr<RawInputGamepadDeviceWin>>
      controllers_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
