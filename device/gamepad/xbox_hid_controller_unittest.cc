// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/xbox_hid_controller.h"

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

constexpr size_t kReportLength = 9;

constexpr uint8_t kStopVibration[] = {0x03,  // report ID
                                      0x03, 0x00, 0x00,
                                      0x00,  // strong magnitude
                                      0x00,  // weak magnitude
                                      0xff, 0x00, 0x01};
static_assert(sizeof(kStopVibration) == kReportLength,
              "kStopVibration has incorrect size");

constexpr uint8_t kStartVibration[] = {0x03,  // report ID
                                       0x03, 0x00, 0x00,
                                       0xff,  // strong magnitude
                                       0x7f,  // weak magnitude
                                       0xff, 0x00, 0x01};
static_assert(sizeof(kStartVibration) == kReportLength,
              "kStartVibration has incorrect size");

// Use 1 ms for all non-zero effect durations. There is no reason to test longer
// delays as they will be skipped anyway.
constexpr double kDurationMillis = 1.0;
// Setting |start_delay| to zero can cause additional reports to be sent.
constexpr double kZeroStartDelayMillis = 0.0;
// Vibration magnitudes for the strong and weak channels of a typical
// dual-rumble vibration effect. kStartVibrationData describes a report with
// these magnitudes.
constexpr double kStrongMagnitude = 1.0;  // 100% intensity
constexpr double kWeakMagnitude = 0.5;    // 50% intensity

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
class XboxHidControllerTest : public testing::Test {
 public:
  XboxHidControllerTest()
      : start_vibration_report_(kStartVibration,
                                kStartVibration + kReportLength),
        stop_vibration_report_(kStopVibration, kStopVibration + kReportLength),
        callback_count_(0),
        callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError) {
    auto fake_hid_writer = std::make_unique<FakeHidWriter>();
    fake_hid_writer_ = fake_hid_writer.get();
    gamepad_ = std::make_unique<XboxHidController>(std::move(fake_hid_writer));
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

  // Callback for PlayEffect or ResetVibration.
  void Callback(mojom::GamepadHapticsResult result) {
    callback_count_++;
    callback_result_ = result;
  }

  const std::vector<uint8_t> start_vibration_report_;
  const std::vector<uint8_t> stop_vibration_report_;
  int callback_count_;
  mojom::GamepadHapticsResult callback_result_;
  FakeHidWriter* fake_hid_writer_;
  std::unique_ptr<XboxHidController> gamepad_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  DISALLOW_COPY_AND_ASSIGN(XboxHidControllerTest);
};

TEST_F(XboxHidControllerTest, PlayEffect) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, callback_count_);

  PostPlayEffect(
      kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
      base::BindOnce(&XboxHidControllerTest::Callback, base::Unretained(this)));

  // Run the queued task and start vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(start_vibration_report_));
  EXPECT_EQ(0, callback_count_);
  EXPECT_LT(0U, task_environment_.GetPendingMainThreadTaskCount());

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(start_vibration_report_));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            callback_result_);
  EXPECT_EQ(0U, task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(XboxHidControllerTest, ResetVibration) {
  EXPECT_TRUE(fake_hid_writer_->output_reports.empty());
  EXPECT_EQ(0, callback_count_);

  PostResetVibration(
      base::BindOnce(&XboxHidControllerTest::Callback, base::Unretained(this)));

  // Run the queued task and reset vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_hid_writer_->output_reports,
              testing::ElementsAre(stop_vibration_report_));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            callback_result_);
  EXPECT_EQ(0U, task_environment_.GetPendingMainThreadTaskCount());
}

}  // namespace

}  // namespace device
