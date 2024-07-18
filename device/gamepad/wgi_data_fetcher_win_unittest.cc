// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/wgi_data_fetcher_win.h"

#include <Windows.Gaming.Input.h>
#include <XInput.h>
#include <winerror.h>

#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_hstring.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "device/gamepad/test_support/fake_igamepad.h"
#include "device/gamepad/test_support/fake_igamepad_statics.h"
#include "device/gamepad/test_support/fake_winrt_wgi_environment.h"
#include "device/gamepad/test_support/wgi_test_error_code.h"
#include "device/gamepad/wgi_gamepad_device.h"
#include "services/device/device_service_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::ABI::Windows::Gaming::Input::GamepadReading;

constexpr uint16_t kHardwareVendorId = 0x045e;
constexpr uint16_t kHardwareProductId = 0x028e;
constexpr uint16_t kUnknownHardwareVendorId = 0x0000;
constexpr uint16_t kUnknownHardwareProductId = 0x0000;
constexpr uint16_t kTriggerRumbleHardwareProductId = 0x0b13;

constexpr char kGamepadDisplayName[] = "XBOX_SERIES_X";
constexpr char16_t kKnownXInputDeviceId[] =
    u"Xbox 360 Controller (XInput STANDARD GAMEPAD)";
constexpr double kErrorTolerance = 1e-5;

constexpr unsigned int kGamepadButtonsLength = 17;
// 16 buttons + 1 meta button + 4 paddles = 21 buttons.
constexpr unsigned int kGamepadWithPaddlesButtonsLength = 21;
constexpr unsigned int kGamepadAxesLength = 4;

constexpr double kDurationMillis = 1.0;
constexpr double kZeroStartDelayMillis = 0.0;
constexpr double kStrongMagnitude = 1.0;  // 100% intensity.
constexpr double kWeakMagnitude = 0.5;    // 50% intensity.

constexpr GamepadId kGamepadsWithTriggerRumble[] = {
    GamepadId::kMicrosoftProduct02d1, GamepadId::kMicrosoftProduct02dd,
    GamepadId::kMicrosoftProduct02fd, GamepadId::kMicrosoftProduct0b20,
    GamepadId::kMicrosoftProduct02ea, GamepadId::kMicrosoftProduct02e0,
    GamepadId::kMicrosoftProduct0b06, GamepadId::kMicrosoftProduct0b12,
    GamepadId::kMicrosoftProduct0b13, GamepadId::kMicrosoftProduct02e3,
    GamepadId::kMicrosoftProduct0b00, GamepadId::kMicrosoftProduct0b05,
    GamepadId::kMicrosoftProduct0b22};

constexpr WgiTestErrorCode kErrors[] = {
    WgiTestErrorCode::kErrorWgiRawGameControllerActivateFailed,
    WgiTestErrorCode::kErrorWgiRawGameControllerFromGameControllerFailed,
    WgiTestErrorCode::kErrorWgiRawGameControllerGetHardwareProductIdFailed,
    WgiTestErrorCode::kErrorWgiRawGameControllerGetHardwareVendorIdFailed};

constexpr WgiTestErrorCode kXInputLoadErrors[] = {
    WgiTestErrorCode::kNullXInputGetCapabilitiesPointer,
    WgiTestErrorCode::kNullXInputGetStateExPointer};

// GamepadReading struct for a gamepad in resting position.
constexpr ABI::Windows::Gaming::Input::GamepadReading
    kZeroPositionGamepadReading = {
        .Timestamp = 1234,
        .Buttons =
            ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_None,
        .LeftTrigger = 0,
        .RightTrigger = 0,
        .LeftThumbstickX = 0,
        .LeftThumbstickY = 0.05,
        .RightThumbstickX = -0.08,
        .RightThumbstickY = 0};

// Sample GamepadReading struct for testing.
constexpr ABI::Windows::Gaming::Input::GamepadReading kGamepadReading = {
    .Timestamp = 1234,
    .Buttons =
        ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_A |
        ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_DPadDown |
        ABI::Windows::Gaming::Input::GamepadButtons::
            GamepadButtons_RightShoulder |
        ABI::Windows::Gaming::Input::GamepadButtons::
            GamepadButtons_LeftThumbstick |
        ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_Paddle1 |
        ABI::Windows::Gaming::Input::GamepadButtons::GamepadButtons_Paddle3,
    .LeftTrigger = 1.0,
    .RightTrigger = 0.6,
    .LeftThumbstickX = 0.2,
    .LeftThumbstickY = 1.0,
    .RightThumbstickX = -0.4,
    .RightThumbstickY = 0.6};

// Bitmask for the Guide button in XInputGamepadEx.wButtons.
constexpr int kXInputGamepadGuide = 0x0400;

