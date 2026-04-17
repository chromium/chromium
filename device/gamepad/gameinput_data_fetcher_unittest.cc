// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gameinput_data_fetcher.h"

#include <winerror.h>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/gamepad/gameinput_gamepad_device.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "device/gamepad/test_support/fake_gameinput_environment.h"
#include "device/gamepad/test_support/fake_igameinput.h"
#include "device/gamepad/test_support/fake_igameinputdevice.h"
#include "device/gamepad/test_support/fake_igameinputreading.h"
#include "services/device/device_service_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr double kErrorTolerance = 1e-5;
constexpr double kDurationMillis = 1.0;
constexpr double kZeroStartDelayMillis = 0.0;
constexpr double kStrongMagnitude = 1.0;         // 100% intensity.
constexpr double kWeakMagnitude = 0.5;           // 50% intensity.
constexpr double kLeftTriggerMagnitude = 0.75;   // 75% intensity.
constexpr double kRightTriggerMagnitude = 0.25;  // 25% intensity.

}  // namespace

class GameInputDataFetcherTest : public DeviceServiceTestBase {
 public:
  GameInputDataFetcherTest() = default;
  ~GameInputDataFetcherTest() override = default;

  void SetUpTestEnv() {
    fake_gameinput_env_ = std::make_unique<FakeGameInputEnvironment>();
    SetUpTestEnvCommon(
        base::BindRepeating(&FakeGameInputEnvironment::GameInputCreate));
  }

  void SetUpTestEnvCommon(
      GameInputDataFetcher::CreateGameInputFunction create_override) {
    auto fetcher = std::make_unique<GameInputDataFetcher>();
    gameinput_fetcher_ = fetcher.get();

    fetcher->OverrideGameInputCreationMethodForTesting(
        std::move(create_override));

    // Initialize provider to retrieve pad state.
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_ = std::make_unique<::device::GamepadProvider>(
        /*connection_change_client=*/nullptr, std::move(fetcher),
        std::move(polling_thread));

    FlushPollingThread();
  }

  void FlushPollingThread() { polling_thread_->FlushForTesting(); }

  bool WaitForData(const GamepadHardwareBuffer* buffer) {
    // RunUntil re-checks its condition via idle callbacks, which only fire
    // after the thread processes work. Since the seqlock is updated on the
    // polling thread without posting tasks to the test thread, we use a
    // RepeatingTimer to periodically wake the test thread's RunLoop.
    base::RepeatingTimer wakeup_timer;
    wakeup_timer.Start(FROM_HERE, base::Milliseconds(10),
                       base::BindRepeating([]() {}));

    const base::subtle::Atomic32 initial_version = buffer->seqlock.ReadBegin();
    return base::test::RunUntil([&]() {
      // Wait until the shared memory buffer's seqlock advances the buffer
      // version indicating that the gamepad provider has written to it after
      // polling the gamepad fetchers. The buffer will report an odd value for
      // the version if the buffer is not in a consistent state, so we also
      // require that the value is even before continuing.
      base::subtle::Atomic32 current_version = buffer->seqlock.ReadBegin();
      return current_version % 2 == 0 && current_version != initial_version;
    });
  }

  Gamepads ReadGamepadHardwareBuffer(const GamepadHardwareBuffer* buffer) {
    Gamepads output;
    base::subtle::Atomic32 version;
    do {
      version = buffer->seqlock.ReadBegin();
      output = buffer->data;
    } while (buffer->seqlock.ReadRetry(version));
    return output;
  }

  GameInputDataFetcher& fetcher() const { return *gameinput_fetcher_; }
  FakeIGameInput* mock_gameinput() {
    return fake_gameinput_env_->GetFakeGameInput();
  }

 protected:
  std::unique_ptr<GamepadProvider> provider_;
  std::unique_ptr<FakeGameInputEnvironment> fake_gameinput_env_;

 private:
  raw_ptr<GameInputDataFetcher> gameinput_fetcher_;
  raw_ptr<base::Thread> polling_thread_;
};

