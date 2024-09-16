// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "services/device/generic_sensor/platform_sensor_provider.h"

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/platform_sensor_util.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace device {

using mojom::SensorType;

namespace {

// Attempts to add a new reading to the sensor owned by |sensor_client|, and
// asserts that it does not lead to OnSensorReadingChanged() being called (i.e.
// PlatformSensor's significance check has failed).
void AddNewReadingAndExpectNoReadingChangedEvent(
    MockPlatformSensorClient* sensor_client,
    const SensorReading& new_reading) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      static_cast<FakePlatformSensor*>(sensor_client->sensor().get());
  fake_sensor->AddNewReading(new_reading);

  base::RunLoop run_loop;
  EXPECT_CALL(*sensor_client,
              OnSensorReadingChanged(sensor_client->sensor()->GetType()))
      .Times(0);
  run_loop.RunUntilIdle();
}

// Add a new reading to the sensor owned by |sensor_client|, and expect reading
// change event.
void AddNewReadingAndExpectReadingChangedEvent(
    MockPlatformSensorClient* sensor_client,
    const SensorReading& new_reading) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      static_cast<FakePlatformSensor*>(sensor_client->sensor().get());
  fake_sensor->AddNewReading(new_reading);

  base::RunLoop run_loop;
  EXPECT_CALL(*sensor_client,
              OnSensorReadingChanged(sensor_client->sensor()->GetType()))
      .WillOnce(Invoke([&](SensorType) { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

class PlatformSensorAndProviderTest : public testing::Test {
 public:
  PlatformSensorAndProviderTest() {
    provider_ = std::make_unique<NiceMock<FakePlatformSensorProvider>>();
  }

  PlatformSensorAndProviderTest(const PlatformSensorAndProviderTest&) = delete;
  PlatformSensorAndProviderTest& operator=(
      const PlatformSensorAndProviderTest&) = delete;

 protected:
  scoped_refptr<FakePlatformSensor> CreateSensorSync(mojom::SensorType type) {
    TestFuture<scoped_refptr<PlatformSensor>> future;
    provider_->CreateSensor(type, future.GetCallback());
    scoped_refptr<FakePlatformSensor> fake_sensor =
        static_cast<FakePlatformSensor*>(future.Get().get());

    // Override FakePlatformSensor's default StartSensor() expectation; we
    // do not want to do anything in StartSensor().
    ON_CALL(*fake_sensor, StartSensor(_))
        .WillByDefault(
            Invoke([&](const PlatformSensorConfiguration& configuration) {
              return true;
            }));

    return fake_sensor;
  }

  std::unique_ptr<NiceMock<FakePlatformSensorProvider>> provider_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PlatformSensorAndProviderTest, ResourcesAreFreed) {
  EXPECT_CALL(*provider_, FreeResources()).Times(2);
  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> s) { EXPECT_TRUE(s); }));
  // Failure.
  EXPECT_CALL(*provider_, CreateSensorInternal)
      .WillOnce(
          Invoke([](mojom::SensorType,
                    PlatformSensorProvider::CreateSensorCallback callback) {
            std::move(callback).Run(nullptr);
          }));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> s) { EXPECT_FALSE(s); }));
}

TEST_F(PlatformSensorAndProviderTest, ResourcesAreNotFreedOnPendingRequest) {
  EXPECT_CALL(*provider_, FreeResources()).Times(0);
  // Suspend.
  EXPECT_CALL(*provider_, CreateSensorInternal)
      .WillOnce(Invoke([](mojom::SensorType,
                          PlatformSensorProvider::CreateSensorCallback) {}));

  TestFuture<scoped_refptr<PlatformSensor>> sensor_future;
  provider_->CreateSensor(mojom::SensorType::AMBIENT_LIGHT,
                          sensor_future.GetCallback());

  TestFuture<scoped_refptr<PlatformSensor>> sensor_future2;
  provider_->CreateSensor(mojom::SensorType::AMBIENT_LIGHT,
                          sensor_future2.GetCallback());

  // CreateSensor callbacks are not invoked while the provider is suspended.
  EXPECT_FALSE(sensor_future.IsReady());
  EXPECT_FALSE(sensor_future2.IsReady());

  // When the provider is destroyed, both callbacks are invoked with nullptr.
  provider_.reset();
  EXPECT_FALSE(sensor_future.Get());
  EXPECT_FALSE(sensor_future2.Get());
}