// Simulate controllers connected in indexes 1 and 3 of the XInput API.
DWORD WINAPI MockXInputGetCapabilitiesFunc(DWORD dwUserIndex,
                                           DWORD dwFlags,
                                           XINPUT_CAPABILITIES* pCapabilities) {
  if (dwUserIndex == 1 || dwUserIndex == 3)
    return ERROR_SUCCESS;

  return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI MockXInputGetStateExFunc(DWORD dwUserIndex,
                                      XInputStateEx* pState) {
  // To prevent the GamepadPadStateProvider sanitization from happening, each
  // gamepad button should be polled at least once while in a resting position
  // (not pressed). Therefore, only after the first poll in a connected gamepad,
  // this function should start reporting the meta button presses for this
  // gamepad.
  static int has_polled_mask = 0;

  // Passing an index greater than XUSER_MAX_COUNT and a nullptr pState will
  // reset the static variable.
  if (dwUserIndex > XUSER_MAX_COUNT && !pState) {
    has_polled_mask = 0;
    return ERROR_DEVICE_NOT_CONNECTED;
  }

  if (dwUserIndex == 1 || dwUserIndex == 3) {
    if (has_polled_mask & 1 << dwUserIndex) {
      pState->Gamepad.wButtons = kXInputGamepadGuide;
    } else {
      has_polled_mask |= 1 << dwUserIndex;
    }
    return ERROR_SUCCESS;
  }
  return ERROR_DEVICE_NOT_CONNECTED;
}

class WgiDataFetcherWinTest : public DeviceServiceTestBase {
 public:
  WgiDataFetcherWinTest() = default;
  ~WgiDataFetcherWinTest() override = default;

  void SetUpXInputEnv(WgiTestErrorCode error_code) {
    // Resetting MockXInputGetStateExFunc static variable state.
    MockXInputGetStateExFunc(XUSER_MAX_COUNT + 1, nullptr);
    XInputDataFetcherWin::OverrideXInputGetCapabilitiesFuncForTesting(
        base::BindLambdaForTesting(
            []() { return &MockXInputGetCapabilitiesFunc; }));
    XInputDataFetcherWin::OverrideXInputGetStateExFuncForTesting(
        base::BindLambdaForTesting([]() { return &MockXInputGetStateExFunc; }));
    // The callbacks should return a nullptr for each point of failure.
    switch (error_code) {
      case WgiTestErrorCode::kNullXInputGetCapabilitiesPointer:
        XInputDataFetcherWin::OverrideXInputGetCapabilitiesFuncForTesting(
            base::BindLambdaForTesting([]() {
              return (XInputDataFetcherWin::XInputGetCapabilitiesFunc) nullptr;
            }));
        break;
      case WgiTestErrorCode::kNullXInputGetStateExPointer:
        XInputDataFetcherWin::OverrideXInputGetStateExFuncForTesting(
            base::BindLambdaForTesting([]() {
              return (XInputDataFetcherWin::XInputGetStateExFunc) nullptr;
            }));
        break;
      default:
        return;
    }
  }

  void SetUpTestEnv(WgiTestErrorCode error_code = WgiTestErrorCode::kOk) {
    wgi_environment_ = std::make_unique<FakeWinrtWgiEnvironment>(error_code);
    SetUpXInputEnv(error_code);
    auto fetcher = std::make_unique<WgiDataFetcherWin>();
    wgi_fetcher_ = fetcher.get();

    // Initialize provider to retrieve pad state.
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/nullptr, std::move(fetcher),
        std::move(polling_thread));

    FlushPollingThread();
  }

  // Adds MockGamepadDataFetcher to the GamepadProvider and also adds an already
  // sanitized generic MockGamepad at index 0.
  void SetUpMockGamepadDataFetcherAndAddMockGamepadAtIndex0() {
    Gamepads zero_data;
    zero_data.items[0].connected = true;
    zero_data.items[0].timestamp = 0;
    zero_data.items[0].buttons_length = 1;
    zero_data.items[0].axes_length = 1;
    zero_data.items[0].buttons[0].value = 0.0f;
    zero_data.items[0].buttons[0].pressed = false;
    zero_data.items[0].axes[0] = 0.0f;

    Gamepads active_data;
    active_data.items[0].connected = true;
    active_data.items[0].timestamp = 0;
    active_data.items[0].buttons_length = 1;
    active_data.items[0].axes_length = 1;
    active_data.items[0].buttons[0].value = 1.0f;
    active_data.items[0].buttons[0].pressed = true;
    active_data.items[0].axes[0] = -1.0f;

    auto mock_generic_fetcher =
        std::make_unique<MockGamepadDataFetcher>(active_data);
    mock_generic_fetcher_ = mock_generic_fetcher.get();
    provider_->AddGamepadDataFetcher(std::move(mock_generic_fetcher));
    FlushPollingThread();

    provider_->Resume();

    base::ReadOnlySharedMemoryRegion shared_memory_region =
        provider_->DuplicateSharedMemoryRegion();
    base::ReadOnlySharedMemoryMapping shared_memory_mapping =
        shared_memory_region.Map();
    EXPECT_TRUE(shared_memory_mapping.IsValid());
    const GamepadHardwareBuffer* gamepad_buffer =
        static_cast<const GamepadHardwareBuffer*>(
            shared_memory_mapping.memory());

    // First we send zeroed out data for sanitization.
    mock_generic_fetcher_->SetTestData(zero_data);
    mock_generic_fetcher_->WaitForDataReadAndCallbacksIssued();
    // Then we send the actual data.
    mock_generic_fetcher_->SetTestData(active_data);
    mock_generic_fetcher_->WaitForDataReadAndCallbacksIssued();

    Gamepads output;
    ReadGamepadHardwareBuffer(gamepad_buffer, &output);

    // The gamepad data at index 0 should reflect the values in `active_data`,
    // indicating that a gamepad of source GamepadSource::kTest has been added
    // at index 0.
    ASSERT_EQ(active_data.items[0].buttons_length,
              output.items[0].buttons_length);
    EXPECT_EQ(active_data.items[0].buttons[0].value,
              output.items[0].buttons[0].value);
    EXPECT_TRUE(output.items[0].buttons[0].pressed);
    ASSERT_EQ(active_data.items[0].axes_length, output.items[0].axes_length);
    EXPECT_EQ(active_data.items[0].axes[0], output.items[0].axes[0]);

    provider_->Pause();
  }

  void FlushPollingThread() { polling_thread_->FlushForTesting(); }

  // Sleep until the shared memory buffer's seqlock advances the buffer version,
  // indicating that the gamepad provider has written to it after polling the
  // gamepad fetchers. The buffer will report an odd value for the version if
  // the buffer is not in a consistent state, so we also require that the value
  // is even before continuing.
  void WaitForData(const GamepadHardwareBuffer* buffer) {
    const base::subtle::Atomic32 initial_version = buffer->seqlock.ReadBegin();
    base::subtle::Atomic32 current_version;
    do {
      base::PlatformThread::Sleep(base::Milliseconds(10));
      current_version = buffer->seqlock.ReadBegin();
    } while (current_version % 2 || current_version == initial_version);
  }

  void ReadGamepadHardwareBuffer(const GamepadHardwareBuffer* buffer,
                                 Gamepads* output) {
    memset(output, 0, sizeof(Gamepads));
    base::subtle::Atomic32 version;
    do {
      version = buffer->seqlock.ReadBegin();
      memcpy(output, &buffer->data, sizeof(Gamepads));
    } while (buffer->seqlock.ReadRetry(version));
  }

  void CheckGamepadAdded(PadState* pad_state,
                         GamepadHapticActuatorType actuator_type) {
    // Check size of connected gamepad list and ensure gamepad state
    // is initialized correctly.
    EXPECT_TRUE(pad_state->is_initialized);
    Gamepad& pad = pad_state->data;
    EXPECT_TRUE(pad.connected);
    EXPECT_TRUE(pad.vibration_actuator.not_null);
    EXPECT_EQ(pad.vibration_actuator.type, actuator_type);
  }

  void CheckGamepadRemoved() {
    EXPECT_TRUE(fetcher().GetGamepadsForTesting().empty());
  }

  void CheckButtonState(
      int canonical_button_index,
      ABI::Windows::Gaming::Input::GamepadButtons input_buttons_bit_mask,
      base::span<GamepadButton const> output_buttons) {
    static constexpr auto kCanonicalButtonBitMaskMapping =
        base::MakeFixedFlatMap<int,
                               ABI::Windows::Gaming::Input::GamepadButtons>(
            {{BUTTON_INDEX_PRIMARY,
              ABI::Windows::Gaming::Input::GamepadButtons_A},
             {BUTTON_INDEX_SECONDARY,
              ABI::Windows::Gaming::Input::GamepadButtons_B},
             {BUTTON_INDEX_TERTIARY,
              ABI::Windows::Gaming::Input::GamepadButtons_X},
             {BUTTON_INDEX_QUATERNARY,
              ABI::Windows::Gaming::Input::GamepadButtons_Y},
             {BUTTON_INDEX_LEFT_SHOULDER,
              ABI::Windows::Gaming::Input::GamepadButtons_LeftShoulder},
             {BUTTON_INDEX_RIGHT_SHOULDER,
              ABI::Windows::Gaming::Input::GamepadButtons_RightShoulder},
             {BUTTON_INDEX_BACK_SELECT,
              ABI::Windows::Gaming::Input::GamepadButtons_View},
             {BUTTON_INDEX_START,
              ABI::Windows::Gaming::Input::GamepadButtons_Menu},
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
              ABI::Windows::Gaming::Input::GamepadButtons_Paddle4}});

    const auto button_bit_mask =
        kCanonicalButtonBitMaskMapping.find(canonical_button_index);
    if (button_bit_mask == kCanonicalButtonBitMaskMapping.end()) {
      ADD_FAILURE() << "Unsupported CanonicalButtonIndex value: "
                    << canonical_button_index;
      return;
    }

    if (input_buttons_bit_mask & button_bit_mask->second) {
      EXPECT_TRUE(output_buttons[canonical_button_index].pressed);
      EXPECT_NEAR(output_buttons[canonical_button_index].value, 1.0f,
                  kErrorTolerance);
    } else {
      EXPECT_FALSE(output_buttons[canonical_button_index].pressed);
      EXPECT_NEAR(output_buttons[canonical_button_index].value, 0.0f,
                  kErrorTolerance);
    }
  }

  void CheckMetaButtonState(bool is_pressed,
                            base::span<GamepadButton const> output_buttons) {
    if (is_pressed) {
      EXPECT_TRUE(output_buttons[BUTTON_INDEX_META].pressed);
      EXPECT_NEAR(output_buttons[BUTTON_INDEX_META].value, 1.0f,
                  kErrorTolerance);
      return;
    }
    EXPECT_FALSE(output_buttons[BUTTON_INDEX_META].pressed);
    EXPECT_NEAR(output_buttons[BUTTON_INDEX_META].value, 0.0f, kErrorTolerance);
  }

  void CheckGamepadInputResult(const GamepadReading& input,
                               const Gamepad& output,
                               bool has_paddles) {
    if (has_paddles) {
      EXPECT_EQ(output.buttons_length, kGamepadWithPaddlesButtonsLength);
    } else {
      EXPECT_EQ(output.buttons_length, kGamepadButtonsLength);
    }

    CheckButtonState(BUTTON_INDEX_PRIMARY, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_SECONDARY, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_TERTIARY, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_QUATERNARY, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_LEFT_SHOULDER, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_RIGHT_SHOULDER, input.Buttons,
                     output.buttons);
    CheckButtonState(BUTTON_INDEX_BACK_SELECT, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_START, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_LEFT_THUMBSTICK, input.Buttons,
                     output.buttons);
    CheckButtonState(BUTTON_INDEX_RIGHT_THUMBSTICK, input.Buttons,
                     output.buttons);
    CheckButtonState(BUTTON_INDEX_DPAD_UP, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_DPAD_DOWN, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_DPAD_LEFT, input.Buttons, output.buttons);
    CheckButtonState(BUTTON_INDEX_DPAD_RIGHT, input.Buttons, output.buttons);
    if (has_paddles) {
      // The Meta button position at CanonicalButtonIndex::BUTTON_INDEX_META in
      // the buttons array should be occupied with a NullButton when paddles are
      // present.
      CheckButtonState(BUTTON_INDEX_META + 1, input.Buttons, output.buttons);
      CheckButtonState(BUTTON_INDEX_META + 2, input.Buttons, output.buttons);
      CheckButtonState(BUTTON_INDEX_META + 3, input.Buttons, output.buttons);
      CheckButtonState(BUTTON_INDEX_META + 4, input.Buttons, output.buttons);
    }

    EXPECT_EQ(input.LeftTrigger > GamepadButton::kDefaultButtonPressedThreshold,
              output.buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed);
    EXPECT_NEAR(input.LeftTrigger,
                output.buttons[BUTTON_INDEX_LEFT_TRIGGER].value,
                kErrorTolerance);

    EXPECT_EQ(
        input.RightTrigger > GamepadButton::kDefaultButtonPressedThreshold,
        output.buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed);
    EXPECT_NEAR(input.RightTrigger,
                output.buttons[BUTTON_INDEX_RIGHT_TRIGGER].value,
                kErrorTolerance);

    // Invert the Y thumbstick axes to match the Standard Gamepad. WGI
    // thumbstick axes use +up/+right but the Standard Gamepad uses
    // +down/+right.
    EXPECT_EQ(output.axes_length, kGamepadAxesLength);
    EXPECT_NEAR(input.LeftThumbstickX, output.axes[AXIS_INDEX_LEFT_STICK_X],
                kErrorTolerance);
    EXPECT_NEAR(input.LeftThumbstickY,
                -1.0f * output.axes[AXIS_INDEX_LEFT_STICK_Y], kErrorTolerance);
    EXPECT_NEAR(input.RightThumbstickX, output.axes[AXIS_INDEX_RIGHT_STICK_X],
                kErrorTolerance);
    EXPECT_NEAR(input.RightThumbstickY,
                -1.0f * output.axes[AXIS_INDEX_RIGHT_STICK_Y], kErrorTolerance);
  }

  WgiDataFetcherWin& fetcher() const { return *wgi_fetcher_; }

  // Gets called after PlayEffect or ResetVibration.
  void HapticsCallback(mojom::GamepadHapticsResult result) {
    haptics_callback_count_++;
    haptics_callback_result_ = result;
  }

  void SimulateDualRumbleEffect(int pad_index) {
    base::RunLoop run_loop;
    provider_->PlayVibrationEffectOnce(
        pad_index,
        mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
        mojom::GamepadEffectParameters::New(
            kDurationMillis, kZeroStartDelayMillis, kStrongMagnitude,
            kWeakMagnitude, /*left_trigger=*/0, /*right_trigger=*/0),
        base::BindOnce(&WgiDataFetcherWinTest::HapticsCallback,
                       base::Unretained(this))
            .Then(run_loop.QuitClosure()));
    FlushPollingThread();
    run_loop.Run();
  }

  void SimulateResetVibration(int pad_index) {
    base::RunLoop run_loop;
    provider_->ResetVibrationActuator(
        pad_index, base::BindOnce(&WgiDataFetcherWinTest::HapticsCallback,
                                  base::Unretained(this))
                       .Then(run_loop.QuitClosure()));
    FlushPollingThread();
    run_loop.Run();
  }

  // Should be used to update the gamepad device state and avoid racing
  // condition between the polling thread and the test framework.
  void UpdateGamepadStateOnThePollingThread(
      Microsoft::WRL::ComPtr<FakeIGamepad> gamepad,
      ABI::Windows::Gaming::Input::GamepadReading gamepad_state) {
    base::RunLoop run_loop;
    polling_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
                     gamepad->SetCurrentReading(gamepad_state);
                   }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  int haptics_callback_count_ = 0;
  mojom::GamepadHapticsResult haptics_callback_result_ =
      mojom::GamepadHapticsResult::GamepadHapticsResultError;
  std::unique_ptr<GamepadProvider> provider_;
  std::unique_ptr<FakeWinrtWgiEnvironment> wgi_environment_;

 private:
  raw_ptr<WgiDataFetcherWin> wgi_fetcher_;
  raw_ptr<MockGamepadDataFetcher> mock_generic_fetcher_;
  raw_ptr<base::Thread> polling_thread_;
};