TEST_F(GameInputDataFetcherTest, AddAndRemoveGameInputGamepad) {
  SetUpTestEnv();

  // Check initial number of connected gamepad and initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            GameInputDataFetcher::InitializationState::kInitialized);
  const base::flat_map<int, std::unique_ptr<GameInputGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 0u);

  // Simulate adding two gamepads
  const auto fake_gamepad1 = Microsoft::WRL::Make<FakeIGameInputDevice>();
  const auto fake_gamepad2 = Microsoft::WRL::Make<FakeIGameInputDevice>();
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad1.Get(),
                                         GameInputDeviceConnected);
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad2.Get(),
                                         GameInputDeviceConnected);

  // Wait for the gamepad polling thread to handle the gamepad added events.
  FlushPollingThread();

  // Assert that the gamepads have been added to the DataFetcher.
  ASSERT_EQ(gamepads.size(), 2u);

  // Simulate the gamepad remove
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad1.Get(),
                                         GameInputDeviceNoStatus);
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad2.Get(),
                                         GameInputDeviceNoStatus);

  // Wait for the gamepad polling thread to handle the gamepad removed event.
  FlushPollingThread();

  ASSERT_EQ(gamepads.size(), 0u);
}

TEST_F(GameInputDataFetcherTest, VerifyGamepadReading) {
  SetUpTestEnv();

  // Check initial number of connected gamepad and initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            GameInputDataFetcher::InitializationState::kInitialized);
  const base::flat_map<int, std::unique_ptr<GameInputGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 0u);

  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGameInputDevice>();
  const auto reading_empty = Microsoft::WRL::Make<FakeIGameInputReading>();

  // Create a reading with a gamepad state where some buttons are pressed:
  GameInputGamepadState pressedState = {};
  pressedState.buttons = GameInputGamepadA | GameInputGamepadY |
                         GameInputGamepadRightShoulder |
                         GameInputGamepadDPadDown;
  pressedState.leftThumbstickX = 0.65f;
  pressedState.rightThumbstickY = -0.22f;
  pressedState.leftTrigger = 0.8f;
  pressedState.rightTrigger = 0.05f;
  const auto reading_setkeys =
      Microsoft::WRL::Make<FakeIGameInputReading>(pressedState);

  // Simulate adding a gamepad
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceConnected);

  // Wait for the gamepad polling thread to handle the gamepad added events.
  FlushPollingThread();

  // Assert that the gamepad has been added to the DataFetcher.
  ASSERT_EQ(gamepads.size(), 1u);

  // Send a new reading with the pressed keys
  provider_->Resume();

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      provider_->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  ASSERT_TRUE(shared_memory_mapping.IsValid());
  const GamepadHardwareBuffer* gamepad_buffer =
      static_cast<const GamepadHardwareBuffer*>(shared_memory_mapping.memory());

  mock_gameinput()->AssignMockReading(reading_empty.Get());

  ASSERT_TRUE(WaitForData(gamepad_buffer));

  mock_gameinput()->AssignMockReading(reading_setkeys.Get());
  mock_gameinput()->InvokeSystemButtonCallback(fake_gamepad.Get(),
                                               /*is_pressed=*/true);
  // Flush the polling thread to ensure the posted guide button callback is
  // processed before waiting for the next data write.
  FlushPollingThread();

  ASSERT_TRUE(WaitForData(gamepad_buffer));

  Gamepads output = ReadGamepadHardwareBuffer(gamepad_buffer);

  // Verify expected buttons are pressed
  EXPECT_EQ(output.items[0].buttons_length, BUTTON_INDEX_COUNT);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_PRIMARY].pressed);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_QUATERNARY].pressed);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_RIGHT_SHOULDER].pressed);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_DPAD_DOWN].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_SECONDARY].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_DPAD_LEFT].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_LEFT_SHOULDER].pressed);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_META].pressed);

  // Verify trigger buttons: left trigger (0.8) should be pressed (above
  // threshold), right trigger (0.05) should not be pressed but should be
  // touched.
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed);
  EXPECT_NEAR(output.items[0].buttons[BUTTON_INDEX_LEFT_TRIGGER].value, 0.8f,
              kErrorTolerance);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_LEFT_TRIGGER].touched);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed);
  EXPECT_NEAR(output.items[0].buttons[BUTTON_INDEX_RIGHT_TRIGGER].value, 0.05f,
              kErrorTolerance);
  EXPECT_TRUE(output.items[0].buttons[BUTTON_INDEX_RIGHT_TRIGGER].touched);

  // Verify axis including the relative inversion of the Y direction
  EXPECT_NEAR(output.items[0].axes[AXIS_INDEX_LEFT_STICK_X], 0.65f,
              kErrorTolerance);
  EXPECT_NEAR(output.items[0].axes[AXIS_INDEX_RIGHT_STICK_Y], 0.22f,
              kErrorTolerance);

  // Reapply a empty reading
  mock_gameinput()->AssignMockReading(reading_empty.Get());
  mock_gameinput()->InvokeSystemButtonCallback(fake_gamepad.Get(),
                                               /*is_pressed=*/false);
  // Flush the polling thread to ensure the posted guide button callback is
  // processed before waiting for the next data write.
  FlushPollingThread();

  ASSERT_TRUE(WaitForData(gamepad_buffer));
  output = ReadGamepadHardwareBuffer(gamepad_buffer);

  // Verify everything is released
  EXPECT_EQ(output.items[0].buttons_length, BUTTON_INDEX_COUNT);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_PRIMARY].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_QUATERNARY].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_RIGHT_SHOULDER].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_DPAD_DOWN].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_SECONDARY].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_DPAD_LEFT].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_LEFT_SHOULDER].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_META].pressed);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed);
  EXPECT_NEAR(output.items[0].buttons[BUTTON_INDEX_LEFT_TRIGGER].value, 0.0f,
              kErrorTolerance);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_LEFT_TRIGGER].touched);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed);
  EXPECT_NEAR(output.items[0].buttons[BUTTON_INDEX_RIGHT_TRIGGER].value, 0.0f,
              kErrorTolerance);
  EXPECT_FALSE(output.items[0].buttons[BUTTON_INDEX_RIGHT_TRIGGER].touched);
  EXPECT_NEAR(output.items[0].axes[AXIS_INDEX_LEFT_STICK_X], 0,
              kErrorTolerance);
  EXPECT_NEAR(output.items[0].axes[AXIS_INDEX_RIGHT_STICK_Y], 0,
              kErrorTolerance);

  // Simulate the gamepad remove
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceNoStatus);

  // Wait for the gamepad polling thread to handle the gamepad removed event.
  FlushPollingThread();

  ASSERT_EQ(gamepads.size(), 0u);
}

