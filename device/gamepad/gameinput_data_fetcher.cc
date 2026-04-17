// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gameinput_data_fetcher.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/dualshock4_controller.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/nintendo_controller.h"
#include "device/gamepad/public/cpp/gamepad_features.h"

namespace device {

namespace {

constexpr uint64_t kGameInputUnregistrationTimeout = 5000;
constexpr std::string_view kDefaultDisplayName = "Unknown Gamepad";
constexpr std::u16string_view kKnownXInputDeviceId =
    u"Xbox 360 Controller (XInput STANDARD GAMEPAD)";

// Retrieves the GameInputCreate function from gameinput.dll.
GameInputDataFetcher::CreateGameInputFunction
GetNativeCreateGameInputFunction() {
  base::NativeLibrary gameinput_module =
      base::PinSystemLibrary(L"gameinput.dll");
  if (!gameinput_module) {
    return GameInputDataFetcher::CreateGameInputFunction();
  }

  auto* create_fn = reinterpret_cast<HRESULT(WINAPI*)(IGameInput**)>(
      base::GetFunctionPointerFromNativeLibrary(gameinput_module,
                                                "GameInputCreate"));
  if (!create_fn) {
    return GameInputDataFetcher::CreateGameInputFunction();
  }

  return base::BindRepeating(create_fn);
}

GameInputDataFetcher::CreateGameInputFunction& GetCreateGameInputFunction() {
  static base::NoDestructor<GameInputDataFetcher::CreateGameInputFunction>
      instance(GetNativeCreateGameInputFunction());
  return *instance;
}

// Check if the gamepad should be added by the gameinput provider. In the
// situation that a Nintendo or Dualshock4 gamepad is connected, there are
// dedicated data fetchers designed for these gamepads.
// We want to let those data fetchers handle the gamepad input instead.
bool ShouldEnumerateGamepad(const GameInputDeviceInfo* device_info) {
  uint16_t vendor_id = device_info->vendorId;
  uint16_t product_id = device_info->productId;
  GamepadId gamepad_id = GamepadIdList::Get().GetGamepadId(
      /*product_name=*/"", vendor_id, product_id);

  if (NintendoController::IsNintendoController(gamepad_id)) {
    // Nintendo devices are handled by the Nintendo data fetcher.
    return false;
  }

  if (Dualshock4Controller::IsDualshock4(gamepad_id)) {
    // Dualshock4 devices are handled by the RawInput data fetcher.
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kIgnorePS5GamepadsInWgi) &&
      GamepadIdList::IsPlayStation5Gamepad(gamepad_id)) {
    // PlayStation 5 gamepads are handled by the RawInput data fetcher.
    return false;
  }

  return true;
}

bool HasTriggerRumbleSupport(const GameInputDeviceInfo* device_info) {
  return (device_info->supportedRumbleMotors &
          GameInputRumbleMotors::GameInputRumbleLeftTrigger) ||
         (device_info->supportedRumbleMotors &
          GameInputRumbleMotors::GameInputRumbleRightTrigger);
}

}  // namespace

GameInputDataFetcher::GameInputDataFetcher() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GameInputDataFetcher::~GameInputDataFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [source_id, gamepad] : devices_) {
    if (gamepad) {
      gamepad->Shutdown();
    }
  }

  if (device_callback_token_ != GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE &&
      gameinput_ != nullptr) {
    bool unregister_succeeded = gameinput_->UnregisterCallback(
        device_callback_token_, kGameInputUnregistrationTimeout);
    DCHECK(unregister_succeeded);
  }

  if (guide_button_callback_token_ != GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE &&
      gameinput_ != nullptr) {
    bool unregister_succeeded = gameinput_->UnregisterCallback(
        guide_button_callback_token_, kGameInputUnregistrationTimeout);
    DCHECK(unregister_succeeded);
  }
}

GamepadSource GameInputDataFetcher::source() {
  return Factory::static_source();
}