TEST_F(WgiDataFetcherWinTest, AddAndRemoveWgiGamepad) {
  SetUpTestEnv();

  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);
  EXPECT_TRUE(fetcher().GetGamepadsForTesting().empty());

  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  const auto fake_trigger_rumble_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  const auto fake_unknown_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();

  // Check that the event handlers were added.
  EXPECT_EQ(fake_gamepad_statics->GetGamepadAddedEventHandlerCount(), 1u);
  EXPECT_EQ(fake_gamepad_statics->GetGamepadRemovedEventHandlerCount(), 1u);

  // Simulate the gamepad adding behavior by passing an IGamepad, and make
  // the gamepad-adding callback return on a different thread, demonstrated the
  // multi-threaded apartments setting of the GamepadStatics COM API.
  // Corresponding threading simulation is in FakeIGamepadStatics class.
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_trigger_rumble_gamepad, kTriggerRumbleHardwareProductId,
      kHardwareVendorId, kGamepadDisplayName);
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_unknown_gamepad, kUnknownHardwareProductId, kUnknownHardwareVendorId,
      kGamepadDisplayName);

  // Wait for the gampad polling thread to handle the gamepad added events.
  FlushPollingThread();

  // Assert that the gamepads have been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 3u);
  auto gamepad_iter = gamepads.begin();
  CheckGamepadAdded(fetcher().GetPadState(gamepad_iter++->first),
                    GamepadHapticActuatorType::kDualRumble);
  CheckGamepadAdded(fetcher().GetPadState(gamepad_iter++->first),
                    GamepadHapticActuatorType::kTriggerRumble);
  CheckGamepadAdded(fetcher().GetPadState(gamepad_iter->first),
                    GamepadHapticActuatorType::kDualRumble);

  // Simulate the gamepad removing behavior, and make the gamepad-removing
  // callback return on a different thread, demonstrated the multi-threaded
  // apartments setting of the GamepadStatics COM API. Corresponding threading
  // simulation is in FakeIGamepadStatics class.
  fake_gamepad_statics->SimulateGamepadRemoved(fake_gamepad);
  fake_gamepad_statics->SimulateGamepadRemoved(fake_trigger_rumble_gamepad);
  fake_gamepad_statics->SimulateGamepadRemoved(fake_unknown_gamepad);

  // Wait for the gampad polling thread to handle the gamepad removed event.
  FlushPollingThread();

  CheckGamepadRemoved();
}