TEST_F(GameInputDataFetcherTest, VerifyControllerIdNames) {
  SetUpTestEnv();

  EXPECT_EQ(fetcher().GetInitializationState(),
            GameInputDataFetcher::InitializationState::kInitialized);
  const base::flat_map<int, std::unique_ptr<GameInputGamepadDevice>>& gamepads =
      fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 0u);

  const auto [vendor_id_0, product_id_0] =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(
          GamepadId::kMicrosoftProduct0b21);
  const auto fake_gamepad_0 =
      Microsoft::WRL::Make<FakeIGameInputDevice>(vendor_id_0, product_id_0);
  const auto [vendor_id_1, product_id_1] =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(
          GamepadId::kMicrosoftProduct028f);
  const auto fake_gamepad_1 =
      Microsoft::WRL::Make<FakeIGameInputDevice>(vendor_id_1, product_id_1);
  const auto [vendor_id_2, product_id_2] =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(
          GamepadId::kMicrosoftProduct0b05);
  const auto fake_gamepad_2 =
      Microsoft::WRL::Make<FakeIGameInputDevice>(vendor_id_2, product_id_2);
  // Use default vid/pid (0x045e, 0x02ea) which is kXInputTypeXboxOne — this
  // should get the known XInput device ID string.
  const auto fake_gamepad_3 = Microsoft::WRL::Make<FakeIGameInputDevice>();

  provider_->Resume();

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      provider_->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  ASSERT_TRUE(shared_memory_mapping.IsValid());
  const GamepadHardwareBuffer* gamepad_buffer =
      static_cast<const GamepadHardwareBuffer*>(shared_memory_mapping.memory());

  // Add gamepads
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad_0.Get(),
                                         GameInputDeviceConnected);
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad_1.Get(),
                                         GameInputDeviceConnected);
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad_2.Get(),
                                         GameInputDeviceConnected);
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad_3.Get(),
                                         GameInputDeviceConnected);

  // Wait for the gamepad polling thread to handle the gamepad added events.
  FlushPollingThread();
  ASSERT_EQ(gamepads.size(), 4u);
  ASSERT_TRUE(WaitForData(gamepad_buffer));
  Gamepads output = ReadGamepadHardwareBuffer(gamepad_buffer);

  // Verify all ids
  const std::u16string known_id_0(
      u"Unknown Gamepad (STANDARD GAMEPAD Vendor: 045e Product: 0b21)");
  const std::u16string known_id_1(
      u"Unknown Gamepad (STANDARD GAMEPAD Vendor: 045e Product: 028f)");
  const std::u16string known_id_2(
      u"Unknown Gamepad (STANDARD GAMEPAD Vendor: 045e Product: 0b05)");
  // Known XInput device should use the standard XInput ID string.
  const std::u16string known_id_3(
      u"Xbox 360 Controller (XInput STANDARD GAMEPAD)");

  EXPECT_EQ(std::u16string(
                reinterpret_cast<const char16_t*>(output.items[0].id.data())),
            known_id_0);
  EXPECT_EQ(std::u16string(
                reinterpret_cast<const char16_t*>(output.items[1].id.data())),
            known_id_1);
  EXPECT_EQ(std::u16string(
                reinterpret_cast<const char16_t*>(output.items[2].id.data())),
            known_id_2);
  EXPECT_EQ(std::u16string(
                reinterpret_cast<const char16_t*>(output.items[3].id.data())),
            known_id_3);
}

