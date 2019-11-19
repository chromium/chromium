// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/ambient_light_sensor.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_test_utils.h"

namespace blink {

namespace {

class MockSensorProxyObserver
    : public GarbageCollected<MockSensorProxyObserver>,
      public SensorProxy::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(MockSensorProxyObserver);

 public:
  virtual ~MockSensorProxyObserver() = default;

  // Synchronously waits for OnSensorReadingChanged() to be called.
  void WaitForOnSensorReadingChanged() {
    run_loop_.emplace();
    run_loop_->Run();
  }

  void OnSensorReadingChanged() override {
    DCHECK(run_loop_.has_value() && run_loop_->running());
    run_loop_->Quit();
  }

 private:
  base::Optional<base::RunLoop> run_loop_;
};

}  // namespace

TEST(AmbientLightSensorTest, IlluminanceInStoppedSensor) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);

  bool illuminance_is_null;
  sensor->illuminance(illuminance_is_null);
  EXPECT_TRUE(illuminance_is_null);
  EXPECT_FALSE(sensor->hasReading());
}

TEST(AmbientLightSensorTest, IlluminanceInSensorWithoutReading) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kActivate);

  bool illuminance_is_null;
  sensor->illuminance(illuminance_is_null);
  EXPECT_TRUE(illuminance_is_null);
  EXPECT_FALSE(sensor->hasReading());
}

TEST(AmbientLightSensorTest, IlluminanceRounding) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kActivate);
  EXPECT_FALSE(sensor->hasReading());

  // At this point, we have received an 'activate' event, so the sensor is
  // initialized and it is connected to a SensorProxy that we can retrieve
  // here. We then attach a new SensorProxy::Observer that we use to
  // synchronously wait for OnSensorReadingChanged() to be called. Even though
  // the order that each observer is notified is arbitrary, we know that by the
  // time we get to the next call here all observers will have been called.
  auto* sensor_proxy =
      SensorProviderProxy::From(To<Document>(context.GetExecutionContext()))
          ->GetSensorProxy(device::mojom::blink::SensorType::AMBIENT_LIGHT);
  ASSERT_NE(sensor_proxy, nullptr);
  auto* mock_observer = MakeGarbageCollected<MockSensorProxyObserver>();
  sensor_proxy->AddObserver(mock_observer);

  bool illuminance_is_null;

  auto* event_counter = MakeGarbageCollected<SensorTestUtils::EventCounter>();
  sensor->addEventListener(event_type_names::kReading, event_counter);

  // Go from no reading to 24. This will cause a new "reading" event to be
  // emitted, and the rounding will cause illuminance() to return 0.
  context.sensor_provider()->UpdateAmbientLightSensorData(24);
  mock_observer->WaitForOnSensorReadingChanged();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);
  EXPECT_EQ(24, sensor->latest_reading_);
  EXPECT_EQ(0, sensor->illuminance(illuminance_is_null));
  EXPECT_FALSE(illuminance_is_null);

  // Go from 24 to 35. The difference is not significant enough, so we will not
  // emit any "reading" event or store the new raw reading, as if the new
  // reading had never existed.
  context.sensor_provider()->UpdateAmbientLightSensorData(35);
  mock_observer->WaitForOnSensorReadingChanged();
  EXPECT_EQ(24, sensor->latest_reading_);
  EXPECT_EQ(0, sensor->illuminance(illuminance_is_null));
  EXPECT_FALSE(illuminance_is_null);

  // Go from 24 to 49. The difference is significant enough, so we will emit a
  // new "reading" event, update our raw reading and return a rounded value of
  // 50 in illuminance().
  context.sensor_provider()->UpdateAmbientLightSensorData(49);
  mock_observer->WaitForOnSensorReadingChanged();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);
  EXPECT_EQ(49, sensor->latest_reading_);
  EXPECT_EQ(50, sensor->illuminance(illuminance_is_null));
  EXPECT_FALSE(illuminance_is_null);

  // Go from 49 to 35. The difference is not significant enough, so we will not
  // emit any "reading" event or store the new raw reading, as if the new
  // reading had never existed.
  context.sensor_provider()->UpdateAmbientLightSensorData(35);
  mock_observer->WaitForOnSensorReadingChanged();
  EXPECT_EQ(49, sensor->latest_reading_);
  EXPECT_EQ(50, sensor->illuminance(illuminance_is_null));
  EXPECT_FALSE(illuminance_is_null);

  // Go from 49 to 24. The difference is significant enough, so we will emit a
  // new "reading" event, update our raw reading and return a rounded value of
  // 0 in illuminance().
  context.sensor_provider()->UpdateAmbientLightSensorData(24);
  mock_observer->WaitForOnSensorReadingChanged();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);
  EXPECT_EQ(24, sensor->latest_reading_);
  EXPECT_EQ(0, sensor->illuminance(illuminance_is_null));
  EXPECT_FALSE(illuminance_is_null);

  // Make sure there were no stray "reading" events besides those we expected
  // above.
  EXPECT_EQ(3U, event_counter->event_count());
}

}  // namespace blink