// This test verifies that the shared buffer's default values are 0.
TEST_F(PlatformSensorAndProviderTest, SharedBufferDefaultValue) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(
      GetSensorReadingSharedBufferOffset(mojom::SensorType::AMBIENT_LIGHT),
      sizeof(SensorReadingSharedBuffer));

  const auto* buffer = mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  EXPECT_THAT(buffer->reading.als.value, 0);
}

// This test verifies that when sensor is stopped, shared buffer contents are
// filled with default values.
TEST_F(PlatformSensorAndProviderTest, SharedBufferCleared) {
  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> sensor) {
        auto client =
            std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
        auto config = PlatformSensorConfiguration(50);

        EXPECT_TRUE(sensor->StartListening(client.get(), config));
        SensorReading reading;
        EXPECT_TRUE(sensor->GetLatestReading(&reading));
        EXPECT_THAT(reading.als.value, 50);

        EXPECT_TRUE(sensor->StopListening(client.get(), config));
        EXPECT_TRUE(sensor->GetLatestReading(&reading));
        EXPECT_THAT(reading.als.value, 0);
      }));
}

// Rounding to nearest 50 (see kAlsRoundingMultiple). 25 (see
// kAlsSignificanceThreshold) difference in values reported in significance
// test, if rounded values differs from previous rounded value.
TEST_F(PlatformSensorAndProviderTest, SensorValueValidityCheckAmbientLight) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      CreateSensorSync(SensorType::AMBIENT_LIGHT);

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));

  // This checks that illuminance significance check causes the following
  // to happen:
  // 1. Initial value is set to 24. And test checks it is correctly rounded
  //    to 0.
  // 2. New reading is attempted to set to 35.
  // 3. Value is read from sensor and compared new reading. But as new
  //    reading was not significantly different compared to initial, for
  //    privacy reasons, service returns the initial value.
  // 4. New value is set to 49. And test checks it is correctly rounded to 50.
  //    New value is allowed as it is significantly different compared to old
  //    value (24).
  // 5. New reading is attempted to set to 35.
  // 6. Value is read from sensor and compared new reading. But as new
  //    reading was not significantly different compared to initial, for
  //    privacy reasons, service returns the initial value.
  // 7. New value is set to 24. And test checks it is correctly rounded to 0.
  //    New value is allowed as it is significantly different compared to old
  //    value (49).
  // 8. Last two values test that if rounded values are same, new reading event
  //    is not triggered.
  const struct {
    const double attempted_als_value;
    const double expected_als_value;
    const bool expect_reading_changed_event;
  } kTestSteps[] = {
      {24, 0, true}, {35, 0, false}, {49, 50, true},  {35, 50, false},
      {24, 0, true}, {50, 50, true}, {25, 50, false},
  };

  for (const auto& test_case : kTestSteps) {
    SensorReading reading;
    reading.raw.timestamp = 1.0;
    reading.als.value = test_case.attempted_als_value;

    if (test_case.expect_reading_changed_event)
      AddNewReadingAndExpectReadingChangedEvent(client.get(), reading);
    else
      AddNewReadingAndExpectNoReadingChangedEvent(client.get(), reading);

    fake_sensor->GetLatestReading(&reading);
    EXPECT_DOUBLE_EQ(reading.als.value, test_case.expected_als_value);
  }
}