struct InitializationFailureParam {
  GameInputTestErrorCode error_code;
  GameInputDataFetcher::InitializationState expected_state;
};

const InitializationFailureParam kInitializationFailures[] = {
    {GameInputTestErrorCode::kGetProcAddressFailed,
     GameInputDataFetcher::InitializationState::kGetProcAddressFailed},
    {GameInputTestErrorCode::kGameInputCreateFailed,
     GameInputDataFetcher::InitializationState::kCreateGameInputFailed},
    {GameInputTestErrorCode::kDeviceCallbackRegistrationFailed,
     GameInputDataFetcher::InitializationState::kFailedDeviceEnumeration},
    {GameInputTestErrorCode::kGuideButtonCallbackRegistrationFailed,
     GameInputDataFetcher::InitializationState::
         kFailedGuideButtonCallbackRegistration},
};

class GameInputInitializationFailureTest
    : public GameInputDataFetcherTest,
      public testing::WithParamInterface<InitializationFailureParam> {};

TEST_P(GameInputInitializationFailureTest, VerifyInitializationFailure) {
  const auto& param = GetParam();
  fake_gameinput_env_ =
      std::make_unique<FakeGameInputEnvironment>(param.error_code);
  GameInputDataFetcher::CreateGameInputFunction create_fn;
  if (param.error_code != GameInputTestErrorCode::kGetProcAddressFailed) {
    create_fn = base::BindRepeating(&FakeGameInputEnvironment::GameInputCreate);
  }
  SetUpTestEnvCommon(std::move(create_fn));
  EXPECT_EQ(fetcher().GetInitializationState(), param.expected_state);
}

INSTANTIATE_TEST_SUITE_P(GameInputInitializationFailures,
                         GameInputInitializationFailureTest,
                         testing::ValuesIn(kInitializationFailures));

// Test that a gamepad with trigger-rumble motors is detected as having
// trigger-rumble support.
TEST_F(GameInputDataFetcherTest, GamepadShouldHaveTriggerRumbleSupport) {
  SetUpTestEnv();
  const auto [vendor_id, product_id] =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(
          GamepadId::kMicrosoftProduct0b20);
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGameInputDevice>(
      vendor_id, product_id,
      static_cast<GameInputRumbleMotors>(GameInputRumbleLeftTrigger |
                                         GameInputRumbleRightTrigger));

  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceConnected);
  FlushPollingThread();

  const auto& gamepads = fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 1u);
  PadState* pad_state = fetcher().GetPadState(gamepads.begin()->first);
  ASSERT_TRUE(pad_state);
  EXPECT_TRUE(pad_state->is_initialized);
  EXPECT_TRUE(pad_state->data.vibration_actuator.not_null);
  EXPECT_EQ(pad_state->data.vibration_actuator.type,
            GamepadHapticActuatorType::kTriggerRumble);
}

