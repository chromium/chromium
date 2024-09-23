// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/xinput_data_fetcher_win.h"

#include <stddef.h>
#include <string.h>

#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace device {

namespace {

// See http://goo.gl/5VSJR. These are not available in all versions of the
// header, but they can be returned from the driver, so we define our own
// versions here.
static const BYTE kDeviceSubTypeGamepad = 1;
static const BYTE kDeviceSubTypeWheel = 2;
static const BYTE kDeviceSubTypeArcadeStick = 3;
static const BYTE kDeviceSubTypeFlightStick = 4;
static const BYTE kDeviceSubTypeDancePad = 5;
static const BYTE kDeviceSubTypeGuitar = 6;
static const BYTE kDeviceSubTypeGuitarAlternate = 7;
static const BYTE kDeviceSubTypeDrumKit = 8;
static const BYTE kDeviceSubTypeGuitarBass = 11;
static const BYTE kDeviceSubTypeArcadePad = 19;

// XInput does not expose the state of the Guide (Xbox) button through the
// XInputGetState method. To access this button, we need to query the gamepad
// state with the undocumented XInputGetStateEx method.
static const LPCSTR kXInputGetStateExOrdinal = (LPCSTR)100;

// Bitmask for the Guide button in XInputGamepadEx.wButtons.
static const int kXInputGamepadGuide = 0x0400;

constexpr base::FilePath::CharType kXInputDllFileName[] =
    FILE_PATH_LITERAL("xinput1_4.dll");

float NormalizeXInputAxis(SHORT value) {
  return ((value + 32768.f) / 32767.5f) - 1.f;
}

}  // namespace

XInputDataFetcherWin::XInputDataFetcherWin() : xinput_available_(false) {}

XInputDataFetcherWin::~XInputDataFetcherWin() {
  for (auto& haptic_gamepad : haptics_) {
    if (haptic_gamepad)
      haptic_gamepad->Shutdown();
  }
}

GamepadSource XInputDataFetcherWin::source() {
  return Factory::static_source();
}

void XInputDataFetcherWin::OnAddedToProvider() {
  xinput_dll_ = base::ScopedNativeLibrary(base::FilePath(kXInputDllFileName));
  xinput_available_ = GetXInputDllFunctions();
}

void XInputDataFetcherWin::EnumerateDevices() {
  TRACE_EVENT0("GAMEPAD", "EnumerateDevices");

  if (xinput_available_) {
    for (size_t i = 0; i < XUSER_MAX_COUNT; ++i) {
      // Check to see if the xinput device is connected
      XINPUT_CAPABILITIES caps;
      DWORD res = xinput_get_capabilities_(i, XINPUT_FLAG_GAMEPAD, &caps);
      xinput_connected_[i] = (res == ERROR_SUCCESS);
      if (!xinput_connected_[i]) {
        if (haptics_[i])
          haptics_[i]->Shutdown();
        haptics_[i] = nullptr;
        continue;
      }

      PadState* state = GetPadState(i);
      if (!state)
        continue;  // No slot available for this gamepad.

      Gamepad& pad = state->data;

      if (!state->is_initialized) {
        state->is_initialized = true;
        if (!haptics_[i]) {
          haptics_[i] =
              std::make_unique<XInputHapticGamepadWin>(i, xinput_set_state_);
        }

        // This is the first time we've seen this device, so do some one-time
        // initialization
        pad.connected = true;

        pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
        pad.vibration_actuator.not_null = true;

        const auto name = [](BYTE sub_type) -> std::u16string_view {
          static constexpr auto kNames =
              base::MakeFixedFlatMap<BYTE, std::u16string_view>({
                  {kDeviceSubTypeGamepad, u"GAMEPAD"},
                  {kDeviceSubTypeWheel, u"WHEEL"},
                  {kDeviceSubTypeArcadeStick, u"ARCADE_STICK"},
                  {kDeviceSubTypeFlightStick, u"FLIGHT_STICK"},
                  {kDeviceSubTypeDancePad, u"DANCE_PAD"},
                  {kDeviceSubTypeGuitar, u"GUITAR"},
                  {kDeviceSubTypeGuitarAlternate, u"GUITAR_ALTERNATE"},
                  {kDeviceSubTypeDrumKit, u"DRUM_KIT"},
                  {kDeviceSubTypeGuitarBass, u"GUITAR_BASS"},
                  {kDeviceSubTypeArcadePad, u"ARCADE_PAD"},
              });
          const auto it = kNames.find(sub_type);
          return (it == kNames.end()) ? u"<UNKNOWN>" : it->second;
        }(caps.SubType);
        pad.SetID(base::StrCat(
            {u"Xbox 360 Controller (XInput STANDARD ", name, u")"}));
        pad.mapping = GamepadMapping::kStandard;
      }
    }
  }
}

