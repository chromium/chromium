// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_data_fetcher_win.h"

#include <stdint.h>
#include <wrl/event.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/windows_version.h"
#include "device/base/event_utils_winrt.h"
#include "device/gamepad/dualshock4_controller.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/nintendo_controller.h"
#include "device/gamepad/wgi_gamepad_device.h"

namespace device {

namespace {

Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameController>
GetRawGameController(ABI::Windows::Gaming::Input::IGamepad* gamepad,
                     WgiDataFetcherWin::GetActivationFactoryFunction
                         get_activation_factory_function_) {
  base::win::ScopedHString raw_game_controller_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Gaming_Input_RawGameController);
  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameControllerStatics>
      raw_game_controller_statics;
  HRESULT hr = get_activation_factory_function_(
      raw_game_controller_string.get(),
      IID_PPV_ARGS(&raw_game_controller_statics));
  if (FAILED(hr))
    return nullptr;

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGameController>
      game_controller;
  hr = gamepad->QueryInterface(IID_PPV_ARGS(&game_controller));
  if (FAILED(hr))
    return nullptr;

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameController>
      raw_game_controller;
  hr = raw_game_controller_statics->FromGameController(game_controller.Get(),
                                                       &raw_game_controller);
  if (FAILED(hr))
    return nullptr;

  return raw_game_controller;
}

// Check if the gamepad should be added by Windows.Gaming.Input. In the
// situation that a Nintendo or Dualshock4 gamepad is connected, there are
// dedicated data fetchers designed for these gamepads.
// We want to let those data fetchers handle the gamepad input instead.
bool ShouldEnumerateGamepad(const std::u16string& product_name,
                            ABI::Windows::Gaming::Input::IGamepad* gamepad,
                            WgiDataFetcherWin::GetActivationFactoryFunction
                                get_activation_factory_function) {
  std::string product_name_string = base::UTF16ToUTF8(product_name);
  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameController>
      raw_game_controller =
          GetRawGameController(gamepad, get_activation_factory_function);
  if (!raw_game_controller) {
    return false;
  }

  uint16_t vendor_id;
  hr = raw_game_controller->get_HardwareVendorId(&vendor_id);
  if (FAILED(hr)) {
    return false;
  }

  uint16_t product_id;
  hr = raw_game_controller->get_HardwareProductId(&product_id);
  if (FAILED(hr)) {
    return false;
  }

  GamepadId gamepad_id = GamepadIdList::Get().GetGamepadId(
      product_name_string, vendor_id, product_id);
  if (NintendoController::IsNintendoController(gamepad_id)) {
    // Nintendo devices are handled by the Nintendo data fetcher.
    return false;
  }

  if (Dualshock4Controller::IsDualshock4(gamepad_id)) {
    // Dualshock4 devices are handled by the RawInput data fetcher.
    return false;
  }

  return true;
}

// Checks if the provided gamepad has paddles and returns the available
// quantity. If the Windows version is less then WIN10_RS1 (WIN10.0.14393.0),
// this function returns 0, since IGamepad2 interface will not be available.
uint32_t GetPaddleNumber(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
        gamepad) {
  uint32_t num_paddles = 0;
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    return num_paddles;
  }

  static constexpr ABI::Windows::Gaming::Input::GamepadButtons kPaddles[] = {
      ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_Paddle1,
      ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_Paddle2,
      ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_Paddle3,
      ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_Paddle4};
  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad2> gamepad2;
  HRESULT hr = gamepad->QueryInterface(IID_PPV_ARGS(&gamepad2));
  if (hr == S_OK) {
    ABI::Windows::Gaming::Input::GameControllerButtonLabel button_label;
    for (const auto& paddle : kPaddles) {
      hr = gamepad2->GetButtonLabel(paddle, &button_label);
      if (hr == S_OK &&
          button_label !=
              ABI::Windows::Gaming::Input::GameControllerButtonLabel::
                  GameControllerButtonLabel_None) {
        ++num_paddles;
      }
    }
  }
  return num_paddles;
}

}  // namespace

WgiDataFetcherWin::WgiDataFetcherWin() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // If a callback has been overridden previously, we'll give preference to it.
  if (GetActivationFactoryFunctionCallback()) {
    get_activation_factory_function_ =
        GetActivationFactoryFunctionCallback().Run();
  } else {
    get_activation_factory_function_ = &base::win::RoGetActivationFactory;
  }
}

WgiDataFetcherWin::~WgiDataFetcherWin() {
  UnregisterEventHandlers();
  for (auto& map_entry : devices_) {
    if (map_entry.second) {
      map_entry.second->Shutdown();
    }
  }
}

GamepadSource WgiDataFetcherWin::source() {
  return Factory::static_source();
}

