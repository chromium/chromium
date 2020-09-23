// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/ambient_light_sensor.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_test_utils.h"

namespace blink {

namespace {

class MockSensorProxyObserver
    : public GarbageCollected<MockSensorProxyObserver>,
      public SensorProxy::Observer {
 public:
  virtual ~MockSensorProxyObserver() = default;

  // Synchronously waits for OnSensorReadingChanged() to be called.
  void WaitForOnSensorReadingChanged() {
    sensor_reading_changed_run_loop_.emplace();
    sensor_reading_changed_run_loop_->Run();
  }

  // Synchronously waits for OnSensorInitialized() to be called.
  void WaitForSensorInitialization() {
    sensor_initialized_run_loop_.emplace();
    sensor_initialized_run_loop_->Run();
  }

  // SensorProxy::Observer overrides.
  void OnSensorInitialized() override {
    if (sensor_initialized_run_loop_.has_value() &&
        sensor_initialized_run_loop_->running()) {
      sensor_initialized_run_loop_->Quit();
    }
  }

  void OnSensorReadingChanged() override {
    if (sensor_reading_changed_run_loop_.has_value() &&
        sensor_reading_changed_run_loop_->running()) {
      sensor_reading_changed_run_loop_->Quit();
    }
  }

 private:
  base::Optional<base::RunLoop> sensor_initialized_run_loop_;
  base::Optional<base::RunLoop> sensor_reading_changed_run_loop_;
};

}  // namespace

TEST(AmbientLightSensorTest, IlluminanceInStoppedSensor) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);

  EXPECT_FALSE(sensor->illuminance().has_value());
  EXPECT_FALSE(sensor->hasReading());
}

TEST(AmbientLightSensorTest, IlluminanceInSensorWithoutReading) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kActivate);

  EXPECT_FALSE(sensor->illuminance().has_value());
  EXPECT_FALSE(sensor->hasReading());
}

TEST(AmbientLightSensorTest, PlatformSensorReadingsBeforeActivation) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();

  auto* sensor_proxy =
      SensorProviderProxy::From(
          To<LocalDOMWindow>(context.GetExecutionContext()))
          ->GetSensorProxy(device::mojom::blink::SensorType::AMBIENT_LIGHT);
  ASSERT_NE(sensor_proxy, nullptr);
  auto* mock_observer = MakeGarbageCollected<MockSensorProxyObserver>();
  sensor_proxy->AddObserver(mock_observer);

  // Instead of waiting for SensorProxy::Observer::OnSensorReadingChanged(), we
  // wait for OnSensorInitialized(), which happens earlier. The platform may
  // start sending readings and calling OnSensorReadingChanged() at any moment
  // from this point on.
  // This test verifies AmbientLightSensor::OnSensorReadingChanged() is able to
  // handle the case of it being called before Sensor itself has transitioned to
  // a fully activated state.
  mock_observer->WaitForSensorInitialization();
  context.sensor_provider()->UpdateAmbientLightSensorData(42);
  ASSERT_FALSE(sensor->activated());
  EXPECT_FALSE(sensor->hasReading());
  EXPECT_FALSE(sensor->illuminance().has_value());
  EXPECT_FALSE(sensor->timestamp(context.GetScriptState()).has_value());

  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);

  EXPECT_EQ(42, sensor->latest_reading_);
  EXPECT_TRUE(sensor->hasReading());
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(50, sensor->illuminance().value());
  EXPECT_TRUE(sensor->timestamp(context.GetScriptState()).has_value());
}

}  // namespace blink