TEST_F(WgiDataFetcherWinTest, AddGamepadAddedEventHandlerErrorHandling) {
  // Let fake gamepad statics add_GamepadAdded return failure code to
  // test error handling.
  SetUpTestEnv(WgiTestErrorCode::kGamepadAddGamepadAddedFailed);

  // Check WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kAddGamepadAddedFailed);
  auto* gamepad_statics = FakeIGamepadStatics::GetInstance();
  EXPECT_EQ(gamepad_statics->GetGamepadAddedEventHandlerCount(), 0u);
}

TEST_F(WgiDataFetcherWinTest, AddGamepadRemovedEventHandlerErrorHandling) {
  // Let fake gamepad statics add_GamepadRemoved return failure code to
  // test error handling.
  SetUpTestEnv(WgiTestErrorCode::kGamepadAddGamepadRemovedFailed);

  // Check WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kAddGamepadRemovedFailed);
  auto* gamepad_statics = FakeIGamepadStatics::GetInstance();
  EXPECT_EQ(gamepad_statics->GetGamepadRemovedEventHandlerCount(), 0u);
}

TEST_F(WgiDataFetcherWinTest, WgiGamepadActivationFactoryErrorHandling) {
  // Let fake RoGetActivationFactory return failure code to
  // test error handling.
  SetUpTestEnv(WgiTestErrorCode::kErrorWgiGamepadActivateFailed);

  // Check WGI initialization status.
  EXPECT_EQ(
      fetcher().GetInitializationState(),
      WgiDataFetcherWin::InitializationState::kRoGetActivationFactoryFailed);
}

