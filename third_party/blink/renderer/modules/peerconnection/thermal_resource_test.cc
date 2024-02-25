// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/thermal_resource.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/fake_resource_listener.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/webrtc/api/adaptation/resource.h"

namespace blink {

namespace {

const int64_t kReportIntervalMs = 10000;

class ThermalResourceTest : public ::testing::Test {
 public:
  ThermalResourceTest()
      : task_runner_(platform_->test_task_runner()),
        resource_(ThermalResource::Create(task_runner_)) {}

  void TearDown() override {
    // Give in-flight tasks a chance to run before shutdown.
    resource_->SetResourceListener(nullptr);
    task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  // Tasks run on the test thread with fake time, use FastForwardBy() to
  // advance time and execute delayed tasks.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  scoped_refptr<ThermalResource> resource_;
  FakeResourceListener listener_;
};

}  // namespace

TEST_F(ThermalResourceTest, NoMeasurementsByDefault) {
  resource_->SetResourceListener(&listener_);
  EXPECT_EQ(0u, listener_.measurement_count());
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  EXPECT_EQ(0u, listener_.measurement_count());
}

TEST_F(ThermalResourceTest, NominalTriggersUnderuse) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kNominal);
  EXPECT_EQ(1u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kUnderuse,
            listener_.latest_measurement());
}

TEST_F(ThermalResourceTest, FairTriggersUnderuse) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kFair);
  EXPECT_EQ(1u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kUnderuse,
            listener_.latest_measurement());
}

TEST_F(ThermalResourceTest, SeriousTriggersOveruse) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kSerious);
  EXPECT_EQ(1u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kOveruse,
            listener_.latest_measurement());
}

TEST_F(ThermalResourceTest, CriticalTriggersOveruse) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kCritical);
  EXPECT_EQ(1u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kOveruse,
            listener_.latest_measurement());
}

TEST_F(ThermalResourceTest, UnknownDoesNotTriggerUsage) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kUnknown);
  EXPECT_EQ(0u, listener_.measurement_count());
}

TEST_F(ThermalResourceTest, MeasurementsRepeatEvery10Seconds) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kSerious);
  size_t expected_count = listener_.measurement_count();

  // First Interval.
  // No new measurement if we advance less than the interval.
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs - 1));
  EXPECT_EQ(expected_count, listener_.measurement_count());
  // When the interval is reached, expect a new measurement.
  task_runner_->FastForwardBy(base::Milliseconds(1));
  ++expected_count;
  EXPECT_EQ(expected_count, listener_.measurement_count());

  // Second Interval.
  // No new measurement if we advance less than the interval.
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs - 1));
  EXPECT_EQ(expected_count, listener_.measurement_count());
  // When the interval is reached, expect a new measurement.
  task_runner_->FastForwardBy(base::Milliseconds(1));
  ++expected_count;
  EXPECT_EQ(expected_count, listener_.measurement_count());

  // Third Interval.
  // No new measurement if we advance less than the interval.
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs - 1));
  EXPECT_EQ(expected_count, listener_.measurement_count());
  // When the interval is reached, expect a new measurement.
  task_runner_->FastForwardBy(base::Milliseconds(1));
  ++expected_count;
  EXPECT_EQ(expected_count, listener_.measurement_count());
}

TEST_F(ThermalResourceTest, NewMeasurementInvalidatesInFlightRepetition) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kSerious);
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));

  // We are repeatedly kOveruse.
  EXPECT_EQ(2u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kOveruse,
            listener_.latest_measurement());
  // Fast-forward half an interval. The repeated measurement is still in-flight.
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs / 2));
  EXPECT_EQ(2u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kOveruse,
            listener_.latest_measurement());
  // Trigger kUnderuse.
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kNominal);
  EXPECT_EQ(3u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kUnderuse,
            listener_.latest_measurement());
  // Fast-forward another half an interval, giving the previous in-flight task
  // a chance to run. No new measurement is expected.
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs / 2));
  EXPECT_EQ(3u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kUnderuse,
            listener_.latest_measurement());
  // Once more, and the repetition of kUnderuse should be observed (one interval
  // has passed since the OnThermalMeasurement).
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs / 2));
  EXPECT_EQ(4u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kUnderuse,
            listener_.latest_measurement());
}

TEST_F(ThermalResourceTest, UnknownStopsRepeatedMeasurements) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kSerious);
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  // The measurement is repeating.
  EXPECT_EQ(2u, listener_.measurement_count());

  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kUnknown);
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  // No more measurements.
  EXPECT_EQ(2u, listener_.measurement_count());
}

TEST_F(ThermalResourceTest, UnregisteringStopsRepeatedMeasurements) {
  resource_->SetResourceListener(&listener_);
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kSerious);
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  // The measurement is repeating.
  EXPECT_EQ(2u, listener_.measurement_count());

  resource_->SetResourceListener(nullptr);
  // If repeating tasks were not stopped, this line would block forever.
  task_runner_->FastForwardUntilNoTasksRemain();
  // No more measurements.
  EXPECT_EQ(2u, listener_.measurement_count());
}

TEST_F(ThermalResourceTest, RegisteringLateTriggersRepeatedMeasurements) {
  resource_->OnThermalMeasurement(mojom::blink::DeviceThermalState::kSerious);
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  EXPECT_EQ(0u, listener_.measurement_count());
  // Registering triggers kOveruse.
  resource_->SetResourceListener(&listener_);
  EXPECT_EQ(1u, listener_.measurement_count());
  EXPECT_EQ(webrtc::ResourceUsageState::kOveruse,
            listener_.latest_measurement());
  // The measurement is repeating.
  task_runner_->FastForwardBy(base::Milliseconds(kReportIntervalMs));
  EXPECT_EQ(2u, listener_.measurement_count());
}

}  // namespace blink
