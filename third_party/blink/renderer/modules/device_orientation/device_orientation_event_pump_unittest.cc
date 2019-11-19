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
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace {

constexpr double kEpsilon = 1e-8;

}  // namespace

namespace blink {

using device::FakeSensorProvider;

class MockDeviceOrientationController final
    : public GarbageCollected<MockDeviceOrientationController>,
      public PlatformEventController {
  USING_GARBAGE_COLLECTED_MIXIN(MockDeviceOrientationController);

 public:
  explicit MockDeviceOrientationController(
      DeviceOrientationEventPump* orientation_pump)
      : PlatformEventController(nullptr),
        did_change_device_orientation_(false),
        orientation_pump_(orientation_pump) {}
  ~MockDeviceOrientationController() override {}

  void Trace(Visitor* visitor) override {
    PlatformEventController::Trace(visitor);
    visitor->Trace(orientation_pump_);
  }

  void DidUpdateData() override { did_change_device_orientation_ = true; }

  bool did_change_device_orientation() const {
    return did_change_device_orientation_;
  }
  void set_did_change_device_orientation(bool value) {
    did_change_device_orientation_ = value;
  }

  void RegisterWithDispatcher() override {
    // In the typical case, |frame| should be non-null. Passing nullptr here
    // causes DeviceOrientationEventPump to exit early from StartListening
    // before DeviceOrientationEventPump::Start is called. As a workaround,
    // Start is called manually by each test case.
    // TODO(crbug.com/850619): Ensure a non-null LocalFrame is passed, and use
    // SetController/RemoveController to start and stop the event pump.
    orientation_pump_->SetController(this);
  }

  bool HasLastData() override {
    return orientation_pump_->LatestDeviceOrientationData();
  }

  void UnregisterWithDispatcher() override {
    orientation_pump_->RemoveController();
  }

  const DeviceOrientationData* data() {
    return orientation_pump_->LatestDeviceOrientationData();
  }

  DeviceOrientationEventPump* orientation_pump() {
    return orientation_pump_.Get();
  }

 private:
  bool did_change_device_orientation_;
  Member<DeviceOrientationEventPump> orientation_pump_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceOrientationController);
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() = default;

 protected:
  void SetUp() override {
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    sensor_provider_.Bind(sensor_provider.InitWithNewPipeAndPassReceiver());
    auto* orientation_pump = MakeGarbageCollected<DeviceOrientationEventPump>(
        base::ThreadTaskRunnerHandle::Get(), false /* absolute */);
    orientation_pump->SetSensorProviderForTesting(
        mojo::PendingRemote<device::mojom::blink::SensorProvider>(
            sensor_provider.PassPipe(),
            device::mojom::SensorProvider::Version_));

    controller_ =
        MakeGarbageCollected<MockDeviceOrientationController>(orientation_pump);

    ExpectRelativeOrientationSensorStateToBe(
        DeviceSensorEntry::State::NOT_INITIALIZED);
    ExpectAbsoluteOrientationSensorStateToBe(
        DeviceSensorEntry::State::NOT_INITIALIZED);
    EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
              controller_->orientation_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->orientation_pump()->FireEvent(nullptr); }

  void ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    EXPECT_EQ(
        expected_sensor_state,
        controller_->orientation_pump()->relative_orientation_sensor_->state());
  }

  void ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    EXPECT_EQ(
        expected_sensor_state,
        controller_->orientation_pump()->absolute_orientation_sensor_->state());
  }

  MockDeviceOrientationController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  Persistent<MockDeviceOrientationController> controller_;
  FakeSensorProvider sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpTest);
};

TEST_F(DeviceOrientationEventPumpTest, MultipleStartAndStopWithWait) {
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceOrientationEventPumpTest,
       MultipleStartAndStopWithWaitWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceOrientationEventPumpTest, CallStop) {
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStopWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStartAndStop) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStartAndStopWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStartMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       CallStartMultipleTimesWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStopMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       CallStopMultipleTimesWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

