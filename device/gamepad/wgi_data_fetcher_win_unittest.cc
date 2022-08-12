// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_data_fetcher_win.h"

#include "base/bind.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "device/gamepad/test_support/fake_igamepad.h"
#include "device/gamepad/test_support/fake_igamepad_statics.h"
#include "device/gamepad/test_support/fake_winrt_wgi_environment.h"
#include "device/gamepad/wgi_data_fetcher_win.h"
#include "device/gamepad/wgi_gamepad_device.h"
#include "services/device/device_service_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::ABI::Windows::Gaming::Input::GamepadReading;

constexpr uint16_t kHardwareVendorId = 0x045e;
constexpr uint16_t kHardwareProductId = 0x028e;
constexpr uint16_t kTriggerRumbleHardwareProductId = 0x0b13;

constexpr char kGamepadDisplayName[] = "XBOX_SERIES_X";
constexpr double kErrorTolerance = 1e-5;

constexpr unsigned int kGamepadButtonsLength = 16;
// The Meta button position at CanonicalButtonIndex::BUTTON_INDEX_META in the
// buttons array should be occupied with a NullButton. So we end up with:
// 16 buttons + 1 null button + 4 paddles = 21 buttons.
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
    GamepadId::kMicrosoftProduct0b12, GamepadId::kMicrosoftProduct0b13,
    GamepadId::kMicrosoftProduct02e3, GamepadId::kMicrosoftProduct0b00,
    GamepadId::kMicrosoftProduct0b05, GamepadId::kMicrosoftProduct0b22};

constexpr ErrorCode kErrors[] = {
    ErrorCode::kErrorWgiRawGameControllerActivateFailed,
    ErrorCode::kErrorWgiRawGameControllerFromGameControllerFailed,
    ErrorCode::kErrorWgiRawGameControllerGetHardwareProductIdFailed,
    ErrorCode::kErrorWgiRawGameControllerGetHardwareVendorIdFailed};

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

class WgiDataFetcherWinTest : public DeviceServiceTestBase {
 public:
  WgiDataFetcherWinTest() = default;
  ~WgiDataFetcherWinTest() override = default;

  void SetUp() override {
    // Windows.Gaming.Input is available in Windows 10.0.10240.0 and later.
    if (base::win::GetVersion() < base::win::Version::WIN10)
      GTEST_SKIP();
    DeviceServiceTestBase::SetUp();
  }

  void SetUpTestEnv(ErrorCode error_code = ErrorCode::kOk) {
    EXPECT_TRUE(base::win::ScopedHString::ResolveCoreWinRTStringDelayload());
    wgi_environment_ = std::make_unique<FakeWinrtWgiEnvironment>(error_code);
    auto fetcher = std::make_unique<WgiDataFetcherWin>();
    fetcher_ = fetcher.get();

    // Initialize provider to retrieve pad state.
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/nullptr, std::move(fetcher),
        std::move(polling_thread));