// Test that a gamepad without trigger-rumble motors falls back to dual-rumble.
TEST_F(GameInputDataFetcherTest, GamepadShouldHaveDualRumbleSupport) {
  SetUpTestEnv();
  const auto [vendor_id, product_id] =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(
          GamepadId::kMicrosoftProduct0b20);
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGameInputDevice>(
      vendor_id, product_id, GameInputRumbleNone);

  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceConnected);
  FlushPollingThread();

  const auto& gamepads = fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 1u);
  PadState* pad_state = fetcher().GetPadState(gamepads.begin()->first);
  ASSERT_TRUE(pad_state);
  EXPECT_TRUE(pad_state->is_initialized);
  EXPECT_TRUE(pad_state->data.vibration_actuator.not_null);
  EXPECT_EQ(pad_state->data.vibration_actuator.type,
            GamepadHapticActuatorType::kDualRumble);
}

TEST_F(GameInputDataFetcherTest, ShouldNotEnumerateGamepads) {
  // Controllers that should not be enumerated by the GameInput data fetcher.
  const GamepadId kShouldNotEnumerateGamepads[] = {
      // Nintendo controllers are handled by the Nintendo data fetcher.
      GamepadId::kNintendoProduct2006,
      GamepadId::kNintendoProduct2007,
      GamepadId::kNintendoProduct2009,
      GamepadId::kNintendoProduct200e,
      // DualShock 4 controllers are handled by the RawInput data fetcher.
      GamepadId::kSonyProduct05c4,
      GamepadId::kSonyProduct09cc,
      // DualSense controllers are handled by the RawInput data fetcher.
      GamepadId::kSonyProduct0ce6,
      GamepadId::kSonyProduct0df2,
  };

  SetUpTestEnv();
  const auto& gamepads = fetcher().GetGamepadsForTesting();

  for (const GamepadId& gamepad_id : kShouldNotEnumerateGamepads) {
    auto [vendor_id, product_id] =
        GamepadIdList::Get().GetDeviceIdsFromGamepadId(gamepad_id);
    const auto fake_gamepad =
        Microsoft::WRL::Make<FakeIGameInputDevice>(vendor_id, product_id);

    mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                           GameInputDeviceConnected);
    FlushPollingThread();

    EXPECT_EQ(gamepads.size(), 0u);
  }
}

// Test that if we can't obtain the gamepad's device info from with GameInput,
// the gamepad won't be enumerated.
TEST_F(GameInputDataFetcherTest, NoDeviceInfoGamepadNotEnumerated) {
  SetUpTestEnv();
  EXPECT_EQ(fetcher().GetInitializationState(),
            GameInputDataFetcher::InitializationState::kInitialized);

  const auto& gamepads = fetcher().GetGamepadsForTesting();
  ASSERT_EQ(gamepads.size(), 0u);

  fake_gameinput_env_->SimulateError(GameInputTestErrorCode::kNoDeviceInfo);

  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGameInputDevice>();
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceConnected);
  FlushPollingThread();

  EXPECT_EQ(gamepads.size(), 0u);
}

enum class HapticsType { kDualRumble, kTriggerRumble };