// This test case checks that the gamepad data obtained by WgiDataFetcherWin is
// correct for the following PadState array configuration:
// Index 0: Generic gamepad with source = GamepadSource::kTest;
// Index 1: WGI gamepad with source = GamepadSource::kWinWgi;
// Index 2: WGI gamepad with source = GamepadSource::kWinWgi;
// Index 3: Empty with source = GamepadSource::kNone;
// Moreover, this test also asserts that when a meta button press is detected,
// it should be redirected to the lowest-index WGI gamepad, i.e., the gamepad at
// index 1.
TEST_F(WgiDataFetcherWinTest, VerifyGamepadInput) {
  SetUpTestEnv();
  SetUpMockGamepadDataFetcherAndAddMockGamepadAtIndex0();

  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  const auto fake_gamepad_with_paddles = Microsoft::WRL::Make<FakeIGamepad>();
  fake_gamepad_with_paddles->SetHasPaddles(true);

  // Add a simulated WGI device.
  provider_->Resume();
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad_with_paddles, kHardwareProductId, kHardwareVendorId,
      kGamepadDisplayName);

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      provider_->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  EXPECT_TRUE(shared_memory_mapping.IsValid());
  const GamepadHardwareBuffer* gamepad_buffer =
      static_cast<const GamepadHardwareBuffer*>(shared_memory_mapping.memory());

  // State should be first set to the rest position to satisfy sanitization pre-
  // requisites.
  UpdateGamepadStateOnThePollingThread(fake_gamepad,
                                       kZeroPositionGamepadReading);
  UpdateGamepadStateOnThePollingThread(fake_gamepad_with_paddles,
                                       kZeroPositionGamepadReading);
  WaitForData(gamepad_buffer);

  UpdateGamepadStateOnThePollingThread(fake_gamepad, kGamepadReading);
  UpdateGamepadStateOnThePollingThread(fake_gamepad_with_paddles,
                                       kGamepadReading);
  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kGamepadReading, output.items[1],
                          /*has_paddles=*/false);
  CheckGamepadInputResult(kGamepadReading, output.items[2],
                          /*has_paddles=*/true);

  // Verify that the meta button input goes to the gamepad at index 0;
  CheckMetaButtonState(/*is_meta_pressed=*/true, output.items[1].buttons);
  CheckMetaButtonState(/*is_meta_pressed=*/false, output.items[2].buttons);
}

