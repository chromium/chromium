// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace device {

class PlatformSensorProviderTest : public testing::Test {
 public:
  PlatformSensorProviderTest() {
    provider_ = std::make_unique<FakePlatformSensorProvider>();
  }

 protected:
  std::unique_ptr<FakePlatformSensorProvider> provider_;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderTest);
};

TEST_F(PlatformSensorProviderTest, ResourcesAreFreed) {
  EXPECT_CALL(*provider_, FreeResources()).Times(2);
  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { EXPECT_TRUE(s); }));
  // Failure.
  EXPECT_CALL(*provider_, DoCreateSensorInternal(_, _, _))
      .WillOnce(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const PlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { EXPECT_FALSE(s); }));
}

TEST_F(PlatformSensorProviderTest, ResourcesAreNotFreedOnPendingRequest) {
  EXPECT_CALL(*provider_, FreeResources()).Times(0);
  // Suspend.
  EXPECT_CALL(*provider_, DoCreateSensorInternal(_, _, _))
      .WillOnce(
          Invoke([](mojom::SensorType, scoped_refptr<PlatformSensor>,
                    const PlatformSensorProvider::CreateSensorCallback&) {}));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { NOTREACHED(); }));

  provider_->CreateSensor(
      mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { NOTREACHED(); }));
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
      base::Bind([](scoped_refptr<PlatformSensor> sensor) {
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

}  // namespace device
