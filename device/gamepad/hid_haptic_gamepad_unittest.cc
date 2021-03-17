// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/gamepad/hid_writer.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Describes a hypothetical HID haptic gamepad with two 16-bit vibration
// magnitude fields.
constexpr uint8_t kReportId = 0x42;
constexpr size_t kReportLength = 5;
constexpr HidHapticGamepad::HapticReportData kHapticReportData = {
    0x1234, 0xabcd, kReportId, kReportLength, 1, 3, 16, 0, 0xffff};

// The expected "stop vibration" report bytes (zero vibration).
constexpr uint8_t kStopVibrationData[] = {kReportId, 0x00, 0x00, 0x00, 0x00};
// The expected "start vibration" report bytes for an effect with 100% intensity
// on the "strong" vibration channel and 50% intensity on the "weak" vibration
// channel.
constexpr uint8_t kStartVibrationData[] = {kReportId, 0xff, 0xff, 0xff, 0x7f};

// Use 1 ms for all non-zero effect durations. There is no reason to test longer
// delays as they will be skipped anyway.
constexpr double kDurationMillis = 1.0;
constexpr double kNonZeroStartDelayMillis = 1.0;
// Setting |start_delay| to zero can cause additional reports to be sent.
constexpr double kZeroStartDelayMillis = 0.0;
// Vibration magnitudes for the strong and weak channels of a typical
// dual-rumble vibration effect. kStartVibrationData describes a report with
// these magnitudes.
constexpr double kStrongMagnitude = 1.0;  // 100% intensity
constexpr double kWeakMagnitude = 0.5;    // 50% intensity
// Zero vibration magnitude. kStopVibrationData describes a report with zero
// vibration on both channels.
constexpr double kZeroMagnitude = 0.0;

constexpr base::TimeDelta kPendingTaskDuration =
    base::TimeDelta::FromMillisecondsD(kDurationMillis);

class FakeHidWriter : public HidWriter {
 public:
  FakeHidWriter() = default;
  ~FakeHidWriter() override = default;

  // HidWriter implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override {
    output_reports.emplace_back(report.begin(), report.end());
    return report.size_bytes();
  }

  std::vector<std::vector<uint8_t>> output_reports;
};

// Main test fixture
class HidHapticGamepadTest : public testing::Test {
 public:
  HidHapticGamepadTest()
      : start_vibration_output_report_(kStartVibrationData,
                                       kStartVibrationData + kReportLength),
        stop_vibration_output_report_(kStopVibrationData,
                                      kStopVibrationData + kReportLength),
        first_callback_count_(0),
        second_callback_count_(0),
        first_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        second_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError) {
    auto fake_hid_writer = std::make_unique<FakeHidWriter>();
    fake_hid_writer_ = fake_hid_writer.get();
    gamepad_ = std::make_unique<HidHapticGamepad>(kHapticReportData,
                                                  std::move(fake_hid_writer));
  }

  void TearDown() override { gamepad_->Shutdown(); }

  void PostPlayEffect(
      double start_delay,
      double strong_magnitude,
      double weak_magnitude,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
    gamepad_->PlayEffect(
        mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
        mojom::GamepadEffectParameters::New(kDurationMillis, start_delay,
                                            strong_magnitude, weak_magnitude),
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

  const std::vector<uint8_t> start_vibration_output_report_;
  const std::vector<uint8_t> stop_vibration_output_report_;
  int first_callback_count_;
  int second_callback_count_;
  mojom::GamepadHapticsResult first_callback_result_;
  mojom::GamepadHapticsResult second_callback_result_;
  FakeHidWriter* fake_hid_writer_;
  std::unique_ptr<HidHapticGamepad> gamepad_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  DISALLOW_COPY_AND_ASSIGN(HidHapticGamepadTest);
};

TEST_F(HidHapticGamepadTest, PlayEffectTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Run the queued task and start vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadTest, ResetVibrationTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostResetVibration(base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                    base::Unretained(this)));

  // Run the queued task and reset vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadTest, ZeroVibrationTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostPlayEffect(kZeroStartDelayMillis, kZeroMagnitude, kZeroMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Run the queued task.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadTest, StartDelayTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);

  // Issue PlayEffect with non-zero |start_delay|.
  PostPlayEffect(kNonZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Stop vibration for the delay period.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Start vibration.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadTest, ZeroStartDelayPreemptionTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Start a second effect with zero |start_delay|. This should cause the first
  // effect to be preempted before it calls SetVibration.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::SecondCallback,
                                base::Unretained(this)));

  // Execute the pending tasks.
  task_environment_.RunUntilIdle();

  // The first effect should have already returned with a "preempted" result
  // without issuing a report. The second effect has started vibration.
  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadTest, NonZeroStartDelayPreemptionTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Start a second effect with non-zero |start_delay|. This should cause the
  // first effect to be preempted before it calls SetVibration.
  PostPlayEffect(kNonZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::SecondCallback,
                                base::Unretained(this)));

  // Execute the pending tasks.
  task_environment_.RunUntilIdle();

  // The first effect should have already returned with a "preempted" result.
  // Because the second effect has a non-zero |start_delay| and is preempting
  // another effect, it will call SetZeroVibration to ensure no vibration
  // occurs during its |start_delay| period.
  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Start vibration.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadTest, ResetVibrationPreemptionTest) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt it with a reset.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadTest::FirstCallback,
                                base::Unretained(this)));

  // Reset vibration. This should cause the effect to be preempted before it
  // calls SetVibration.
  PostResetVibration(base::BindOnce(&HidHapticGamepadTest::SecondCallback,
                                    base::Unretained(this)));

  // Execute the pending tasks.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_output_report_));
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