void XInputDataFetcherWin::GetGamepadData(bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  if (!xinput_available_)
    return;

  // A note on XInput devices:
  // If we got notification that system devices have been updated, then
  // run GetCapabilities to update the connected status and the device
  // identifier. It can be slow to do to both GetCapabilities and
  // GetState on unconnected devices, so we want to avoid a 2-5ms pause
  // here by only doing this when the devices are updated (despite
  // documentation claiming it's OK to call it any time).
  if (devices_changed_hint)
    EnumerateDevices();

  for (size_t i = 0; i < XUSER_MAX_COUNT; ++i) {
    if (xinput_connected_[i])
      GetXInputPadData(i);
  }
}

void XInputDataFetcherWin::GetXInputPadData(int i) {
  PadState* pad_state = GetPadState(i);
  if (!pad_state)
    return;

  Gamepad& pad = pad_state->data;

  // Use XInputGetStateEx if it is available, otherwise fall back to
  // XInputGetState. We can use the same struct for both since XInputStateEx
  // has identical layout to XINPUT_STATE except for an extra padding member at
  // the end.
  XInputStateEx state;
  memset(&state, 0, sizeof(XInputStateEx));
  TRACE_EVENT_BEGIN1("GAMEPAD", "XInputGetState", "id", i);
  DWORD dwResult;
  if (xinput_get_state_ex_)
    dwResult = xinput_get_state_ex_(i, &state);
  else
    dwResult = xinput_get_state_(i, reinterpret_cast<XINPUT_STATE*>(&state));
  TRACE_EVENT_END1("GAMEPAD", "XInputGetState", "id", i);

  if (dwResult == ERROR_SUCCESS) {
    pad.timestamp = CurrentTimeInMicroseconds();
    pad.buttons_length = 0;
    WORD val = state.Gamepad.wButtons;
#define ADD(b)                                                \
  pad.buttons[pad.buttons_length].pressed = (val & (b)) != 0; \
  pad.buttons[pad.buttons_length++].value = ((val & (b)) ? 1.f : 0.f);
    ADD(XINPUT_GAMEPAD_A);
    ADD(XINPUT_GAMEPAD_B);
    ADD(XINPUT_GAMEPAD_X);
    ADD(XINPUT_GAMEPAD_Y);
    ADD(XINPUT_GAMEPAD_LEFT_SHOULDER);
    ADD(XINPUT_GAMEPAD_RIGHT_SHOULDER);

    pad.buttons[pad.buttons_length].pressed =
        state.Gamepad.bLeftTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    pad.buttons[pad.buttons_length++].value =
        state.Gamepad.bLeftTrigger / 255.f;

    pad.buttons[pad.buttons_length].pressed =
        state.Gamepad.bRightTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    pad.buttons[pad.buttons_length++].value =
        state.Gamepad.bRightTrigger / 255.f;

    ADD(XINPUT_GAMEPAD_BACK);
    ADD(XINPUT_GAMEPAD_START);
    ADD(XINPUT_GAMEPAD_LEFT_THUMB);
    ADD(XINPUT_GAMEPAD_RIGHT_THUMB);
    ADD(XINPUT_GAMEPAD_DPAD_UP);
    ADD(XINPUT_GAMEPAD_DPAD_DOWN);
    ADD(XINPUT_GAMEPAD_DPAD_LEFT);
    ADD(XINPUT_GAMEPAD_DPAD_RIGHT);
    if (xinput_get_state_ex_) {
      // Only XInputGetStateEx reports the Guide button state.
      ADD(kXInputGamepadGuide);
    }
#undef ADD
    pad.axes_length = 0;

    float value = 0.0;
#define ADD(a, factor)                     \
  value = factor * NormalizeXInputAxis(a); \
  pad.axes[pad.axes_length++] = value;

    // XInput are +up/+right, -down/-left, we want -up/-left.
    ADD(state.Gamepad.sThumbLX, 1);
    ADD(state.Gamepad.sThumbLY, -1);
    ADD(state.Gamepad.sThumbRX, 1);
    ADD(state.Gamepad.sThumbRY, -1);
#undef ADD
  }
}

void XInputDataFetcherWin::PlayEffect(
    int pad_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (pad_id < 0 || pad_id >= XUSER_MAX_COUNT) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  if (!xinput_available_ || !xinput_connected_[pad_id] ||
      haptics_[pad_id] == nullptr) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  haptics_[pad_id]->PlayEffect(type, std::move(params), std::move(callback),
                               std::move(callback_runner));
}

void XInputDataFetcherWin::ResetVibration(
    int pad_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (pad_id < 0 || pad_id >= XUSER_MAX_COUNT) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  if (!xinput_available_ || !xinput_connected_[pad_id] ||
      haptics_[pad_id] == nullptr) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  haptics_[pad_id]->ResetVibration(std::move(callback),
                                   std::move(callback_runner));
}