TEST_F(WgiDataFetcherWinTest, PlayDualRumbleEffect) {
  SetUpTestEnv();
  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();

  // Add a simulated WGI device.
  provider_->Resume();
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);

  SimulateDualRumbleEffect(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 1);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
  ABI::Windows::Gaming::Input::GamepadVibration fake_gamepad_vibration;
  fake_gamepad->get_Vibration(&fake_gamepad_vibration);
  EXPECT_EQ(fake_gamepad_vibration.LeftMotor, kStrongMagnitude);
  EXPECT_EQ(fake_gamepad_vibration.RightMotor, kWeakMagnitude);
  EXPECT_EQ(fake_gamepad_vibration.LeftTrigger, 0.0f);
  EXPECT_EQ(fake_gamepad_vibration.RightTrigger, 0.0f);

  // Calling ResetVibration sets the vibration intensity to 0 for all motors.
  SimulateResetVibration(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 2);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
  fake_gamepad->get_Vibration(&fake_gamepad_vibration);
  EXPECT_EQ(fake_gamepad_vibration.LeftMotor, 0.0f);
  EXPECT_EQ(fake_gamepad_vibration.RightMotor, 0.0f);
  EXPECT_EQ(fake_gamepad_vibration.LeftTrigger, 0.0f);
  EXPECT_EQ(fake_gamepad_vibration.RightTrigger, 0.0f);

  // Attempting to call haptics methods on invalid pad_id's will return a result
  // of type GamepadHapticsResultNotSupported.
  fake_gamepad_statics->SimulateGamepadRemoved(fake_gamepad);
  SimulateDualRumbleEffect(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 3);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
  SimulateResetVibration(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 4);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
}

// When an error happens when calling GamepadGetCurrentReading, the state in
// the shared buffer will not be modified.
TEST_F(WgiDataFetcherWinTest, WgiGamepadGetCurrentReadingError) {
  SetUpTestEnv();

  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();

  // Add a simulated WGI device.
  provider_->Resume();
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      provider_->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  EXPECT_TRUE(shared_memory_mapping.IsValid());
  const GamepadHardwareBuffer* gamepad_buffer =
      static_cast<const GamepadHardwareBuffer*>(shared_memory_mapping.memory());

  // State should be first set to the rest position to satisfy sanitization pre-
  // requisites.
  UpdateGamepadStateOnThePollingThread(fake_gamepad,
                                       kZeroPositionGamepadReading);
  WaitForData(gamepad_buffer);

  wgi_environment_->SimulateError(
      WgiTestErrorCode::kErrorWgiGamepadGetCurrentReadingFailed);

  UpdateGamepadStateOnThePollingThread(fake_gamepad, kGamepadReading);
  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kZeroPositionGamepadReading, output.items[0],
                          /*has_paddles=*/false);
  // Even if the data fetcher failed to obtain the gamepad data through WGI, the
  // 0-index gamepad should still display the meta button input, which might
  // have been triggered by other gamepad.
  CheckMetaButtonState(/*is_meta_pressed=*/true, output.items[0].buttons);
}