class GameInputHapticsTest : public GameInputDataFetcherTest,
                             public testing::WithParamInterface<HapticsType> {
 protected:
  void HapticsCallback(mojom::GamepadHapticsResult result) {
    haptics_callback_count_++;
    haptics_callback_result_ = result;
  }

  void PlayVibrationEffect(int pad_index,
                           mojom::GamepadHapticEffectType type,
                           mojom::GamepadEffectParametersPtr params) {
    base::test::TestFuture<void> future;
    provider_->PlayVibrationEffectOnce(
        pad_index, type, std::move(params),
        base::BindOnce(&GameInputHapticsTest::HapticsCallback,
                       base::Unretained(this))
            .Then(base::BindPostTask(
                base::SequencedTaskRunner::GetCurrentDefault(),
                future.GetCallback())));
    FlushPollingThread();
    EXPECT_TRUE(future.Wait());
  }

  void SimulateDualRumbleEffect(int pad_index) {
    PlayVibrationEffect(
        pad_index,
        mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
        mojom::GamepadEffectParameters::New(
            kDurationMillis, kZeroStartDelayMillis, kStrongMagnitude,
            kWeakMagnitude, /*left_trigger=*/0, /*right_trigger=*/0));
  }

  void SimulateTriggerRumbleEffect(int pad_index) {
    PlayVibrationEffect(
        pad_index,
        mojom::GamepadHapticEffectType::GamepadHapticEffectTypeTriggerRumble,
        mojom::GamepadEffectParameters::New(
            kDurationMillis, kZeroStartDelayMillis, kStrongMagnitude,
            kWeakMagnitude, kLeftTriggerMagnitude, kRightTriggerMagnitude));
  }

  void SimulateResetVibration(int pad_index) {
    base::test::TestFuture<void> future;
    provider_->ResetVibrationActuator(
        pad_index, base::BindOnce(&GameInputHapticsTest::HapticsCallback,
                                  base::Unretained(this))
                       .Then(base::BindPostTask(
                           base::SequencedTaskRunner::GetCurrentDefault(),
                           future.GetCallback())));
    FlushPollingThread();
    CHECK(future.Wait());
  }

  void SimulateVibrationEffect(int pad_index) {
    switch (GetParam()) {
      case HapticsType::kDualRumble:
        SimulateDualRumbleEffect(pad_index);
        break;
      case HapticsType::kTriggerRumble:
        SimulateTriggerRumbleEffect(pad_index);
        break;
    }
  }

  int haptics_callback_count_ = 0;
  mojom::GamepadHapticsResult haptics_callback_result_ =
      mojom::GamepadHapticsResult::GamepadHapticsResultError;
};

TEST_P(GameInputHapticsTest, VibrationEffect) {
  SetUpTestEnv();
  EXPECT_EQ(fetcher().GetInitializationState(),
            GameInputDataFetcher::InitializationState::kInitialized);
  const auto& gamepads = fetcher().GetGamepadsForTesting();

  // Simulate adding a gamepad with trigger rumble support.
  const auto [vendor_id, product_id] =
      GamepadIdList::Get().GetDeviceIdsFromGamepadId(
          GamepadId::kMicrosoftProduct0b20);
  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGameInputDevice>(
      vendor_id, product_id,
      static_cast<GameInputRumbleMotors>(GameInputRumbleLeftTrigger |
                                         GameInputRumbleRightTrigger));
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceConnected);
  FlushPollingThread();
  ASSERT_EQ(gamepads.size(), 1u);

  provider_->Resume();

  SimulateVibrationEffect(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 1);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultComplete);

  // Calling ResetVibration sets the vibration intensity to 0 for all motors.
  SimulateResetVibration(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 2);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultComplete);

  // Remove the gamepad and verify haptics return NotSupported.
  mock_gameinput()->InvokeDeviceCallback(fake_gamepad.Get(),
                                         GameInputDeviceNoStatus);
  FlushPollingThread();

  SimulateVibrationEffect(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 3);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
  SimulateResetVibration(/*pad_index=*/0);
  EXPECT_EQ(haptics_callback_count_, 4);
  EXPECT_EQ(haptics_callback_result_,
            mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
}

INSTANTIATE_TEST_SUITE_P(GameInputHaptics,
                         GameInputHapticsTest,
                         testing::Values(HapticsType::kDualRumble,
                                         HapticsType::kTriggerRumble),
                         [](const testing::TestParamInfo<HapticsType>& info) {
                           switch (info.param) {
                             case HapticsType::kDualRumble:
                               return "DualRumble";
                             case HapticsType::kTriggerRumble:
                               return "TriggerRumble";
                           }
                         });

}  // namespace device