// Test a sequence of Start(), Stop(), Start() calls only bind sensor once.
TEST_F(DeviceOrientationEventPumpTest, SensorOnlyBindOnce) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

// Test when using fallback from relative orientation to absolute orientation,
// a sequence of Start(), Stop(), Start() calls only bind sensor once.
TEST_F(DeviceOrientationEventPumpTest, SensorOnlyBindOnceWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, SensorIsActive) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 /* alpha */, 2 /* beta */, 3 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides relative orientation data when it is
  // available.
  EXPECT_DOUBLE_EQ(1, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, SensorIsActiveWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());

  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      NAN /* alpha */, 2 /* beta */, 3 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       SomeSensorDataFieldsNotAvailableWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, NAN /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, FireAllNullEvent) {
  // No active sensors.
  sensor_provider()->set_relative_orientation_sensor_is_available(false);
  sensor_provider()->set_absolute_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_FALSE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZeroWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, UpdateRespectsOrientationThreshold) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 /* alpha */, 2 /* beta */, 3 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides relative orientation data when it is
  // available.
  EXPECT_DOUBLE_EQ(1, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* alpha */,
      2 /* beta */, 3 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_FALSE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(1, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 + DeviceOrientationEventPump::kOrientationThreshold /* alpha */,
      2 /* beta */, 3 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(1 + DeviceOrientationEventPump::kOrientationThreshold,
                   received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       UpdateRespectsOrientationThresholdWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_FALSE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold +
          kEpsilon /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(
      5 + DeviceOrientationEventPump::kOrientationThreshold + kEpsilon,
      received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

class DeviceAbsoluteOrientationEventPumpTest : public testing::Test {
 public:
  DeviceAbsoluteOrientationEventPumpTest() = default;

 protected:
  void SetUp() override {
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    sensor_provider_.Bind(sensor_provider.InitWithNewPipeAndPassReceiver());
    auto* absolute_orientation_pump =
        MakeGarbageCollected<DeviceOrientationEventPump>(
            base::ThreadTaskRunnerHandle::Get(), true /* absolute */);
    absolute_orientation_pump->SetSensorProviderForTesting(
        mojo::PendingRemote<device::mojom::blink::SensorProvider>(
            sensor_provider.PassPipe(),
            device::mojom::SensorProvider::Version_));
    controller_ = MakeGarbageCollected<MockDeviceOrientationController>(
        absolute_orientation_pump);

    ExpectAbsoluteOrientationSensorStateToBe(
        DeviceSensorEntry::State::NOT_INITIALIZED);
    EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
              controller_->orientation_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->orientation_pump()->FireEvent(nullptr); }

  void ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State expected_sensor_state) {
    EXPECT_EQ(
        expected_sensor_state,
        controller_->orientation_pump()->absolute_orientation_sensor_->state());
  }

  MockDeviceOrientationController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  Persistent<MockDeviceOrientationController> controller_;
  FakeSensorProvider sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceAbsoluteOrientationEventPumpTest);
};

TEST_F(DeviceAbsoluteOrientationEventPumpTest, MultipleStartAndStopWithWait) {
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStop) {
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStartAndStop) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStartMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStopMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

// Test multiple DeviceSensorEventPump::Start() calls only bind sensor once.
TEST_F(DeviceAbsoluteOrientationEventPumpTest, SensorOnlyBindOnce) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, SensorIsActive) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, NAN /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, FireAllNullEvent) {
  // No active sensor.
  sensor_provider()->set_absolute_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_FALSE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceSensorEntry::State::NOT_INITIALIZED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       UpdateRespectsOrientationThreshold) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_FALSE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold +
          kEpsilon /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(
      5 + DeviceOrientationEventPump::kOrientationThreshold + kEpsilon,
      received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(DeviceSensorEntry::State::SUSPENDED);
}

}  // namespace blink