void WgiDataFetcherWin::OnAddedToProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::win::HStringReference::ResolveCoreWinRTStringDelayload()) {
    initialization_state_ =
        InitializationState::kCoreWinrtStringDelayLoadFailed;
    return;
  }

  HRESULT hr = get_activation_factory_function_(
      base::win::HStringReference(RuntimeClass_Windows_Gaming_Input_Gamepad)
          .Get(),
      IID_PPV_ARGS(&gamepad_statics_));
  if (FAILED(hr)) {
    initialization_state_ = InitializationState::kRoGetActivationFactoryFailed;
    return;
  }

  // Create a Windows::Foundation::IEventHandler that runs a
  // base::RepeatingCallback() on the gamepad polling thread when a gamepad
  // is added or removed. This callback stores the current sequence task runner
  // and weak pointer, so those two objects would remain active until the
  // callback returns.
  added_event_token_ = AddEventHandler(
      gamepad_statics_.Get(),
      &ABI::Windows::Gaming::Input::IGamepadStatics::add_GamepadAdded,
      base::BindRepeating(&WgiDataFetcherWin::OnGamepadAdded,
                          weak_factory_.GetWeakPtr()));
  if (!added_event_token_) {
    initialization_state_ = InitializationState::kAddGamepadAddedFailed;
    UnregisterEventHandlers();
    return;
  }

  removed_event_token_ = AddEventHandler(
      gamepad_statics_.Get(),
      &ABI::Windows::Gaming::Input::IGamepadStatics::add_GamepadRemoved,
      base::BindRepeating(&WgiDataFetcherWin::OnGamepadRemoved,
                          weak_factory_.GetWeakPtr()));
  if (!removed_event_token_) {
    initialization_state_ = InitializationState::kAddGamepadRemovedFailed;
    UnregisterEventHandlers();
    return;
  }

  initialization_state_ = InitializationState::kInitialized;
}

void WgiDataFetcherWin::OnGamepadAdded(
    IInspectable* /* sender */,
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  // While base::win::AddEventHandler stores the sequence_task_runner in the
  // callback function object, it post the task back to the same sequence - the
  // gamepad polling thread when the callback is returned on a different thread
  // from the IGamepadStatics COM API. Thus `OnGamepadAdded` is also running on
  // gamepad polling thread, it is the only thread that is able to access the
  // `devices_` object, making it thread-safe.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialization_state_ != InitializationState::kInitialized)
    return;

  const std::u16string display_name = GetGamepadDisplayName(gamepad);
  if (!ShouldEnumerateGamepad(display_name, gamepad,
                              get_activation_factory_function_)) {
    return;
  }

  int source_id = next_source_id_++;
  PadState* state = GetPadState(source_id);
  if (!state)
    return;
  state->is_initialized = true;
  Gamepad& pad = state->data;
  pad.SetID(display_name);
  pad.connected = true;
  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = true;
  pad.mapping = GamepadMapping::kStandard;
  devices_[source_id] = std::make_unique<WgiGamepadDevice>(gamepad);
}

void WgiDataFetcherWin::OnGamepadRemoved(
    IInspectable* /* sender */,
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  // While ::device::AddEventHandler stores the sequence_task_runner in the
  // callback function object, it post the task back to the same sequence - the
  // gamepad polling thread when the callback is returned on a different thread
  // from the IGamepadStatics COM API. Thus `OnGamepadRemoved` is also running
  // on gamepad polling thread, it is the only thread that is able to access the
  // `devices_` object, making it thread-safe.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(initialization_state_, InitializationState::kInitialized);

  base::EraseIf(devices_, [=](const auto& map_entry) {
    if (map_entry.second->GetGamepad().Get() == gamepad) {
      map_entry.second->Shutdown();
      return true;
    }
    return false;
  });
}