// If Gamepad::GetButtonLabel fails, the stored gamepad state buttons_length
// property will be equal to 17, even though the connected device may have
// paddles - i.e., the paddles will not be recognized.
TEST_F(WgiDataFetcherWinTest, WgiGamepadGetButtonLabelError) {
  SetUpTestEnv(WgiTestErrorCode::kErrorWgiGamepadGetButtonLabelFailed);

  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad_with_paddles = Microsoft::WRL::Make<FakeIGamepad>();
  fake_gamepad_with_paddles->SetHasPaddles(true);

  // Add a simulated WGI device.
  provider_->Resume();
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad_with_paddles, kHardwareProductId, kHardwareVendorId,
      kGamepadDisplayName);

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      provider_->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  EXPECT_TRUE(shared_memory_mapping.IsValid());
  const GamepadHardwareBuffer* gamepad_buffer =
      static_cast<const GamepadHardwareBuffer*>(shared_memory_mapping.memory());

  // State should be first set to the rest position to satisfy sanitization pre-
  // requisites.
  UpdateGamepadStateOnThePollingThread(fake_gamepad_with_paddles,
                                       kZeroPositionGamepadReading);
  WaitForData(gamepad_buffer);

  UpdateGamepadStateOnThePollingThread(fake_gamepad_with_paddles,
                                       kGamepadReading);
  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kGamepadReading, output.items[0],
                          /*has_paddles=*/false);
  // The gamepad should still receive the meta input, even if it failed to
  // obtain paddle data.
  CheckMetaButtonState(/*is_meta_pressed=*/true, output.items[0].buttons);
}

// This test checks that the WgiDataFetcherWin did not enumerate any controllers
// it was not supposed to - e.g., Dualshock and Nintendo controllers.
TEST_F(WgiDataFetcherWinTest, ShouldNotEnumerateControllers) {
  SetUpTestEnv();
  constexpr GamepadId kShouldNotEnumerateControllers[] = {
      GamepadId::kNintendoProduct2006, GamepadId::kNintendoProduct2007,
      GamepadId::kNintendoProduct2009, GamepadId::kNintendoProduct200e,
      GamepadId::kSonyProduct05c4,     GamepadId::kSonyProduct09cc};

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();

  for (const GamepadId& device_id : kShouldNotEnumerateControllers) {
    const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
    uint16_t vendor_id;
    uint16_t product_id;
    std::tie(vendor_id, product_id) =
        GamepadIdList::Get().GetDeviceIdsFromGamepadId(device_id);
    fake_gamepad_statics->SimulateGamepadAdded(fake_gamepad, product_id,
                                               vendor_id, "");
  }

  // Wait for the gampad polling thread to handle the gamepad added event.
  FlushPollingThread();

  // Assert that the gamepad has not been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  EXPECT_EQ(gamepads.size(), 0u);
}

// Test class created to assert that gamepads gamepads with trigger-rumble are
// being detected correctly.
class WgiDataFetcherTriggerRumbleSupportTest
    : public WgiDataFetcherWinTest,
      public testing::WithParamInterface<GamepadId> {};

TEST_P(WgiDataFetcherTriggerRumbleSupportTest,
       GamepadShouldHaveTriggerRumbleSupport) {
  SetUpTestEnv();
  const GamepadId gamepad_id = GetParam();
  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  uint16_t vendor_id;
  uint16_t product_id;
  std::tie(vendor_id, product_id) =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(gamepad_id);
  fake_gamepad_statics->SimulateGamepadAdded(fake_gamepad, product_id,
                                             vendor_id, "");
  // Wait for the gampad polling thread to handle the gamepad added event.
  FlushPollingThread();

  // Assert that the gamepad has been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 1u);
  // Assert that the gamepad has been assigned the correct type.
  CheckGamepadAdded(fetcher().GetPadState(gamepads.begin()->first),
                    GamepadHapticActuatorType::kTriggerRumble);
}
INSTANTIATE_TEST_SUITE_P(WgiDataFetcherTriggerRumbleSupportTests,
                         WgiDataFetcherTriggerRumbleSupportTest,
                         testing::ValuesIn(kGamepadsWithTriggerRumble));

// class created to simulate scenarios where the OS may throw errors.
class WgiDataFetcherWinErrorTest
    : public WgiDataFetcherWinTest,
      public testing::WithParamInterface<WgiTestErrorCode> {};

// This test simulates OS errors that prevent the controller from being
// enumerated by WgiDataFetcherWin.
TEST_P(WgiDataFetcherWinErrorTest, GamepadShouldNotbeEnumerated) {
  const WgiTestErrorCode error_code = GetParam();
  SetUpTestEnv(error_code);
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();

  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);

  // Wait for the gampad polling thread to handle the gamepad added event.
  FlushPollingThread();

  // Assert that the gamepad has not been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  EXPECT_EQ(gamepads.size(), 0u);
}

INSTANTIATE_TEST_SUITE_P(WgiDataFetcherWinErrorTests,
                         WgiDataFetcherWinErrorTest,
                         testing::ValuesIn(kErrors));

// Class used to simulate XInput loading error scenarios.
class WgiDataFetcherWinXInputErrorTest
    : public WgiDataFetcherWinTest,
      public testing::WithParamInterface<WgiTestErrorCode> {};

