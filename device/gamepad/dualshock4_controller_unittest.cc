// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "device/gamepad/hid_writer.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr size_t kUsbReportLength = 32;
constexpr size_t kBluetoothReportLength = 78;

constexpr uint8_t kUsbStopVibration[] = {
    0x05,  // report ID
    0x01, 0x00, 0x00,
    0x00,  // weak magnitude
    0x00,  // strong magnitude
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static_assert(sizeof(kUsbStopVibration) == kUsbReportLength,
              "kUsbStopVibration has incorrect size");

constexpr uint8_t kUsbStartVibration[] = {
    0x05,  // report ID
    0x01, 0x00, 0x00,
    0x80,  // weak magnitude
    0xff,  // strong magnitude
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static_assert(sizeof(kUsbStartVibration) == kUsbReportLength,
              "kUsbStartVibration has incorrect size");

constexpr uint8_t kBtStopVibration[] = {
    0x11,  // report ID
    0xc0, 0x20, 0xf1, 0x04, 0x00,
    0x00,  // weak magnitude
    0x00,  // strong magnitude
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x43, 0x43, 0x00, 0x4d, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // CRC32
    0x30, 0x50, 0xad, 0x07};
static_assert(sizeof(kBtStopVibration) == kBluetoothReportLength,
              "kBtStopVibration has incorrect size");

constexpr uint8_t kBtStartVibration[] = {
    0x11,  // report ID
    0xc0, 0x20, 0xf1, 0x04, 0x00,
    0x80,  // weak magnitude
    0xff,  // strong magnitude
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x43, 0x43, 0x00, 0x4d, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // CRC32
    0x1e, 0x67, 0x45, 0xb4};
static_assert(sizeof(kBtStartVibration) == kBluetoothReportLength,
              "kBtStartVibration has incorrect size");

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
    base::Milliseconds(kDurationMillis);

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
class Dualshock4ControllerTest : public testing::Test {
 public:
  Dualshock4ControllerTest()
      : usb_start_vibration_report_(kUsbStartVibration,
                                    kUsbStartVibration + kUsbReportLength),
        usb_stop_vibration_report_(kUsbStopVibration,
                                   kUsbStopVibration + kUsbReportLength),
        bluetooth_start_vibration_report_(
            kBtStartVibration,
            kBtStartVibration + kBluetoothReportLength),
        bluetooth_stop_vibration_report_(
            kBtStopVibration,
            kBtStopVibration + kBluetoothReportLength),
        callback_count_(0),
        callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError) {
    // Create two Dualshock4Controller instances, one configured for USB and one
    // for Bluetooth.
    auto usb_writer = std::make_unique<FakeHidWriter>();
    usb_writer_ = usb_writer.get();
    ds4_usb_ = std::make_unique<Dualshock4Controller>(
        GamepadId::kSonyProduct05c4, GAMEPAD_BUS_USB, std::move(usb_writer));

    auto bluetooth_writer = std::make_unique<FakeHidWriter>();
    bluetooth_writer_ = bluetooth_writer.get();
    ds4_bluetooth_ = std::make_unique<Dualshock4Controller>(
        GamepadId::kSonyProduct05c4, GAMEPAD_BUS_BLUETOOTH,
        std::move(bluetooth_writer));
  }

  Dualshock4ControllerTest(const Dualshock4ControllerTest&) = delete;
  Dualshock4ControllerTest& operator=(const Dualshock4ControllerTest&) = delete;

  void TearDown() override {
    ds4_usb_->Shutdown();
    ds4_bluetooth_->Shutdown();
  }

  void PostPlayEffect(
      Dualshock4Controller* gamepad,
      double start_delay,
      double strong_magnitude,
      double weak_magnitude,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
    gamepad->PlayEffect(
        mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
        mojom::GamepadEffectParameters::New(
            kDurationMillis, start_delay, strong_magnitude, weak_magnitude,
            /*left_trigger=*/0, /*right_trigger=*/0),
        std::move(callback), base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void PostResetVibration(
      Dualshock4Controller* gamepad,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
    gamepad->ResetVibration(std::move(callback),
                            base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  // Callback for PlayEffect or ResetVibration.
  void Callback(mojom::GamepadHapticsResult result) {
    callback_count_++;
    callback_result_ = result;
  }

  const std::vector<uint8_t> usb_start_vibration_report_;
  const std::vector<uint8_t> usb_stop_vibration_report_;
  const std::vector<uint8_t> bluetooth_start_vibration_report_;
  const std::vector<uint8_t> bluetooth_stop_vibration_report_;
  int callback_count_;
  mojom::GamepadHapticsResult callback_result_;
  raw_ptr<FakeHidWriter, DanglingUntriaged> usb_writer_;
  raw_ptr<FakeHidWriter, DanglingUntriaged> bluetooth_writer_;
  std::unique_ptr<Dualshock4Controller> ds4_usb_;
  std::unique_ptr<Dualshock4Controller> ds4_bluetooth_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(Dualshock4ControllerTest, PlayEffectUsb) {
  EXPECT_TRUE(usb_writer_->output_reports.empty());
  EXPECT_EQ(0, callback_count_);

  PostPlayEffect(ds4_usb_.get(), kZeroStartDelayMillis, kStrongMagnitude,
                 kWeakMagnitude,
                 base::BindOnce(&Dualshock4ControllerTest::Callback,
                                base::Unretained(this)));

  // Run the queued task and start vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(usb_writer_->output_reports,
              testing::ElementsAre(usb_start_vibration_report_));
  EXPECT_EQ(0, callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(usb_writer_->output_reports,
              testing::ElementsAre(usb_start_vibration_report_));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(Dualshock4ControllerTest, PlayEffectBluetooth) {
  EXPECT_TRUE(bluetooth_writer_->output_reports.empty());
  EXPECT_EQ(0, callback_count_);

  PostPlayEffect(ds4_bluetooth_.get(), kZeroStartDelayMillis, kStrongMagnitude,
                 kWeakMagnitude,
                 base::BindOnce(&Dualshock4ControllerTest::Callback,
                                base::Unretained(this)));

  // Run the queued task and start vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(bluetooth_writer_->output_reports,
              testing::ElementsAre(bluetooth_start_vibration_report_));
  EXPECT_EQ(0, callback_count_);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(bluetooth_writer_->output_reports,
              testing::ElementsAre(bluetooth_start_vibration_report_));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(Dualshock4ControllerTest, ResetVibrationUsb) {
  EXPECT_TRUE(usb_writer_->output_reports.empty());
  EXPECT_EQ(0, callback_count_);

  PostResetVibration(ds4_usb_.get(),
                     base::BindOnce(&Dualshock4ControllerTest::Callback,
                                    base::Unretained(this)));

  // Run the queued task and reset vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(usb_writer_->output_reports,
              testing::ElementsAre(usb_stop_vibration_report_));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(Dualshock4ControllerTest, ResetVibrationBluetooth) {
  EXPECT_TRUE(bluetooth_writer_->output_reports.empty());
  EXPECT_EQ(0, callback_count_);

  PostResetVibration(ds4_bluetooth_.get(),
                     base::BindOnce(&Dualshock4ControllerTest::Callback,
                                    base::Unretained(this)));

  // Run the queued task and reset vibration.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(bluetooth_writer_->output_reports,
              testing::ElementsAre(bluetooth_stop_vibration_report_));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            callback_result_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

}  // namespace

}  // namespace device
