// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/abstract_haptic_gamepad.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Use 1 ms for all non-zero effect durations. There is no reason to test longer
// delays as they will be skipped anyway.
constexpr double kDurationMillis = 1.0;
constexpr double kNonZeroStartDelayMillis = 1.0;
// Setting |start_delay| to zero can cause additional reports to be sent.
constexpr double kZeroStartDelayMillis = 0.0;
// Vibration magnitudes for the strong and weak channels of a typical
// dual-rumble vibration effect.
constexpr double kStrongMagnitude = 1.0;  // 100% intensity
constexpr double kWeakMagnitude = 0.5;    // 50% intensity

constexpr base::TimeDelta kPendingTaskDuration =
    base::TimeDelta::FromMillisecondsD(kDurationMillis);

// An implementation of AbstractHapticGamepad that records how many times its
// SetVibration and SetZeroVibration methods have been called.
class FakeHapticGamepad final : public AbstractHapticGamepad {
 public:
  FakeHapticGamepad() : set_vibration_count_(0), set_zero_vibration_count_(0) {}
  ~FakeHapticGamepad() override = default;

  void SetVibration(double strong_magnitude, double weak_magnitude) override {
    set_vibration_count_++;
  }

  void SetZeroVibration() override { set_zero_vibration_count_++; }

  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  int set_vibration_count_;
  int set_zero_vibration_count_;
  base::WeakPtrFactory<FakeHapticGamepad> weak_factory_{this};
};

// Main test fixture
class AbstractHapticGamepadTest : public testing::Test {
 public:
  AbstractHapticGamepadTest()
      : first_callback_count_(0),
        second_callback_count_(0),
        first_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        second_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        gamepad_(std::make_unique<FakeHapticGamepad>()) {}

  void TearDown() override { gamepad_->Shutdown(); }

  void PostPlayEffect(
      mojom::GamepadHapticEffectType type,
      double duration,
      double start_delay,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
    gamepad_->PlayEffect(
        type,
        mojom::GamepadEffectParameters::New(duration, start_delay,
                                            kStrongMagnitude, kWeakMagnitude),
        std::move(callback), base::ThreadTaskRunnerHandle::Get());
  }

  void PostResetVibration(
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
    gamepad_->ResetVibration(std::move(callback),
                             base::ThreadTaskRunnerHandle::Get());
  }

  // Callback for the first PlayEffect or ResetVibration call in a test.
  void FirstCallback(mojom::GamepadHapticsResult result) {
    first_callback_count_++;
    first_callback_result_ = result;
  }

  // Callback for the second PlayEffect or ResetVibration call in a test. Use
  // this when multiple callbacks may be received and the test should check the
  // result codes for each.
  void SecondCallback(mojom::GamepadHapticsResult result) {
    second_callback_count_++;
    second_callback_result_ = result;
  }

  int first_callback_count_;
  int second_callback_count_;
  mojom::GamepadHapticsResult first_callback_result_;
  mojom::GamepadHapticsResult second_callback_result_;
  std::unique_ptr<FakeHapticGamepad> gamepad_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  DISALLOW_COPY_AND_ASSIGN(AbstractHapticGamepadTest);
};

TEST_F(AbstractHapticGamepadTest, PlayEffectTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);

  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                     base::Unretained(this)));

  // Run the queued task to start the effect.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  // SetZeroVibration is not called. Typically, the renderer would issue a call
  // to SetZeroVibration once the callback receives a success result.
  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(AbstractHapticGamepadTest, ResetVibrationTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);

  PostResetVibration(base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                                    base::Unretained(this)));

  // Run the queued task to reset vibration.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(AbstractHapticGamepadTest, UnsupportedEffectTypeTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);

  mojom::GamepadHapticEffectType unsupported_effect_type =
      static_cast<mojom::GamepadHapticEffectType>(123);
  PostPlayEffect(unsupported_effect_type, kDurationMillis,
                 kZeroStartDelayMillis,
                 base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Run the queued task to start the effect.
  task_environment_.RunUntilIdle();

  // An unsupported effect should return a "not-supported" result without
  // calling SetVibration or SetZeroVibration.
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(AbstractHapticGamepadTest, StartDelayTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);

  // Issue PlayEffect with non-zero |start_delay|.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kNonZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                     base::Unretained(this)));

  // Run the queued task to start the effect.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Start vibration.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(AbstractHapticGamepadTest, ZeroStartDelayPreemptionTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                     base::Unretained(this)));

  // Start a second effect with zero |start_delay|. This should cause the first
  // effect to be preempted before it calls SetVibration.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::SecondCallback,
                     base::Unretained(this)));

  // Run the queued task to start the effect.
  task_environment_.RunUntilIdle();

  // The first effect should have already returned with a "preempted" result.
  // The second effect should have started vibration.
  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  // Now the second effect should have returned with a "complete" result.
  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(AbstractHapticGamepadTest, NonZeroStartDelayPreemptionTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                     base::Unretained(this)));

  // Start a second effect with non-zero |start_delay|. This should cause the
  // first effect to be preempted before it calls SetVibration.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kNonZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::SecondCallback,
                     base::Unretained(this)));

  // Run the queued tasks.
  task_environment_.RunUntilIdle();

  // The first effect should have already returned with a "preempted" result.
  // Because the second effect has a non-zero |start_delay|, it will call
  // SetZeroVibration to ensure no vibration occurs during the delay.
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Start vibration.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_EQ(1, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(AbstractHapticGamepadTest, ResetVibrationPreemptionTest) {
  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(0, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt it with a reset.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      kDurationMillis, kZeroStartDelayMillis,
      base::BindOnce(&AbstractHapticGamepadTest::FirstCallback,
                     base::Unretained(this)));

  // Reset vibration. This should cause the effect to be preempted before it
  // calls SetVibration.
  PostResetVibration(base::BindOnce(&AbstractHapticGamepadTest::SecondCallback,
                                    base::Unretained(this)));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, gamepad_->set_vibration_count_);
  EXPECT_EQ(1, gamepad_->set_zero_vibration_count_);
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

}  // namespace

}  // namespace device
