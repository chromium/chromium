// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/raw_input_data_fetcher_win.h"

#include <stddef.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/gamepad_uma.h"
#include "device/gamepad/nintendo_controller.h"

namespace device {

namespace {

const uint16_t DeviceUsages[] = {
    RawInputGamepadDeviceWin::kGenericDesktopJoystick,
    RawInputGamepadDeviceWin::kGenericDesktopGamePad,
    RawInputGamepadDeviceWin::kGenericDesktopMultiAxisController,
};

}  // namespace

RawInputDataFetcher::RawInputDataFetcher() = default;

RawInputDataFetcher::~RawInputDataFetcher() {
  StopMonitor();
  ClearControllers();
  DCHECK(!window_);
  DCHECK(!events_monitored_);
}

GamepadSource RawInputDataFetcher::source() {
  return Factory::static_source();
}

base::HeapArray<RAWINPUTDEVICE> RawInputDataFetcher::GetRawInputDevices(
    DWORD flags) {
  size_t usage_count = std::size(DeviceUsages);
  auto devices = base::HeapArray<RAWINPUTDEVICE>::Uninit(usage_count);
  for (size_t i = 0; i < usage_count; ++i) {
    devices[i].dwFlags = flags;
    devices[i].usUsagePage = 1;
    devices[i].usUsage = DeviceUsages[i];
    devices[i].hwndTarget = (flags & RIDEV_REMOVE) ? 0 : window_->hwnd();
  }
  return devices;
}

void RawInputDataFetcher::PauseHint(bool pause) {
  if (pause)
    StopMonitor();
  else
    StartMonitor();
}

void RawInputDataFetcher::StartMonitor() {
  if (events_monitored_)
    return;

  if (!window_) {
    window_ = std::make_unique<base::win::MessageWindow>();
    if (!window_->Create(base::BindRepeating(
            &RawInputDataFetcher::HandleMessage, base::Unretained(this)))) {
      PLOG(ERROR) << "Failed to create the raw input window";
      window_.reset();
      return;
    }
  }

  // Register to receive raw HID input.
  auto devices = GetRawInputDevices(RIDEV_INPUTSINK);
  if (!::RegisterRawInputDevices(devices.data(), std::size(DeviceUsages),
                                 sizeof(RAWINPUTDEVICE))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed for RIDEV_INPUTSINK";
    window_.reset();
    return;
  }

  events_monitored_ = true;
}

void RawInputDataFetcher::StopMonitor() {
  if (!events_monitored_)
    return;

  // Stop receiving raw input.
  DCHECK(window_);
  auto devices = GetRawInputDevices(RIDEV_INPUTSINK);
  if (!::RegisterRawInputDevices(devices.data(), std::size(DeviceUsages),
                                 sizeof(RAWINPUTDEVICE))) {
    PLOG(INFO) << "RegisterRawInputDevices() failed for RIDEV_REMOVE";
  }

  events_monitored_ = false;
  window_.reset();
}

bool RawInputDataFetcher::DisconnectUnrecognizedGamepad(int source_id) {
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it) {
    if (it->second->GetSourceId() == source_id) {
      it->second->Shutdown();
      controllers_.erase(it);
      return true;
    }
  }
  return false;
}

void RawInputDataFetcher::ClearControllers() {
  for (const auto& entry : controllers_)
    entry.second->Shutdown();
  controllers_.clear();
}

void RawInputDataFetcher::GetGamepadData(bool devices_changed_hint) {
  if (devices_changed_hint)
    EnumerateDevices();

  for (const auto& entry : controllers_) {
    const RawInputGamepadDeviceWin* device = entry.second.get();
    PadState* state = GetPadState(device->GetSourceId());
    if (!state)
      continue;

    device->ReadPadState(&state->data);
  }
}