void GameInputDataFetcher::OnAddedToProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateGameInputFunction& create_gameinput_function =
      GetCreateGameInputFunction();

  if (create_gameinput_function.is_null()) {
    initialization_state_ = InitializationState::kGetProcAddressFailed;
    return;
  }

  if (!gameinput_) {
    if (FAILED(create_gameinput_function.Run(&gameinput_))) {
      initialization_state_ = InitializationState::kCreateGameInputFailed;
      return;
    }
  }

  device_enumerated_callback_ = std::make_unique<DeviceEnumeratedCallback>(
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindRepeating(
                             &GameInputDataFetcher::OnDeviceEnumeratedSequenced,
                             weak_factory_.GetWeakPtr())));

  if (FAILED(gameinput_->RegisterDeviceCallback(
          /* IGameInputDevice */ nullptr,     // Don't filter to events from a
                                              // specific device
          GameInputKindGamepad,               // Gamepad devices only
          GameInputDeviceConnected,           // Connected/disconnected events
          GameInputAsyncEnumeration,          // Enumerate asynchronously
          device_enumerated_callback_.get(),  // class context
          OnDeviceEnumerated,                 // Callback function
          &device_callback_token_)))          // Generated token
  {
    initialization_state_ = InitializationState::kFailedDeviceEnumeration;
    return;
  }

  guide_button_changed_callback_ =
      std::make_unique<GuideButtonChangedCallback>(base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindRepeating(
              &GameInputDataFetcher::OnGuideButtonChangedSequenced,
              weak_factory_.GetWeakPtr())));

  if (FAILED(gameinput_->RegisterSystemButtonCallback(
          /* IGameInputDevice */ nullptr,
          GameInputSystemButtons::GameInputSystemButtonGuide,
          guide_button_changed_callback_.get(), OnGuideButtonChanged,
          &guide_button_callback_token_))) {
    initialization_state_ =
        InitializationState::kFailedGuideButtonCallbackRegistration;
    return;
  }

  initialization_state_ = InitializationState::kInitialized;
}

void GameInputDataFetcher::OnDeviceEnumerated(
    GameInputCallbackToken callbackToken,
    void* context,
    IGameInputDevice* device,
    uint64_t timestamp,
    GameInputDeviceStatus current_status,
    GameInputDeviceStatus previous_status) {
  DeviceEnumeratedCallback* callback =
      reinterpret_cast<DeviceEnumeratedCallback*>(context);
  callback->Run(device, current_status);
}

void GameInputDataFetcher::OnDeviceEnumeratedSequenced(
    Microsoft::WRL::ComPtr<IGameInputDevice> device,
    GameInputDeviceStatus current_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_status & GameInputDeviceConnected) {
    OnGamepadAdded(device.Get());
  } else {
    OnGamepadRemoved(device.Get());
  }
}

void GameInputDataFetcher::OnGamepadAdded(IGameInputDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GameInputDeviceInfo* device_info = device->GetDeviceInfo();
  if (!device_info) {
    return;
  }

  if (!ShouldEnumerateGamepad(device_info)) {
    return;
  }

  std::string product_identifier = GamepadIdList::GetProductIdentifier(
      device_info->vendorId, device_info->productId);
  int source_id = next_source_id_++;
  PadState* state =
      GetPadState(source_id, /*new_pad_recognized=*/true, product_identifier);
  if (!state) {
    return;
  }

  state->is_initialized = true;
  Gamepad& pad = state->data;
  pad.connected = true;

  XInputType xinput_type = GamepadIdList::Get().GetXInputType(
      device_info->vendorId, device_info->productId);
  // Given that this device might have been previously enumerated by the WGI
  // data fetcher, let's use the "Unknown Gamepad" product name, which is also
  // used by the WGI data fetcher when it fails to obtain the gamepad's
  // DisplayName.
  if (xinput_type == kXInputTypeNone) {
    UpdateGamepadStrings(std::string(kDefaultDisplayName), device_info->vendorId,
                         device_info->productId,
                         /*has_standard_mapping*/ true, pad);
  } else {
    pad.SetID(std::u16string(kKnownXInputDeviceId));
  }

  if (HasTriggerRumbleSupport(device_info)) {
    pad.vibration_actuator.type = GamepadHapticActuatorType::kTriggerRumble;
  } else {
    pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  }

  pad.vibration_actuator.not_null = true;
  pad.mapping = GamepadMapping::kStandard;
  devices_[source_id] = std::make_unique<GameInputGamepadDevice>(
      device, std::move(product_identifier));
}