TEST_P(WgiDataFetcherWinXInputErrorTest, MetaUnavailableWhenXInputFailsToLoad) {
  const WgiTestErrorCode error_code = GetParam();
  SetUpTestEnv(error_code);
  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  const auto fake_gamepad_with_paddles = Microsoft::WRL::Make<FakeIGamepad>();
  fake_gamepad_with_paddles->SetHasPaddles(true);

  // Add a simulated WGI device.
  provider_->Resume();
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);
  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad_with_paddles, kHardwareProductId, kHardwareVendorId,
      kGamepadDisplayName);

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      provider_->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  EXPECT_TRUE(shared_memory_mapping.IsValid());
  const GamepadHardwareBuffer* gamepad_buffer =
      static_cast<const GamepadHardwareBuffer*>(shared_memory_mapping.memory());

  // State should be first set to the rest position to satisfy sanitization pre-
  // requisites.
  UpdateGamepadStateOnThePollingThread(fake_gamepad,
                                       kZeroPositionGamepadReading);
  UpdateGamepadStateOnThePollingThread(fake_gamepad_with_paddles,
                                       kZeroPositionGamepadReading);
  WaitForData(gamepad_buffer);

  // Set the gamepads to the actual test state.
  UpdateGamepadStateOnThePollingThread(fake_gamepad, kGamepadReading);
  UpdateGamepadStateOnThePollingThread(fake_gamepad_with_paddles,
                                       kGamepadReading);
  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kGamepadReading, output.items[0],
                          /*has_paddles=*/false);
  CheckGamepadInputResult(kGamepadReading, output.items[1],
                          /*has_paddles=*/true);

  // Verify that the meta button input is not available to any gamepad;
  CheckMetaButtonState(/*is_meta_pressed=*/false, output.items[0].buttons);
  CheckMetaButtonState(/*is_meta_pressed=*/false, output.items[1].buttons);
}

INSTANTIATE_TEST_SUITE_P(WgiDataFetcherWinXInputErrorTests,
                         WgiDataFetcherWinXInputErrorTest,
                         testing::ValuesIn(kXInputLoadErrors));

// Tests the gamepad id generation both when RawGameController2::get_DisplayName
// succeeds and fails when calling WgiDataFetcherWin::GetDisplayName. In case of
// failure, a gamepad will be added with a default DisplayName.
class WgiDataFetcherWinGamepadIdTest
    : public WgiDataFetcherWinTest,
      public testing::WithParamInterface<bool> {};

TEST_P(WgiDataFetcherWinGamepadIdTest, GamepadIds) {
  const bool should_get_display_name_fail = GetParam();
  std::string display_name;
  if (should_get_display_name_fail) {
    SetUpTestEnv(
        WgiTestErrorCode::kErrorWgiRawGameControllerGetDisplayNameFailed);
    display_name = "Unknown Gamepad";
  } else {
    SetUpTestEnv();
    display_name = kGamepadDisplayName;
  }

  constexpr GamepadId kGamepadIds[] = {// XInputTypeNone gamepad.
                                       GamepadId::kMicrosoftProduct0b21,
                                       // XInputTypeXbox360 gamepad.
                                       GamepadId::kMicrosoftProduct028e,
                                       // XInputTypeXboxOne gamepad.
                                       GamepadId::kMicrosoftProduct0b12,
                                       GamepadId::kUnknownGamepad};

  // Iterate and add fake gamepads.
  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  for (const GamepadId& device_id : kGamepadIds) {
    const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
    uint16_t vendor_id, product_id;
    if (device_id == GamepadId::kUnknownGamepad) {
      vendor_id = kUnknownHardwareVendorId;
      product_id = kUnknownHardwareProductId;
    } else {
      std::tie(vendor_id, product_id) =
          GamepadIdList::Get().GetDeviceIdsFromGamepadId(device_id);
    }
    fake_gamepad_statics->SimulateGamepadAdded(fake_gamepad, product_id,
                                               vendor_id, display_name);
  }

  // Wait for the gampad polling thread to handle the gamepad added event.
  FlushPollingThread();

  // Assert that the gamepads have been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  EXPECT_EQ(gamepads.size(), 4u);

  // Build vector with the expected id strings.
  const std::u16string display_name_16 = base::UTF8ToUTF16(display_name);
  std::vector<std::u16string> expected_gamepad_id_strings{
      display_name_16 + u" (STANDARD GAMEPAD Vendor: 045e Product: 0b21)",
      kKnownXInputDeviceId, kKnownXInputDeviceId,
      display_name_16 + u" (STANDARD GAMEPAD)"};

  size_t id_string_index = 0;
  for (auto it = gamepads.begin(); it != gamepads.end(); ++it) {
    PadState* pad = fetcher().GetPadState(it->first);
    std::u16string display_id(pad->data.id);
    EXPECT_EQ(display_id, expected_gamepad_id_strings[id_string_index++]);
  }
}

INSTANTIATE_TEST_SUITE_P(WgiDataFetcherWinGamepadIdTests,
                         WgiDataFetcherWinGamepadIdTest,
                         testing::Bool());

}  // namespace device