void WgiDataFetcherWin::GetGamepadData(bool devices_changed_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& map_entry : devices_) {
    PadState* state = GetPadState(map_entry.first);
    if (!state)
      continue;

    ABI::Windows::Gaming::Input::GamepadReading reading;
    Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad =
        map_entry.second->GetGamepad();
    if (FAILED(gamepad->GetCurrentReading(&reading)))
      continue;

    Gamepad& pad = state->data;
    pad.timestamp = CurrentTimeInMicroseconds();
    const uint32_t num_paddles = GetPaddleNumber(gamepad);
    if (num_paddles == 0) {
      pad.buttons_length = BUTTON_INDEX_COUNT - 1;  // No meta.
    } else {
      pad.buttons_length = BUTTON_INDEX_COUNT + num_paddles;
    }

    static constexpr struct {
      int button_index;
      ABI::Windows::Gaming::Input::GamepadButtons wgi_button_mask;
    } kButtonMappings[] = {
        {BUTTON_INDEX_PRIMARY, ABI::Windows::Gaming::Input::GamepadButtons_A},
        {BUTTON_INDEX_SECONDARY, ABI::Windows::Gaming::Input::GamepadButtons_B},
        {BUTTON_INDEX_TERTIARY, ABI::Windows::Gaming::Input::GamepadButtons_X},
        {BUTTON_INDEX_QUATERNARY,
         ABI::Windows::Gaming::Input::GamepadButtons_Y},
        {BUTTON_INDEX_LEFT_SHOULDER,
         ABI::Windows::Gaming::Input::GamepadButtons_LeftShoulder},
        {BUTTON_INDEX_RIGHT_SHOULDER,
         ABI::Windows::Gaming::Input::GamepadButtons_RightShoulder},
        {BUTTON_INDEX_BACK_SELECT,
         ABI::Windows::Gaming::Input::GamepadButtons_View},
        {BUTTON_INDEX_START, ABI::Windows::Gaming::Input::GamepadButtons_Menu},
        {BUTTON_INDEX_LEFT_THUMBSTICK,
         ABI::Windows::Gaming::Input::GamepadButtons_LeftThumbstick},
        {BUTTON_INDEX_RIGHT_THUMBSTICK,
         ABI::Windows::Gaming::Input::GamepadButtons_RightThumbstick},
        {BUTTON_INDEX_DPAD_UP,
         ABI::Windows::Gaming::Input::GamepadButtons_DPadUp},
        {BUTTON_INDEX_DPAD_DOWN,
         ABI::Windows::Gaming::Input::GamepadButtons_DPadDown},
        {BUTTON_INDEX_DPAD_LEFT,
         ABI::Windows::Gaming::Input::GamepadButtons_DPadLeft},
        {BUTTON_INDEX_DPAD_RIGHT,
         ABI::Windows::Gaming::Input::GamepadButtons_DPadRight},
        {BUTTON_INDEX_META + 1,
         ABI::Windows::Gaming::Input::GamepadButtons_Paddle1},
        {BUTTON_INDEX_META + 2,
         ABI::Windows::Gaming::Input::GamepadButtons_Paddle2},
        {BUTTON_INDEX_META + 3,
         ABI::Windows::Gaming::Input::GamepadButtons_Paddle3},
        {BUTTON_INDEX_META + 4,
         ABI::Windows::Gaming::Input::GamepadButtons_Paddle4},
    };

    for (const auto& button : kButtonMappings) {
      if (reading.Buttons & button.wgi_button_mask) {
        pad.buttons[button.button_index].pressed = true;
        pad.buttons[button.button_index].value = 1.0f;
      } else {
        pad.buttons[button.button_index].pressed = false;
        pad.buttons[button.button_index].value = 0.0f;
      }
    }

    pad.buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed =
        reading.LeftTrigger > GamepadButton::kDefaultButtonPressedThreshold;
    pad.buttons[BUTTON_INDEX_LEFT_TRIGGER].value = reading.LeftTrigger;

    pad.buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed =
        reading.RightTrigger > GamepadButton::kDefaultButtonPressedThreshold;
    pad.buttons[BUTTON_INDEX_RIGHT_TRIGGER].value = reading.RightTrigger;

    pad.axes_length = AXIS_INDEX_COUNT;

    // Invert the Y thumbstick axes to match the Standard Gamepad. WGI
    // thumbstick axes use +up/+right but the Standard Gamepad uses
    // +down/+right.
    pad.axes[AXIS_INDEX_LEFT_STICK_X] = reading.LeftThumbstickX;
    pad.axes[AXIS_INDEX_LEFT_STICK_Y] = reading.LeftThumbstickY * -1.0f;
    pad.axes[AXIS_INDEX_RIGHT_STICK_X] = reading.RightThumbstickX;
    pad.axes[AXIS_INDEX_RIGHT_STICK_Y] = reading.RightThumbstickY * -1.0f;
  }
}

void WgiDataFetcherWin::PlayEffect(
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

void WgiDataFetcherWin::ResetVibration(
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

// static
void WgiDataFetcherWin::OverrideActivationFactoryFunctionForTesting(
    WgiDataFetcherWin::ActivationFactoryFunctionCallback callback) {
  GetActivationFactoryFunctionCallback() = callback;
}

// static
WgiDataFetcherWin::ActivationFactoryFunctionCallback&
WgiDataFetcherWin::GetActivationFactoryFunctionCallback() {
  static base::NoDestructor<
      WgiDataFetcherWin::ActivationFactoryFunctionCallback>
      instance;
  return *instance;
}

WgiDataFetcherWin::InitializationState
WgiDataFetcherWin::GetInitializationState() const {
  return initialization_state_;
}

std::u16string WgiDataFetcherWin::GetGamepadDisplayName(
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  static constexpr char16_t kDefaultDisplayName[] = u"Unknown Gamepad";

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameController>
      raw_game_controller = ::device::GetRawGameController(
          gamepad, get_activation_factory_function_);

  if (!raw_game_controller) {
    return kDefaultDisplayName;
  }

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameController2>
      raw_game_controller2;
  if (FAILED(raw_game_controller.As(&raw_game_controller2))) {
    return kDefaultDisplayName;
  }

  HSTRING display_name;
  if (FAILED(raw_game_controller2->get_DisplayName(&display_name))) {
    return kDefaultDisplayName;
  }
  base::win::ScopedHString scoped_display_name(display_name);
  return base::AsString16(scoped_display_name.Get());
}

void WgiDataFetcherWin::UnregisterEventHandlers() {
  if (added_event_token_) {
    HRESULT hr =
        gamepad_statics_->remove_GamepadAdded(added_event_token_.value());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Removing GamepadAdded Handler failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }

  if (removed_event_token_) {
    HRESULT hr =
        gamepad_statics_->remove_GamepadRemoved(removed_event_token_.value());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Removing GamepadRemoved Handler failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }
}

}  // namespace device