    FlushPollingThread();
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
      const GamepadButton output_buttons[]) {
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

    // WGI does not have support to the Meta button.
    if (canonical_button_index == BUTTON_INDEX_META) {
      EXPECT_FALSE(output_buttons[canonical_button_index].pressed);
      EXPECT_FALSE(output_buttons[canonical_button_index].touched);
      EXPECT_FALSE(output_buttons[canonical_button_index].used);
      EXPECT_NEAR(output_buttons[canonical_button_index].value, 0.0f,
                  kErrorTolerance);
      return;
    }

    const auto* button_bit_mask =
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

  void CheckGamepadInputResult(const GamepadReading& input,
                               const Gamepad& output,
                               bool has_paddles = false) {
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
      CheckButtonState(BUTTON_INDEX_META, input.Buttons, output.buttons);
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

  WgiDataFetcherWin& fetcher() const { return *fetcher_; }

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

 protected:
  int haptics_callback_count_ = 0;
  mojom::GamepadHapticsResult haptics_callback_result_ =
      mojom::GamepadHapticsResult::GamepadHapticsResultError;
  std::unique_ptr<GamepadProvider> provider_;
  std::unique_ptr<FakeWinrtWgiEnvironment> wgi_environment_;

 private:
  raw_ptr<WgiDataFetcherWin> fetcher_;
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

  // Wait for the gampad polling thread to handle the gamepad added events.
  FlushPollingThread();

  // Assert that the gamepads have been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 2u);
  auto gamepad_iter = gamepads.begin();
  CheckGamepadAdded(fetcher().GetPadState(gamepad_iter++->first),
                    GamepadHapticActuatorType::kDualRumble);
  CheckGamepadAdded(fetcher().GetPadState(gamepad_iter->first),
                    GamepadHapticActuatorType::kTriggerRumble);

  // Simulate the gamepad removing behavior, and make the gamepad-removing
  // callback return on a different thread, demonstrated the multi-threaded
  // apartments setting of the GamepadStatics COM API. Corresponding threading
  // simulation is in FakeIGamepadStatics class.
  fake_gamepad_statics->SimulateGamepadRemoved(fake_gamepad);
  fake_gamepad_statics->SimulateGamepadRemoved(fake_trigger_rumble_gamepad);

  // Wait for the gampad polling thread to handle the gamepad removed event.
  FlushPollingThread();

  CheckGamepadRemoved();
}

TEST_F(WgiDataFetcherWinTest, AddGamepadAddedEventHandlerErrorHandling) {
  // Let fake gamepad statics add_GamepadAdded return failure code to
  // test error handling.
  SetUpTestEnv(ErrorCode::kGamepadAddGamepadAddedFailed);

  // Check WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kAddGamepadAddedFailed);
  auto* gamepad_statics = FakeIGamepadStatics::GetInstance();
  EXPECT_EQ(gamepad_statics->GetGamepadAddedEventHandlerCount(), 0u);
}

TEST_F(WgiDataFetcherWinTest, AddGamepadRemovedEventHandlerErrorHandling) {
  // Let fake gamepad statics add_GamepadRemoved return failure code to
  // test error handling.
  SetUpTestEnv(ErrorCode::kGamepadAddGamepadRemovedFailed);

  // Check WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kAddGamepadRemovedFailed);
  auto* gamepad_statics = FakeIGamepadStatics::GetInstance();
  EXPECT_EQ(gamepad_statics->GetGamepadRemovedEventHandlerCount(), 0u);
}

TEST_F(WgiDataFetcherWinTest, WgiGamepadActivationFactoryErrorHandling) {
  // Let fake RoGetActivationFactory return failure code to
  // test error handling.
  SetUpTestEnv(ErrorCode::kErrorWgiGamepadActivateFailed);

  // Check WGI initialization status.
  EXPECT_EQ(
      fetcher().GetInitializationState(),
      WgiDataFetcherWin::InitializationState::kRoGetActivationFactoryFailed);
}

// If RawGameController2::get_DisplayName fails when calling
// WgiDataFetcherWin::GetDisplayName, a gamepad will be added with a default
// DisplayName.
TEST_F(WgiDataFetcherWinTest, FailuretoGetDisplayNameOnGamepadAdded) {
  constexpr char16_t kDefaultDisplayName[] = u"Unknown Gamepad";
  SetUpTestEnv(ErrorCode::kErrorWgiRawGameControllerGetDisplayNameFailed);

  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();

  fake_gamepad_statics->SimulateGamepadAdded(
      fake_gamepad, kHardwareProductId, kHardwareVendorId, kGamepadDisplayName);

  // Wait for the gampad polling thread to handle the gamepad added event.
  FlushPollingThread();

  // Assert that the gamepad has not been added to the DataFetcher.
  const base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 1u);
  PadState* pad = fetcher().GetPadState(gamepads.begin()->first);
  std::u16string display_id(pad->data.id);
  EXPECT_EQ(kDefaultDisplayName, display_id);
  CheckGamepadAdded(pad, GamepadHapticActuatorType::kDualRumble);
}

TEST_F(WgiDataFetcherWinTest, VerifyGamepadInput) {
  SetUpTestEnv();

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
  fake_gamepad->SetCurrentReading(kZeroPositionGamepadReading);
  fake_gamepad_with_paddles->SetCurrentReading(kZeroPositionGamepadReading);
  WaitForData(gamepad_buffer);

  fake_gamepad->SetCurrentReading(kGamepadReading);
  fake_gamepad_with_paddles->SetCurrentReading(kGamepadReading);

  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kGamepadReading, output.items[0]);
  CheckGamepadInputResult(kGamepadReading, output.items[1],
                          /*has_paddles*/ true);
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

  // State should be first set to the rest position to satisfy sanitization pre-
  // requisites.
  fake_gamepad->SetCurrentReading(kZeroPositionGamepadReading);

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

  WaitForData(gamepad_buffer);

  wgi_environment_->SimulateError(
      ErrorCode::kErrorWgiGamepadGetCurrentReadingFailed);

  fake_gamepad->SetCurrentReading(kGamepadReading);

  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kZeroPositionGamepadReading, output.items[0]);
}

// If Gamepad::GetButtonLabel fails, the stored gamepad state buttons_length
// property will be equal to 16, even though the connected device may have
// paddles - i.e., the paddles will not be recognized.
TEST_F(WgiDataFetcherWinTest, WgiGamepadGetButtonLabelError) {
  SetUpTestEnv(ErrorCode::kErrorWgiGamepadGetButtonLabelFailed);

  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            WgiDataFetcherWin::InitializationState::kInitialized);

  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();
  const auto fake_gamepad_with_paddles = Microsoft::WRL::Make<FakeIGamepad>();
  fake_gamepad_with_paddles->SetHasPaddles(true);

  // State should be first set to the rest position to satisfy sanitization pre-
  // requisites.
  fake_gamepad_with_paddles->SetCurrentReading(kZeroPositionGamepadReading);

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

  WaitForData(gamepad_buffer);

  fake_gamepad_with_paddles->SetCurrentReading(kGamepadReading);

  WaitForData(gamepad_buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(gamepad_buffer, &output);

  // Get connected gamepad state to verify gamepad input results.
  CheckGamepadInputResult(kGamepadReading, output.items[0]);
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
      public testing::WithParamInterface<ErrorCode> {};

// This test simulates OS errors that prevent the controller from being
// enumerated by WgiDataFetcherWin.
TEST_P(WgiDataFetcherWinErrorTest, GamepadShouldNotbeEnumerated) {
  const ErrorCode error_code = GetParam();
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

}  // namespace device
