// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"

#include <string.h>

#include <memory>

#include "base/numerics/angle_conversions.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_rotation_rate.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using device::FakeSensorProvider;

class MockDeviceMotionController final
    : public GarbageCollected<MockDeviceMotionController>,
      public PlatformEventController {
 public:
  explicit MockDeviceMotionController(DeviceMotionEventPump* motion_pump,
                                      LocalDOMWindow& window)
      : PlatformEventController(window),
        did_change_device_motion_(false),
        motion_pump_(motion_pump) {}

  MockDeviceMotionController(const MockDeviceMotionController&) = delete;
  MockDeviceMotionController& operator=(const MockDeviceMotionController&) =
      delete;

  ~MockDeviceMotionController() override {}

  void Trace(Visitor* visitor) const override {
    PlatformEventController::Trace(visitor);
    visitor->Trace(motion_pump_);
  }

  void DidUpdateData() override {
    did_change_device_motion_ = true;
    ++number_of_events_;
  }

  bool did_change_device_motion() const { return did_change_device_motion_; }

  int number_of_events() const { return number_of_events_; }

  void RegisterWithDispatcher() override { motion_pump_->SetController(this); }

  bool HasLastData() override { return motion_pump_->LatestDeviceMotionData(); }

  void UnregisterWithDispatcher() override { motion_pump_->RemoveController(); }

  const DeviceMotionData* data() {
    return motion_pump_->LatestDeviceMotionData();
  }

  DeviceMotionEventPump* motion_pump() { return motion_pump_.Get(); }

 private:
  bool did_change_device_motion_;
  int number_of_events_;
  Member<DeviceMotionEventPump> motion_pump_;
};

class DeviceMotionEventPumpTest : public testing::Test {
 public:
  DeviceMotionEventPumpTest() = default;

  DeviceMotionEventPumpTest(const DeviceMotionEventPumpTest&) = delete;
  DeviceMotionEventPumpTest& operator=(const DeviceMotionEventPumpTest&) =
      delete;

 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();

    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    sensor_provider_.Bind(sensor_provider.InitWithNewPipeAndPassReceiver());
    auto* motion_pump =
        MakeGarbageCollected<DeviceMotionEventPump>(page_holder_->GetFrame());
    motion_pump->SetSensorProviderForTesting(
        ToCrossVariantMojoType(std::move(sensor_provider)));

    controller_ = MakeGarbageCollected<MockDeviceMotionController>(
        motion_pump, *page_holder_->GetFrame().DomWindow());

    ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kNotInitialized);
    EXPECT_EQ(DeviceMotionEventPump::PumpState::kStopped,
              controller_->motion_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->motion_pump()->FireEvent(nullptr); }

  void ExpectAccelerometerStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->motion_pump()->accelerometer_->state());
  }

  void ExpectLinearAccelerationSensorStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->motion_pump()->linear_acceleration_sensor_->state());
  }

  void ExpectGyroscopeStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->motion_pump()->gyroscope_->state());
  }

  void ExpectAllThreeSensorsStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    ExpectAccelerometerStateToBe(expected_sensor_state);
    ExpectLinearAccelerationSensorStateToBe(expected_sensor_state);
    ExpectGyroscopeStateToBe(expected_sensor_state);
  }

  MockDeviceMotionController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  test::TaskEnvironment task_environment_;
  Persistent<MockDeviceMotionController> controller_;
  std::unique_ptr<DummyPageHolder> page_holder_;

  FakeSensorProvider sensor_provider_;
};

TEST_F(DeviceMotionEventPumpTest, AllSensorsAreActive) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kActive);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_TRUE(
      received_data->GetAccelerationIncludingGravity()->HasAccelerationData());
  EXPECT_EQ(1, received_data->GetAccelerationIncludingGravity()->x().value());
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->y().value());
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->z().value());

  EXPECT_TRUE(received_data->GetAcceleration()->HasAccelerationData());
  EXPECT_EQ(4, received_data->GetAcceleration()->x().value());
  EXPECT_EQ(5, received_data->GetAcceleration()->y().value());
  EXPECT_EQ(6, received_data->GetAcceleration()->z().value());

  EXPECT_TRUE(received_data->GetRotationRate()->HasRotationData());
  EXPECT_EQ(base::RadToDeg(7.0),
            received_data->GetRotationRate()->alpha().value());
  EXPECT_EQ(base::RadToDeg(8.0),
            received_data->GetRotationRate()->beta().value());
  EXPECT_EQ(base::RadToDeg(9.0),
            received_data->GetRotationRate()->gamma().value());

  controller()->UnregisterWithDispatcher();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kSuspended);
}