bool XInputDataFetcherWin::GetXInputDllFunctions() {
  xinput_get_capabilities_ = nullptr;
  xinput_get_state_ = nullptr;
  xinput_get_state_ex_ = nullptr;
  xinput_set_state_ = nullptr;
  xinput_get_capabilities_ = reinterpret_cast<XInputGetCapabilitiesFunc>(
      xinput_dll_.GetFunctionPointer("XInputGetCapabilities"));
  if (!xinput_get_capabilities_)
    return false;

  // Get undocumented function XInputGetStateEx. If it is not present, fall back
  // to XInputGetState.
  xinput_get_state_ex_ = reinterpret_cast<XInputGetStateExFunc>(
      ::GetProcAddress(xinput_dll_.get(), kXInputGetStateExOrdinal));
  if (!xinput_get_state_ex_) {
    xinput_get_state_ = reinterpret_cast<XInputGetStateFunc>(
        xinput_dll_.GetFunctionPointer("XInputGetState"));
  }

  if (!xinput_get_state_ && !xinput_get_state_ex_)
    return false;
  xinput_set_state_ =
      reinterpret_cast<XInputHapticGamepadWin::XInputSetStateFunc>(
          xinput_dll_.GetFunctionPointer("XInputSetState"));
  return !!xinput_set_state_;
}

// static
void XInputDataFetcherWin::OverrideXInputGetCapabilitiesFuncForTesting(
    XInputDataFetcherWin::XInputGetCapabilitiesFunctionCallback callback) {
  GetXInputGetCapabilitiesFunctionCallback() = callback;
}

// static
XInputDataFetcherWin::XInputGetCapabilitiesFunctionCallback&
XInputDataFetcherWin::GetXInputGetCapabilitiesFunctionCallback() {
  static base::NoDestructor<
      XInputDataFetcherWin::XInputGetCapabilitiesFunctionCallback>
      instance;
  return *instance;
}

// static
void XInputDataFetcherWin::OverrideXInputGetStateExFuncForTesting(
    XInputDataFetcherWin::XInputGetStateExFunctionCallback callback) {
  GetXInputGetStateExFunctionCallback() = callback;
}

// static
XInputDataFetcherWin::XInputGetStateExFunctionCallback&
XInputDataFetcherWin::GetXInputGetStateExFunctionCallback() {
  static base::NoDestructor<
      XInputDataFetcherWin::XInputGetStateExFunctionCallback>
      instance;
  return *instance;
}

bool XInputDataFetcherWin::GetXInputDllFunctionsForWgiDataFetcher() {
  xinput_get_capabilities_ = nullptr;
  if (GetXInputGetCapabilitiesFunctionCallback()) {
    xinput_get_capabilities_ = GetXInputGetCapabilitiesFunctionCallback().Run();
  } else {
    xinput_get_capabilities_ = reinterpret_cast<XInputGetCapabilitiesFunc>(
        xinput_dll_.GetFunctionPointer("XInputGetCapabilities"));
  }
  if (!xinput_get_capabilities_)
    return false;

  // Get the undocumented XInputGetStateEx, which will allow access to the Guide
  // button's state.
  xinput_get_state_ex_ = nullptr;
  if (GetXInputGetStateExFunctionCallback()) {
    xinput_get_state_ex_ = GetXInputGetStateExFunctionCallback().Run();
  } else {
    xinput_get_state_ex_ = reinterpret_cast<XInputGetStateExFunc>(
        ::GetProcAddress(xinput_dll_.get(), kXInputGetStateExOrdinal));
  }
  return !!xinput_get_state_ex_;
}

void XInputDataFetcherWin::InitializeForWgiDataFetcher() {
  xinput_dll_ = base::ScopedNativeLibrary(base::FilePath(kXInputDllFileName));
  xinput_available_ = GetXInputDllFunctionsForWgiDataFetcher();
}

bool XInputDataFetcherWin::IsAnyMetaButtonPressed() {
  if (!xinput_available_)
    return false;

  for (size_t i = 0; i < XUSER_MAX_COUNT; ++i) {
    // Check to see if the xinput device is connected.
    XINPUT_CAPABILITIES caps;
    DWORD res = xinput_get_capabilities_(i, XINPUT_FLAG_GAMEPAD, &caps);
    // No device connected at i-index.
    if (res != ERROR_SUCCESS)
      continue;

    XInputStateEx xinput_state;
    memset(&xinput_state, 0, sizeof(XInputStateEx));
    DWORD dwResult;
    dwResult = xinput_get_state_ex_(i, &xinput_state);

    if (dwResult != ERROR_SUCCESS)
      continue;

    // Check the nexus button state and report only the first press detected.
    WORD xinput_buttons_state = xinput_state.Gamepad.wButtons;
    if (xinput_buttons_state & kXInputGamepadGuide) {
      return true;
    }
  }
  return false;
}

}  // namespace device