TEST_F(PlatformSensorAndProviderTest, ResetLastReadingsOnStop) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      CreateSensorSync(SensorType::AMBIENT_LIGHT);
  ASSERT_EQ(fake_sensor->GetReportingMode(), mojom::ReportingMode::ON_CHANGE);

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));

  SensorReading reading;
  reading.raw.timestamp = 1.0;
  reading.als.value = 1.0;

  AddNewReadingAndExpectReadingChangedEvent(client.get(), reading);

  // We could have also called PlatformSensor::StopListening(),
  // PlatformSensor::UpdateSensor() and PlatformSensor::StartListening(). The
  // idea is to get the sensor to stop, regardless of whether is_active_ is
  // true or not.
  ON_CALL(*client, IsSuspended()).WillByDefault(Return(true));
  fake_sensor->UpdateSensor();
  ON_CALL(*client, IsSuspended()).WillByDefault(Return(false));
  fake_sensor->UpdateSensor();

  // Set the exact same readings. They should be stored because stopping the
  // sensor causes the previous readings to be reset.
  AddNewReadingAndExpectReadingChangedEvent(client.get(), reading);
}

TEST_F(PlatformSensorAndProviderTest, DoNotStoreReadingsWhenInactive) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      CreateSensorSync(SensorType::AMBIENT_LIGHT);
  ASSERT_EQ(fake_sensor->GetReportingMode(), mojom::ReportingMode::ON_CHANGE);

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));
  EXPECT_TRUE(fake_sensor->IsActiveForTesting());

  ON_CALL(*client, IsSuspended()).WillByDefault(Return(true));
  fake_sensor->UpdateSensor();
  EXPECT_FALSE(fake_sensor->IsActiveForTesting());

  SensorReading reading;
  reading.raw.timestamp = 1.0;
  reading.als.value = 1.0;

  AddNewReadingAndExpectNoReadingChangedEvent(client.get(), reading);

  ON_CALL(*client, IsSuspended()).WillByDefault(Return(false));
  fake_sensor->UpdateSensor();
  EXPECT_TRUE(fake_sensor->IsActiveForTesting());

  // Set the exact same readings. They should be stored because
  // |last_raw_reading_| and |last_rounded_reading_| should not have been
  // updated while the sensor was not active.
  AddNewReadingAndExpectReadingChangedEvent(client.get(), reading);
}

// Rounding to nearest 0.1 (see kAccelerometerRoundingMultiple). New reading
// event is triggered if rounded values differs from previous rounded value.
TEST_F(PlatformSensorAndProviderTest, SensorValueValidityCheckAccelerometer) {
  const double kTestValue = 10.0;  // Made up test value.
  scoped_refptr<FakePlatformSensor> fake_sensor =
      CreateSensorSync(SensorType::ACCELEROMETER);

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));

  const struct {
    const double attempted_accelerometer_value_x;
    const double expected_accelerometer_value;
    const bool expect_reading_changed_event;
  } kTestSteps[] = {
      {kTestValue, kTestValue, true},
      {kTestValue, kTestValue, false},
      {kTestValue + kEpsilon, kTestValue, false},
      {kTestValue + kAccelerometerRoundingMultiple - kEpsilon,
       kTestValue + kAccelerometerRoundingMultiple, true},
      {kTestValue + kAccelerometerRoundingMultiple + kEpsilon,
       kTestValue + kAccelerometerRoundingMultiple, false},
  };

  for (const auto& test_case : kTestSteps) {
    SensorReading reading;
    reading.raw.timestamp = 1.0;
    reading.accel.x = test_case.attempted_accelerometer_value_x;
    reading.accel.y = reading.accel.z = 0;

    if (test_case.expect_reading_changed_event)
      AddNewReadingAndExpectReadingChangedEvent(client.get(), reading);
    else
      AddNewReadingAndExpectNoReadingChangedEvent(client.get(), reading);

    fake_sensor->GetLatestReading(&reading);
    EXPECT_DOUBLE_EQ(reading.accel.x, test_case.expected_accelerometer_value);
  }
}

