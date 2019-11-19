// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/nintendo_data_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "device/gamepad/gamepad_service.h"
#include "device/gamepad/gamepad_uma.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

NintendoDataFetcher::NintendoDataFetcher() = default;

NintendoDataFetcher::~NintendoDataFetcher() {
  for (auto& entry : controllers_) {
    auto& device = *entry.second;
    device.Shutdown();
  }
}

GamepadSource NintendoDataFetcher::source() {
  return Factory::static_source();
}

void NintendoDataFetcher::OnAddedToProvider() {
  // Open a connection to the HID service. On a successful connection,
  // OnGetDevices will be called with a list of connected HID devices.
  connector()->Connect(mojom::kServiceName,
                       hid_manager_.BindNewPipeAndPassReceiver());
  hid_manager_->GetDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&NintendoDataFetcher::OnGetDevices,
                     weak_factory_.GetWeakPtr()));
}

void NintendoDataFetcher::OnGetDevices(
    std::vector<mojom::HidDeviceInfoPtr> device_infos) {
  for (auto& device_info : device_infos)
    DeviceAdded(std::move(device_info));
}

void NintendoDataFetcher::OnDeviceReady(int source_id) {
  auto find_it = controllers_.find(source_id);
  if (find_it == controllers_.end())
    return;

  const auto* ready_device_ptr = find_it->second.get();
  DCHECK(ready_device_ptr);
  if (ready_device_ptr->IsComposite())
    return;

  // Extract a connected device that can be associated with the newly-connected
  // device, if it exists.
  std::unique_ptr<NintendoController> associated_device =
      ExtractAssociatedDevice(ready_device_ptr);
  if (associated_device) {
    // Extract the newly-ready device from |controllers_|.
    auto ready_device = std::move(find_it->second);
    controllers_.erase(source_id);

    // Create a composite gamepad from the newly-ready device and the associated
    // device. Each device provides button and axis state for half of the
    // gamepad and vibration effects are split between them.
    int composite_source_id = next_source_id_++;
    auto emplace_result = controllers_.emplace(
        composite_source_id,
        NintendoController::CreateComposite(
            composite_source_id, std::move(associated_device),
            std::move(ready_device), hid_manager_.get()));
    if (emplace_result.second) {
      auto& composite_gamepad = emplace_result.first->second;
      DCHECK(composite_gamepad);
      composite_gamepad->Open(
          base::BindOnce(&NintendoDataFetcher::OnDeviceReady,
                         weak_factory_.GetWeakPtr(), composite_source_id));
    }
  }
}

void NintendoDataFetcher::DeviceAdded(mojom::HidDeviceInfoPtr device_info) {
  if (NintendoController::IsNintendoController(device_info->vendor_id,
                                               device_info->product_id)) {
    AddDevice(std::move(device_info));
  }
}

void NintendoDataFetcher::DeviceRemoved(mojom::HidDeviceInfoPtr device_info) {
  if (NintendoController::IsNintendoController(device_info->vendor_id,
                                               device_info->product_id)) {
    RemoveDevice(device_info->guid);
  }
}

bool NintendoDataFetcher::AddDevice(mojom::HidDeviceInfoPtr device_info) {
  DCHECK(hid_manager_);
  RecordConnectedGamepad(device_info->vendor_id, device_info->product_id);
  int source_id = next_source_id_++;
  auto emplace_result = controllers_.emplace(
      source_id, NintendoController::Create(source_id, std::move(device_info),
                                            hid_manager_.get()));
  if (emplace_result.second) {
    auto& new_device = emplace_result.first->second;
    DCHECK(new_device);
    new_device->Open(base::BindOnce(&NintendoDataFetcher::OnDeviceReady,
                                    weak_factory_.GetWeakPtr(), source_id));
    return true;
  }
  return false;
}

bool NintendoDataFetcher::RemoveDevice(const std::string& guid) {
  for (auto& entry : controllers_) {
    auto& match_device = entry.second;
    if (match_device->HasGuid(guid)) {
      if (match_device->IsComposite()) {
        // Decompose the composite device and return the remaining subdevice to
        // |controllers_|.
        auto decomposed_devices = match_device->Decompose();
        match_device->Shutdown();
        controllers_.erase(entry.first);
        for (auto& device : decomposed_devices) {
          if (device->HasGuid(guid)) {
            device->Shutdown();
          } else {
            int source_id = device->GetSourceId();
            controllers_.emplace(source_id, std::move(device));
          }
        }
      } else {
        match_device->Shutdown();
        controllers_.erase(entry.first);
      }
      return true;
    }
  }
  return false;
}

std::unique_ptr<NintendoController>
NintendoDataFetcher::ExtractAssociatedDevice(const NintendoController* device) {
  DCHECK(device);
  std::unique_ptr<NintendoController> associated_device;
  GamepadHand device_hand = device->GetGamepadHand();
  if (device_hand != GamepadHand::kNone) {
    GamepadHand target_hand = (device_hand == GamepadHand::kLeft)
                                  ? GamepadHand::kRight
                                  : GamepadHand::kLeft;
    for (auto& entry : controllers_) {
      auto& match_device = entry.second;
      if (match_device->IsOpen() &&
          match_device->GetGamepadHand() == target_hand &&
          match_device->GetBusType() == device->GetBusType()) {
        associated_device = std::move(match_device);
        controllers_.erase(entry.first);
        break;
      }
    }
  }

  // Set the PadState source back to the default to signal that the slot
  // occupied by the associated device is no longer in use.
  if (associated_device) {
    PadState* state = GetPadState(associated_device->GetSourceId());
    if (state)
      state->source = GAMEPAD_SOURCE_NONE;
  }

  return associated_device;
}

void NintendoDataFetcher::GetGamepadData(bool) {
  for (auto& entry : controllers_) {
    auto& device = entry.second;
    if (device->IsOpen() && device->IsUsable()) {
      PadState* state = GetPadState(device->GetSourceId());
      if (!state)
        continue;

      if (!state->is_initialized) {
        state->mapper = device->GetMappingFunction();
        device->InitializeGamepadState(state->mapper != nullptr, state->data);
        state->is_initialized = true;
      }
      device->UpdateGamepadState(state->data);
    }
  }
}

NintendoController* NintendoDataFetcher::GetControllerFromGuid(
    const std::string& guid) {
  for (auto& entry : controllers_) {
    auto& device = entry.second;
    if (device->HasGuid(guid))
      return device.get();
  }
  return nullptr;
}

NintendoController* NintendoDataFetcher::GetControllerFromSourceId(
    int source_id) {
  auto find_it = controllers_.find(source_id);
  return find_it == controllers_.end() ? nullptr : find_it->second.get();
}

void NintendoDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto* controller = GetControllerFromSourceId(source_id);
  if (!controller || !controller->IsOpen()) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }
  controller->PlayEffect(type, std::move(params), std::move(callback),
                         std::move(callback_runner));
}

void NintendoDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto* controller = GetControllerFromSourceId(source_id);
  if (!controller || !controller->IsOpen()) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }
  controller->ResetVibration(std::move(callback), std::move(callback_runner));
}

}  // namespace device