TEST_F(DeviceMotionEventPumpTest, TwoSensorsAreActive) {
  sensor_provider()->set_linear_acceleration_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  ExpectAccelerometerStateToBe(DeviceSensorEntry::State::kActive);
  ExpectLinearAccelerationSensorStateToBe(
      DeviceSensorEntry::State::kNotInitialized);
  ExpectGyroscopeStateToBe(DeviceSensorEntry::State::kActive);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_TRUE(
      received_data->GetAccelerationIncludingGravity()->HasAccelerationData());
  EXPECT_EQ(1, received_data->GetAccelerationIncludingGravity()->x().value());
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->y().value());
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->z().value());

  EXPECT_FALSE(received_data->GetAcceleration()->x().has_value());
  EXPECT_FALSE(received_data->GetAcceleration()->y().has_value());
  EXPECT_FALSE(received_data->GetAcceleration()->z().has_value());

  EXPECT_TRUE(received_data->GetRotationRate()->HasRotationData());
  EXPECT_EQ(base::RadToDeg(7.0),
            received_data->GetRotationRate()->alpha().value());
  EXPECT_EQ(base::RadToDeg(8.0),
            received_data->GetRotationRate()->beta().value());
  EXPECT_EQ(base::RadToDeg(9.0),
            received_data->GetRotationRate()->gamma().value());

  controller()->UnregisterWithDispatcher();

  ExpectAccelerometerStateToBe(DeviceSensorEntry::State::kSuspended);
  ExpectLinearAccelerationSensorStateToBe(
      DeviceSensorEntry::State::kNotInitialized);
  ExpectGyroscopeStateToBe(DeviceSensorEntry::State::kSuspended);
}

TEST_F(DeviceMotionEventPumpTest, SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kActive);

  sensor_provider()->UpdateAccelerometerData(NAN, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, NAN, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, NAN);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_FALSE(
      received_data->GetAccelerationIncludingGravity()->x().has_value());
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->y().value());
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->z().value());

  EXPECT_EQ(4, received_data->GetAcceleration()->x().value());
  EXPECT_FALSE(received_data->GetAcceleration()->y().has_value());
  EXPECT_EQ(6, received_data->GetAcceleration()->z().value());

  EXPECT_TRUE(received_data->GetAcceleration()->HasAccelerationData());
  EXPECT_EQ(base::RadToDeg(7.0),
            received_data->GetRotationRate()->alpha().value());
  EXPECT_EQ(base::RadToDeg(8.0),
            received_data->GetRotationRate()->beta().value());
  EXPECT_FALSE(received_data->GetRotationRate()->gamma().has_value());

  controller()->UnregisterWithDispatcher();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kSuspended);
}

TEST_F(DeviceMotionEventPumpTest, FireAllNullEvent) {
  // No active sensors.
  sensor_provider()->set_accelerometer_is_available(false);
  sensor_provider()->set_linear_acceleration_sensor_is_available(false);
  sensor_provider()->set_gyroscope_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kNotInitialized);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_FALSE(received_data->GetAcceleration()->HasAccelerationData());

  EXPECT_FALSE(
      received_data->GetAccelerationIncludingGravity()->HasAccelerationData());

  EXPECT_FALSE(received_data->GetRotationRate()->HasRotationData());

  controller()->UnregisterWithDispatcher();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kNotInitialized);
}

TEST_F(DeviceMotionEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kActive);

  FireEvent();
  EXPECT_FALSE(controller()->did_change_device_motion());

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  FireEvent();
  EXPECT_FALSE(controller()->did_change_device_motion());

  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  FireEvent();
  EXPECT_FALSE(controller()->did_change_device_motion());

  sensor_provider()->UpdateGyroscopeData(7, 8, 9);
  FireEvent();
  // Event is fired only after all the available sensors have data.
  EXPECT_TRUE(controller()->did_change_device_motion());

  controller()->UnregisterWithDispatcher();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kSuspended);
}

// Confirm that the frequency of pumping events is not greater than 60Hz.
// A rate above 60Hz would allow for the detection of keystrokes.
// (crbug.com/421691)
TEST_F(DeviceMotionEventPumpTest, PumpThrottlesEventRate) {
  // Confirm that the delay for pumping events is 60 Hz.
  EXPECT_GE(60, base::Time::kMicrosecondsPerSecond /
                    DeviceMotionEventPump::kDefaultPumpDelayMicroseconds);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kActive);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  base::RunLoop loop;
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
      FROM_HERE, loop.QuitWhenIdleClosure(), base::Milliseconds(100));
  loop.Run();
  controller()->UnregisterWithDispatcher();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::kSuspended);

  // Check that the PlatformEventController does not receive excess
  // events.
  EXPECT_TRUE(controller()->did_change_device_motion());
  EXPECT_GE(6, controller()->number_of_events());
}

}  // namespace blink
