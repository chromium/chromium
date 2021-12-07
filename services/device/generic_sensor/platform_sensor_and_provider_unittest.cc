// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "services/device/generic_sensor/platform_sensor_provider.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

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
  EXPECT_CALL(*sensor_client, OnSensorReadingChanged(SensorType::AMBIENT_LIGHT))
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
  EXPECT_CALL(*sensor_client, OnSensorReadingChanged(SensorType::AMBIENT_LIGHT))
      .WillOnce(Invoke([&](SensorType) { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

class PlatformSensorProviderTest : public testing::Test {
 public:
  PlatformSensorProviderTest() {
    provider_ = std::make_unique<FakePlatformSensorProvider>();
  }

  PlatformSensorProviderTest(const PlatformSensorProviderTest&) = delete;
  PlatformSensorProviderTest& operator=(const PlatformSensorProviderTest&) =
      delete;

 protected:
  std::unique_ptr<FakePlatformSensorProvider> provider_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PlatformSensorProviderTest, ResourcesAreFreed) {
  EXPECT_CALL(*provider_, FreeResources()).Times(2);
  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> s) { EXPECT_TRUE(s); }));
  // Failure.
  EXPECT_CALL(*provider_, DoCreateSensorInternal(_, _, _))
      .WillOnce(
          Invoke([](mojom::SensorType, scoped_refptr<PlatformSensor>,
                    PlatformSensorProvider::CreateSensorCallback callback) {
            std::move(callback).Run(nullptr);
          }));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> s) { EXPECT_FALSE(s); }));
}

TEST_F(PlatformSensorProviderTest, ResourcesAreNotFreedOnPendingRequest) {
  EXPECT_CALL(*provider_, FreeResources()).Times(0);
  // Suspend.
  EXPECT_CALL(*provider_, DoCreateSensorInternal(_, _, _))
      .WillOnce(Invoke([](mojom::SensorType, scoped_refptr<PlatformSensor>,
                          PlatformSensorProvider::CreateSensorCallback) {}));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> s) { NOTREACHED(); }));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> s) { NOTREACHED(); }));
}

// This test verifies that the shared buffer's default values are 0.
TEST_F(PlatformSensorProviderTest, SharedBufferDefaultValue) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(mojom::SensorType::AMBIENT_LIGHT));

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.als.value, 0);
}

// This test verifies that when sensor is stopped, shared buffer contents are
// filled with default values.
TEST_F(PlatformSensorProviderTest, SharedBufferCleared) {
  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce([](scoped_refptr<PlatformSensor> sensor) {
        auto client =
            std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
        auto config = PlatformSensorConfiguration(10);

        EXPECT_TRUE(sensor->StartListening(client.get(), config));
        SensorReading reading;
        EXPECT_TRUE(sensor->GetLatestReading(&reading));
        EXPECT_THAT(reading.als.value, 10);

        EXPECT_TRUE(sensor->StopListening(client.get(), config));
        EXPECT_TRUE(sensor->GetLatestReading(&reading));
        EXPECT_THAT(reading.als.value, 0);
      }));
}

TEST_F(PlatformSensorProviderTest, PlatformSensorSignificanceChecks) {
  base::test::TestFuture<scoped_refptr<PlatformSensor>> future;
  provider_->CreateSensor(SensorType::AMBIENT_LIGHT, future.GetCallback());
  scoped_refptr<FakePlatformSensor> fake_sensor =
      static_cast<FakePlatformSensor*>(future.Get().get());

  // Override FakePlatformSensor's default StartSensor() expectation; we
  // do not want to do anything in StartSensor().
  ON_CALL(*fake_sensor, StartSensor(_))
      .WillByDefault(
          Invoke([&](const PlatformSensorConfiguration& configuration) {
            return true;
          }));

  auto client = std::make_unique<MockPlatformSensorClient>(fake_sensor);
  EXPECT_TRUE(fake_sensor->StartListening(client.get(),
                                          PlatformSensorConfiguration(10)));

  // This checks that illuminance significance check causes the following
  // to happen:
  // 1. Initial value is set to 24. And test checks it can be read back.
  // 2. New reading is attempted to set to 35.
  // 3. Value is read from sensor and compared new reading. But as new
  //    reading was not significantly different compared to initial, for
  //    privacy reasons, service returns the initial value.
  // 4. New value is set to 49. And test checks it can be read back. New
  //    value is allowed as it is significantly different compared to old
  //    value (24).
  // 5. New reading is attempted to set to 35.
  // 6. Value is read from sensor and compared new reading. But as new
  //    reading was not significantly different compared to initial, for
  //    privacy reasons, service returns the initial value.
  // 7. New value is set to 24. And test checks it can be read back. New
  //    value is allowed as it is significantly different compared to old
  //    value (49).
  const struct {
    const double attempted_als_value;
    const double expected_als_value;
  } kTestCases[] = {
      {24, 24}, {35, 24}, {49, 49}, {35, 49}, {24, 24},
  };

  for (const auto& test_case : kTestCases) {
    SensorReading reading;
    reading.raw.timestamp = 1.0;
    reading.als.value = test_case.attempted_als_value;

    if (reading.als.value == test_case.expected_als_value)
      AddNewReadingAndExpectReadingChangedEvent(client.get(), reading);
    else
      AddNewReadingAndExpectNoReadingChangedEvent(client.get(), reading);

    fake_sensor->GetLatestReading(&reading);
    EXPECT_EQ(reading.als.value, test_case.expected_als_value);
  }
}

}  // namespace device