void GameInputDataFetcher::OnGamepadRemoved(IGameInputDevice* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = std::find_if(devices_.begin(), devices_.end(),
                         [device](const auto& entry) {
                           return entry.second->GetGamepad().Get() == device;
                         });

  if (it != devices_.end()) {
    it->second->Shutdown();
    devices_.erase(it);
  }
}

void GameInputDataFetcher::OnGuideButtonChanged(
    GameInputCallbackToken callbackToken,
    void* context,
    IGameInputDevice* device,
    uint64_t timestamp,
    GameInputSystemButtons currentButtons,
    GameInputSystemButtons previousButtons) {
  reinterpret_cast<GuideButtonChangedCallback*>(context)->Run(device,
                                                              currentButtons);
}

void GameInputDataFetcher::OnGuideButtonChangedSequenced(
    Microsoft::WRL::ComPtr<IGameInputDevice> device,
    GameInputSystemButtons current_buttons) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [source_id, gamepad_device] : devices_) {
    if (gamepad_device->GetGamepad() != device) {
      continue;
    }

    PadState* state = GetPadState(source_id);
    if (!state) {
      return;
    }

    if (current_buttons & GameInputSystemButtons::GameInputSystemButtonGuide) {
      state->data.buttons[BUTTON_INDEX_META].pressed = true;
      state->data.buttons[BUTTON_INDEX_META].value = 1.f;
    } else {
      state->data.buttons[BUTTON_INDEX_META].pressed = false;
      state->data.buttons[BUTTON_INDEX_META].value = 0.f;
    }
    return;
  }
}

