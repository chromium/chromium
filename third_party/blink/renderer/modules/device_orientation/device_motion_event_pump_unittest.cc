// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_rotation_rate.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace blink {

using device::FakeSensorProvider;

class MockDeviceMotionController final
    : public GarbageCollected<MockDeviceMotionController>,
      public PlatformEventController {
  USING_GARBAGE_COLLECTED_MIXIN(MockDeviceMotionController);

 public:
  explicit MockDeviceMotionController(DeviceMotionEventPump* motion_pump)
      : PlatformEventController(nullptr),
        did_change_device_motion_(false),
        motion_pump_(motion_pump) {}
  ~MockDeviceMotionController() override {}

  void Trace(Visitor* visitor) override {
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

  DISALLOW_COPY_AND_ASSIGN(MockDeviceMotionController);
};

class DeviceMotionEventPumpTest : public testing::Test {
 public:
  DeviceMotionEventPumpTest() = default;

 protected:
  void SetUp() override {
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    sensor_provider_.Bind(sensor_provider.InitWithNewPipeAndPassReceiver());
    auto* motion_pump = MakeGarbageCollected<DeviceMotionEventPump>(
        base::ThreadTaskRunnerHandle::Get());
    motion_pump->SetSensorProviderForTesting(
        mojo::PendingRemote<device::mojom::blink::SensorProvider>(
            sensor_provider.PassPipe(),
            device::mojom::SensorProvider::Version_));

    controller_ = MakeGarbageCollected<MockDeviceMotionController>(motion_pump);

    ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::NOT_INITIALIZED);
    EXPECT_EQ(DeviceMotionEventPump::PumpState::STOPPED,
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
  Persistent<MockDeviceMotionController> controller_;

  FakeSensorProvider sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPumpTest);
};

TEST_F(DeviceMotionEventPumpTest, MultipleStartAndStopWithWait) {
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::RUNNING,
            controller()->motion_pump()->GetPumpStateForTesting());

  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::STOPPED,
            controller()->motion_pump()->GetPumpStateForTesting());

  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::RUNNING,
            controller()->motion_pump()->GetPumpStateForTesting());

  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::STOPPED,
            controller()->motion_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceMotionEventPumpTest, CallStop) {
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceMotionEventPumpTest, CallStartAndStop) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, CallStartMultipleTimes) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, CallStopMultipleTimes) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

// Test multiple DeviceSensorEventPump::Start() calls only bind sensor once.
TEST_F(DeviceMotionEventPumpTest, SensorOnlyBindOnce) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, AllSensorsAreActive) {
  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  bool is_null;
  EXPECT_TRUE(
      received_data->GetAccelerationIncludingGravity()->HasAccelerationData());
  EXPECT_EQ(1, received_data->GetAccelerationIncludingGravity()->x(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->y(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->z(is_null));
  EXPECT_FALSE(is_null);

  EXPECT_TRUE(received_data->GetAcceleration()->HasAccelerationData());
  EXPECT_EQ(4, received_data->GetAcceleration()->x(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(5, received_data->GetAcceleration()->y(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(6, received_data->GetAcceleration()->z(is_null));
  EXPECT_FALSE(is_null);

  EXPECT_TRUE(received_data->GetRotationRate()->HasRotationData());
  EXPECT_EQ(gfx::RadToDeg(7.0),
            received_data->GetRotationRate()->alpha(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(gfx::RadToDeg(8.0),
            received_data->GetRotationRate()->beta(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(gfx::RadToDeg(9.0),
            received_data->GetRotationRate()->gamma(is_null));
  EXPECT_FALSE(is_null);

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, TwoSensorsAreActive) {
  sensor_provider()->set_linear_acceleration_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAccelerometerStateToBe(DeviceSensorEntry::State::ACTIVE);
  ExpectLinearAccelerationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectGyroscopeStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  bool is_null;
  EXPECT_TRUE(
      received_data->GetAccelerationIncludingGravity()->HasAccelerationData());
  EXPECT_EQ(1, received_data->GetAccelerationIncludingGravity()->x(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->y(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->z(is_null));
  EXPECT_FALSE(is_null);

  received_data->GetAcceleration()->x(is_null);
  EXPECT_TRUE(is_null);
  received_data->GetAcceleration()->y(is_null);
  EXPECT_TRUE(is_null);
  received_data->GetAcceleration()->z(is_null);
  EXPECT_TRUE(is_null);

  EXPECT_TRUE(received_data->GetRotationRate()->HasRotationData());
  EXPECT_EQ(gfx::RadToDeg(7.0),
            received_data->GetRotationRate()->alpha(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(gfx::RadToDeg(8.0),
            received_data->GetRotationRate()->beta(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(gfx::RadToDeg(9.0),
            received_data->GetRotationRate()->gamma(is_null));
  EXPECT_FALSE(is_null);

  controller()->motion_pump()->Stop();

  ExpectAccelerometerStateToBe(DeviceSensorEntry::State::SUSPENDED);
  ExpectLinearAccelerationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectGyroscopeStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(NAN, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, NAN, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, NAN);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  bool is_null;
  received_data->GetAccelerationIncludingGravity()->x(is_null);
  EXPECT_TRUE(is_null);
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->y(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->z(is_null));
  EXPECT_FALSE(is_null);

  EXPECT_EQ(4, received_data->GetAcceleration()->x(is_null));
  EXPECT_FALSE(is_null);
  received_data->GetAcceleration()->y(is_null);
  EXPECT_TRUE(is_null);
  EXPECT_EQ(6, received_data->GetAcceleration()->z(is_null));
  EXPECT_FALSE(is_null);

  EXPECT_TRUE(received_data->GetAcceleration()->HasAccelerationData());
  EXPECT_EQ(gfx::RadToDeg(7.0),
            received_data->GetRotationRate()->alpha(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(gfx::RadToDeg(8.0),
            received_data->GetRotationRate()->beta(is_null));
  EXPECT_FALSE(is_null);
  received_data->GetRotationRate()->gamma(is_null);
  EXPECT_TRUE(is_null);

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, FireAllNullEvent) {
  // No active sensors.
  sensor_provider()->set_accelerometer_is_available(false);
  sensor_provider()->set_linear_acceleration_sensor_is_available(false);
  sensor_provider()->set_gyroscope_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::NOT_INITIALIZED);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_FALSE(received_data->GetAcceleration()->HasAccelerationData());

  EXPECT_FALSE(
      received_data->GetAccelerationIncludingGravity()->HasAccelerationData());

  EXPECT_FALSE(received_data->GetRotationRate()->HasRotationData());

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceMotionEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);

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

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

// Confirm that the frequency of pumping events is not greater than 60Hz.
// A rate above 60Hz would allow for the detection of keystrokes.
// (crbug.com/421691)
TEST_F(DeviceMotionEventPumpTest, PumpThrottlesEventRate) {
  // Confirm that the delay for pumping events is 60 Hz.
  EXPECT_GE(60, base::Time::kMicrosecondsPerSecond /
                    DeviceMotionEventPump::kDefaultPumpDelayMicroseconds);

  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  base::RunLoop loop;
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
      FROM_HERE, loop.QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(100));
  loop.Run();
  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceSensorEntry::State::SUSPENDED);

  // Check that the PlatformEventController does not receive excess
  // events.
  EXPECT_TRUE(controller()->did_change_device_motion());
  EXPECT_GE(6, controller()->number_of_events());
}

}  // namespace blink