void RawInputDataFetcher::EnumerateDevices() {
  UINT count = 0;
  UINT result =
      ::GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceList() failed";
    return;
  }
  DCHECK_EQ(0u, result);

  auto device_list = base::HeapArray<RAWINPUTDEVICELIST>::Uninit(count);
  result = ::GetRawInputDeviceList(device_list.data(), &count,
                                   sizeof(RAWINPUTDEVICELIST));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceList() failed";
    return;
  }
  DCHECK_EQ(count, result);

  std::unordered_set<HANDLE> enumerated_device_handles;
  for (UINT i = 0; i < count; ++i) {
    if (device_list[i].dwType == RIM_TYPEHID) {
      HANDLE device_handle = device_list[i].hDevice;
      auto controller_it = controllers_.find(device_handle);

      RawInputGamepadDeviceWin* device;
      if (controller_it != controllers_.end()) {
        device = controller_it->second.get();
      } else {
        int source_id = ++last_source_id_;
        auto new_device = std::make_unique<RawInputGamepadDeviceWin>(
            device_handle, source_id);
        if (!new_device->IsValid()) {
          new_device->Shutdown();
          continue;
        }

        const int vendor_int = new_device->GetVendorId();
        const int product_int = new_device->GetProductId();
        const int version_number = new_device->GetVersionNumber();
        const std::wstring product_wstring = new_device->GetProductString();
        const std::string product_string = base::SysWideToUTF8(product_wstring);
        const GamepadId gamepad_id = GamepadIdList::Get().GetGamepadId(
            product_string, vendor_int, product_int);

        if (NintendoController::IsNintendoController(gamepad_id)) {
          // Nintendo devices are handled by the Nintendo data fetcher.
          new_device->Shutdown();
          continue;
        }

        bool is_recognized = GamepadId::kUnknownGamepad != gamepad_id;

        // Record gamepad metrics before excluding XInput devices. This allows
        // us to recognize XInput devices even though the XInput API masks
        // the vendor and product IDs.
        RecordConnectedGamepad(gamepad_id);

        // The presence of "IG_" in the device name indicates that this is an
        // XInput Gamepad. Skip enumerating these devices and let the XInput
        // path handle it.
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ee417014.aspx
        const std::wstring device_name = new_device->GetDeviceName();
        if (filter_xinput_ && base::Contains(device_name, L"IG_")) {
          new_device->Shutdown();
          continue;
        }

        PadState* state = GetPadState(source_id, is_recognized);
        if (!state) {
          new_device->Shutdown();
          continue;  // No slot available for this gamepad.
        }

        auto emplace_result =
            controllers_.emplace(device_handle, std::move(new_device));
        device = emplace_result.first->second.get();

        Gamepad& pad = state->data;
        pad.connected = true;

        pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
        pad.vibration_actuator.not_null = device->SupportsVibration();

        state->mapper = GetGamepadStandardMappingFunction(
            product_string, vendor_int, product_int,
            /*hid_specification_version=*/0, version_number,
            GAMEPAD_BUS_UNKNOWN);
        state->axis_mask = 0;
        state->button_mask = 0;

        UpdateGamepadStrings(product_string, vendor_int, product_int,
                             state->mapper != nullptr, pad);
      }

      enumerated_device_handles.insert(device_handle);
    }
  }

  // Clear out old controllers that weren't part of this enumeration pass.
  auto controller_it = controllers_.begin();
  while (controller_it != controllers_.end()) {
    if (enumerated_device_handles.find(controller_it->first) ==
        enumerated_device_handles.end()) {
      controller_it->second->Shutdown();
      controller_it = controllers_.erase(controller_it);
    } else {
      ++controller_it;
    }
  }
}

RawInputGamepadDeviceWin* RawInputDataFetcher::DeviceFromSourceId(
    int source_id) {
  for (const auto& entry : controllers_) {
    if (entry.second->GetSourceId() == source_id)
      return entry.second.get();
  }
  return nullptr;
}

void RawInputDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  RawInputGamepadDeviceWin* device = DeviceFromSourceId(source_id);
  if (!device) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  if (!device->SupportsVibration()) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  device->PlayEffect(type, std::move(params), std::move(callback),
                     std::move(callback_runner));
}

void RawInputDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  RawInputGamepadDeviceWin* device = DeviceFromSourceId(source_id);
  if (!device) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  if (!device->SupportsVibration()) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  device->ResetVibration(std::move(callback), std::move(callback_runner));
}

LRESULT RawInputDataFetcher::OnInput(HRAWINPUT input_handle) {
  // Get the size of the input record.
  UINT size = 0;
  UINT result = ::GetRawInputData(input_handle, RID_INPUT, nullptr, &size,
                                  sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(0u, result);

  // Retrieve the input record.
  auto buffer = base::HeapArray<uint8_t>::Uninit(size);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.data());
  result = ::GetRawInputData(input_handle, RID_INPUT, buffer.data(), &size,
                             sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(size, result);

  // Notify the observer about events generated locally.
  if (input->header.dwType == RIM_TYPEHID && input->header.hDevice != NULL) {
    auto it = controllers_.find(input->header.hDevice);
    if (it != controllers_.end())
      it->second->UpdateGamepad(input);
  }

  return ::DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool RawInputDataFetcher::HandleMessage(UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam,
                                        LRESULT* result) {
  switch (message) {
    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

}  // namespace device