void GameInputDataFetcher::GetGamepadData(bool devices_changed_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& map_entry : devices_) {
    const auto& [source_id, gamepad_device] = map_entry;
    std::optional<std::string_view> product_identifier;
    if (base::FeatureList::IsEnabled(
            features::kClaimDuplicateGamepadsProductIdentifier)) {
      product_identifier = gamepad_device->GetProductIdentifier();
    }
    PadState* state =
        GetPadState(source_id, /*new_pad_recognized=*/true, product_identifier);
    if (!state) {
      continue;
    }

    Microsoft::WRL::ComPtr<IGameInputReading> reading;
    if (FAILED(gameinput_->GetCurrentReading(GameInputKindGamepad,
                                             gamepad_device->GetGamepad().Get(),
                                             &reading))) {
      continue;
    }

    GameInputGamepadState gamepad_state;
    bool get_state_succeeded = reading->GetGamepadState(&gamepad_state);
    DCHECK(get_state_succeeded);

    Gamepad& pad = state->data;
    pad.timestamp = CurrentTimeInMicroseconds();

    // TODO(crbug.com/502624503): Handle guide button properly when moving to
    // GameInput v3. We should check if the guide button is supported via
    // GameInputDeviceInfo::supportedSystemButtons.
    pad.buttons_length = BUTTON_INDEX_COUNT;

    static constexpr struct {
      int button_index;
      GameInputGamepadButtons button_mask;
    } kButtonMappings[] = {
        {BUTTON_INDEX_PRIMARY, GameInputGamepadA},
        {BUTTON_INDEX_SECONDARY, GameInputGamepadB},
        {BUTTON_INDEX_TERTIARY, GameInputGamepadX},
        {BUTTON_INDEX_QUATERNARY, GameInputGamepadY},
        {BUTTON_INDEX_LEFT_SHOULDER, GameInputGamepadLeftShoulder},
        {BUTTON_INDEX_RIGHT_SHOULDER, GameInputGamepadRightShoulder},
        {BUTTON_INDEX_BACK_SELECT, GameInputGamepadView},
        {BUTTON_INDEX_START, GameInputGamepadMenu},
        {BUTTON_INDEX_LEFT_THUMBSTICK, GameInputGamepadLeftThumbstick},
        {BUTTON_INDEX_RIGHT_THUMBSTICK, GameInputGamepadRightThumbstick},
        {BUTTON_INDEX_DPAD_UP, GameInputGamepadDPadUp},
        {BUTTON_INDEX_DPAD_DOWN, GameInputGamepadDPadDown},
        {BUTTON_INDEX_DPAD_LEFT, GameInputGamepadDPadLeft},
        {BUTTON_INDEX_DPAD_RIGHT, GameInputGamepadDPadRight},
    };

    base::span<GamepadButton> pad_buttons(pad.buttons);
    for (const auto& button : kButtonMappings) {
      if (gamepad_state.buttons & button.button_mask) {
        pad_buttons[button.button_index].pressed = true;
        pad_buttons[button.button_index].value = 1.0f;
        pad_buttons[button.button_index].touched = true;
      } else {
        pad_buttons[button.button_index].pressed = false;
        pad_buttons[button.button_index].value = 0.0f;
        pad_buttons[button.button_index].touched = false;
      }
    }

    // TODO(crbug.com/502624503): When moving to GameInput v3, access trigger
    // buttons using
    // GameInputGamepadButtons::GameInputGamepad(Left|Right)TriggerButton.
    pad_buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed =
        gamepad_state.leftTrigger >
        GamepadButton::kDefaultButtonPressedThreshold;
    pad_buttons[BUTTON_INDEX_LEFT_TRIGGER].value = gamepad_state.leftTrigger;
    pad_buttons[BUTTON_INDEX_LEFT_TRIGGER].touched =
        gamepad_state.leftTrigger > 0.0f;

    pad_buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed =
        gamepad_state.rightTrigger >
        GamepadButton::kDefaultButtonPressedThreshold;
    pad_buttons[BUTTON_INDEX_RIGHT_TRIGGER].value = gamepad_state.rightTrigger;
    pad_buttons[BUTTON_INDEX_RIGHT_TRIGGER].touched =
        gamepad_state.rightTrigger > 0.0f;

    pad.axes_length = AXIS_INDEX_COUNT;

    // Invert the Y thumbstick axes to match the Standard Gamepad. GameInput
    // thumbstick axes use +up/+right but the Standard Gamepad uses
    // +down/+right.
    pad.axes[AXIS_INDEX_LEFT_STICK_X] = gamepad_state.leftThumbstickX;
    pad.axes[AXIS_INDEX_LEFT_STICK_Y] = gamepad_state.leftThumbstickY * -1.0f;
    pad.axes[AXIS_INDEX_RIGHT_STICK_X] = gamepad_state.rightThumbstickX;
    pad.axes[AXIS_INDEX_RIGHT_STICK_Y] = gamepad_state.rightThumbstickY * -1.0f;
  }
}

void GameInputDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto map_entry = devices_.find(source_id);
  if (map_entry == devices_.end()) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  map_entry->second->PlayEffect(type, std::move(params), std::move(callback),
                                std::move(callback_runner));
}

void GameInputDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto map_entry = devices_.find(source_id);
  if (map_entry == devices_.end()) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  map_entry->second->ResetVibration(std::move(callback),
                                    std::move(callback_runner));
}

GameInputDataFetcher::InitializationState
GameInputDataFetcher::GetInitializationState() const {
  return initialization_state_;
}

void GameInputDataFetcher::OverrideGameInputCreationMethodForTesting(
    CreateGameInputFunction create_override) {
  GetCreateGameInputFunction() = std::move(create_override);
}

}  // namespace device