TEST_F(PlatformSensorAndProviderTest, IsSignificantlyDifferentAmbientLight) {
  // Test for AMBIENT_LIGHT as it has different significance threshold compared
  // to others.
  const double kTestValue = 100.0;  // Made up test value.
  scoped_refptr<FakePlatformSensor> fake_sensor =
      CreateSensorSync(SensorType::AMBIENT_LIGHT);

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));

  // Ambient light sensor has threshold points around initial_reading.
  // IsSignificantlyDifferent() returns false when value is located between
  // upper and lower threshold point and true when it is outside of threshold
  // points. Below text shows these threshold points and different areas.
  //
  // Smaller value                   Large values
  //      [--------------X--------------]
  // (1) (2) (3)        (4)        (5) (6) (7)
  //
  // Selected test values are:
  // 1. just below lower threshold point in significantly different lower area
  // 2. lower threshold point
  // 3. just above lower threshold point in not significantly different area
  // 4. center (new reading is same as initial reading)
  // 5. just below upper threshold point in not significantly different area
  // 6. upper threshold point
  // 7. just above upper threshold point in significantly different upper area
  const struct {
    const bool expectation;
    const double new_reading;
  } kTestSteps[] = {
      {true, kTestValue - kAlsSignificanceThreshold - 1},
      {true, kTestValue - kAlsSignificanceThreshold},
      {false, kTestValue - kAlsSignificanceThreshold + 1},
      {false, kTestValue},
      {false, kTestValue + kAlsSignificanceThreshold - 1},
      {true, kTestValue + kAlsSignificanceThreshold},
      {true, kTestValue + kAlsSignificanceThreshold + 1},
  };

  SensorReading initial_reading;
  initial_reading.als.value = kTestValue;

  for (const auto& test_case : kTestSteps) {
    SensorReading new_reading;
    new_reading.als.value = test_case.new_reading;
    EXPECT_THAT(fake_sensor->IsSignificantlyDifferent(
                    initial_reading, new_reading, SensorType::AMBIENT_LIGHT),
                test_case.expectation);
  }
}

TEST_F(PlatformSensorAndProviderTest, IsSignificantlyDifferentMagnetometer) {
  // Test for standard sensor with three values.
  scoped_refptr<FakePlatformSensor> fake_sensor =
      CreateSensorSync(SensorType::MAGNETOMETER);

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));

  const double kTestValue = 100.0;  // Made up test value.
  SensorReading last_reading;
  SensorReading new_reading;

  // No difference in values does not count as a significant change.
  last_reading.magn.x = kTestValue;
  last_reading.magn.y = kTestValue;
  last_reading.magn.z = kTestValue;
  EXPECT_FALSE(fake_sensor->IsSignificantlyDifferent(last_reading, last_reading,
                                                     SensorType::MAGNETOMETER));

  // Check that different values on one axis are reported as significantly
  // different.
  new_reading.magn.x = last_reading.magn.x;
  new_reading.magn.y = last_reading.magn.y + kEpsilon;
  new_reading.magn.z = last_reading.magn.z;
  EXPECT_TRUE(fake_sensor->IsSignificantlyDifferent(last_reading, new_reading,
                                                    SensorType::MAGNETOMETER));

  // Check that different values on all axes are reported as significantly
  // different.
  new_reading.magn.x = last_reading.magn.x + kEpsilon;
  new_reading.magn.y = last_reading.magn.y + kEpsilon;
  new_reading.magn.z = last_reading.magn.z + kEpsilon;
  EXPECT_TRUE(fake_sensor->IsSignificantlyDifferent(last_reading, new_reading,
                                                    SensorType::MAGNETOMETER));

  // Check that different values on all axes are reported as significantly
  // different.
  new_reading.magn.x = last_reading.magn.x - kEpsilon;
  new_reading.magn.y = last_reading.magn.y - kEpsilon;
  new_reading.magn.z = last_reading.magn.z - kEpsilon;
  EXPECT_TRUE(fake_sensor->IsSignificantlyDifferent(last_reading, new_reading,
                                                    SensorType::MAGNETOMETER));
}

}  // namespace device
