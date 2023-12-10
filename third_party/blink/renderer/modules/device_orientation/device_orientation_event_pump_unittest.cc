// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"

#include <string.h>

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace {

constexpr double kEpsilon = 1e-8;

}  // namespace

namespace blink {

using device::FakeSensorProvider;
using device::mojom::SensorType;

using State = DeviceSensorEntry::State;

class MockDeviceOrientationController final
    : public GarbageCollected<MockDeviceOrientationController>,
      public PlatformEventController {
 public:
  explicit MockDeviceOrientationController(
      DeviceOrientationEventPump* orientation_pump,
      LocalDOMWindow& window)
      : PlatformEventController(window),
        did_change_device_orientation_(false),
        orientation_pump_(orientation_pump) {}

  MockDeviceOrientationController(const MockDeviceOrientationController&) =
      delete;
  MockDeviceOrientationController& operator=(
      const MockDeviceOrientationController&) = delete;

  ~MockDeviceOrientationController() override {}

  void Trace(Visitor* visitor) const override {
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

  DeviceSensorEntry::State relative_sensor_state() {
    return orientation_pump_->GetRelativeSensorStateForTesting();
  }

  DeviceSensorEntry::State absolute_sensor_state() {
    return orientation_pump_->GetAbsoluteSensorStateForTesting();
  }

  DeviceOrientationEventPump* orientation_pump() {
    return orientation_pump_.Get();
  }

 private:
  bool did_change_device_orientation_;
  Member<DeviceOrientationEventPump> orientation_pump_;
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() = default;

  DeviceOrientationEventPumpTest(const DeviceOrientationEventPumpTest&) =
      delete;
  DeviceOrientationEventPumpTest& operator=(
      const DeviceOrientationEventPumpTest&) = delete;

 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();

    mojo::PendingRemote<mojom::blink::WebSensorProvider> sensor_provider;
    sensor_provider_.Bind(ToCrossVariantMojoType(
        sensor_provider.InitWithNewPipeAndPassReceiver()));
    auto* orientation_pump = MakeGarbageCollected<DeviceOrientationEventPump>(
        page_holder_->GetFrame(), false /* absolute */);
    orientation_pump->SetSensorProviderForTesting(std::move(sensor_provider));

    controller_ = MakeGarbageCollected<MockDeviceOrientationController>(
        orientation_pump, *page_holder_->GetFrame().DomWindow());

    EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
    EXPECT_EQ(controller()->absolute_sensor_state(), State::kNotInitialized);
    EXPECT_EQ(DeviceOrientationEventPump::PumpState::kStopped,
              controller_->orientation_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->orientation_pump()->FireEvent(nullptr); }

  MockDeviceOrientationController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  test::TaskEnvironment task_environment_;
  Persistent<MockDeviceOrientationController> controller_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  FakeSensorProvider sensor_provider_;
};

TEST_F(DeviceOrientationEventPumpTest, SensorIsActive) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest, SensorSuspendedDuringInitialization) {
  controller()->RegisterWithDispatcher();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kInitializing);

  controller()->UnregisterWithDispatcher();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kShouldSuspend);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kSuspended);

  controller()->RegisterWithDispatcher();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest, SensorIsActiveWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest, SensorSuspendedDuringFallback) {
  // Make the relative orientation sensor unavailable and the first time it is
  // requested cause Stop() to be called before the error is processed.
  sensor_provider()->set_relative_orientation_sensor_is_available(false);
  sensor_provider()->set_sensor_requested_callback(
      base::BindLambdaForTesting([&](SensorType type) {
        EXPECT_EQ(type, SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
        controller()->UnregisterWithDispatcher();
        EXPECT_EQ(controller()->relative_sensor_state(), State::kShouldSuspend);
      }));

  controller()->RegisterWithDispatcher();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kInitializing);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest, SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest,
       SomeSensorDataFieldsNotAvailableWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest, FireAllNullEvent) {
  // No active sensors.
  sensor_provider()->set_relative_orientation_sensor_is_available(false);
  sensor_provider()->set_absolute_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kNotInitialized);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_FALSE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kNotInitialized);
}

TEST_F(DeviceOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kActive);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZeroWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest, UpdateRespectsOrientationThreshold) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kSuspended);
}

TEST_F(DeviceOrientationEventPumpTest,
       UpdateRespectsOrientationThresholdWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->relative_sensor_state(), State::kNotInitialized);
  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

class DeviceAbsoluteOrientationEventPumpTest : public testing::Test {
 public:
  DeviceAbsoluteOrientationEventPumpTest() = default;

  DeviceAbsoluteOrientationEventPumpTest(
      const DeviceAbsoluteOrientationEventPumpTest&) = delete;
  DeviceAbsoluteOrientationEventPumpTest& operator=(
      const DeviceAbsoluteOrientationEventPumpTest&) = delete;

 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();

    mojo::PendingRemote<mojom::blink::WebSensorProvider> sensor_provider;
    sensor_provider_.Bind(ToCrossVariantMojoType(
        sensor_provider.InitWithNewPipeAndPassReceiver()));
    auto* absolute_orientation_pump =
        MakeGarbageCollected<DeviceOrientationEventPump>(
            page_holder_->GetFrame(), true /* absolute */);
    absolute_orientation_pump->SetSensorProviderForTesting(
        std::move(sensor_provider));

    controller_ = MakeGarbageCollected<MockDeviceOrientationController>(
        absolute_orientation_pump, *page_holder_->GetFrame().DomWindow());

    EXPECT_EQ(controller()->absolute_sensor_state(), State::kNotInitialized);
    EXPECT_EQ(DeviceOrientationEventPump::PumpState::kStopped,
              controller_->orientation_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->orientation_pump()->FireEvent(nullptr); }

  MockDeviceOrientationController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  test::TaskEnvironment task_environment_;
  Persistent<MockDeviceOrientationController> controller_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  FakeSensorProvider sensor_provider_;
};

TEST_F(DeviceAbsoluteOrientationEventPumpTest, SensorIsActive) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, FireAllNullEvent) {
  // No active sensor.
  sensor_provider()->set_absolute_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kNotInitialized);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_FALSE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kNotInitialized);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       UpdateRespectsOrientationThreshold) {
  controller()->RegisterWithDispatcher();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kActive);

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

  controller()->UnregisterWithDispatcher();

  EXPECT_EQ(controller()->absolute_sensor_state(), State::kSuspended);
}

}  // namespace blink
