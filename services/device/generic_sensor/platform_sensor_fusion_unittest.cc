// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace device {

using mojom::SensorType;

class PlatformSensorFusionTest : public testing::Test {
 public:
  PlatformSensorFusionTest() {
    provider_ = std::make_unique<FakePlatformSensorProvider>();
  }

 protected:
  void AccelerometerCallback(scoped_refptr<PlatformSensor> sensor) {
    accelerometer_callback_called_ = true;
    accelerometer_ = static_cast<FakePlatformSensor*>(sensor.get());
  }

  void MagnetometerCallback(scoped_refptr<PlatformSensor> sensor) {
    magnetometer_callback_called_ = true;
    magnetometer_ = static_cast<FakePlatformSensor*>(sensor.get());
  }

  void PlatformSensorFusionCallback(scoped_refptr<PlatformSensor> sensor) {
    platform_sensor_fusion_callback_called_ = true;
    fusion_sensor_ = static_cast<PlatformSensorFusion*>(sensor.get());
  }

  void CreateAccelerometer() {
    auto callback = base::Bind(&PlatformSensorFusionTest::AccelerometerCallback,
                               base::Unretained(this));
    provider_->CreateSensor(SensorType::ACCELEROMETER, callback);
    EXPECT_TRUE(accelerometer_callback_called_);
    EXPECT_TRUE(accelerometer_);
    EXPECT_EQ(SensorType::ACCELEROMETER, accelerometer_->GetType());
  }

  void CreateMagnetometer() {
    auto callback = base::Bind(&PlatformSensorFusionTest::MagnetometerCallback,
                               base::Unretained(this));
    provider_->CreateSensor(SensorType::MAGNETOMETER, callback);
    EXPECT_TRUE(magnetometer_callback_called_);
    EXPECT_TRUE(magnetometer_);
    EXPECT_EQ(SensorType::MAGNETOMETER, magnetometer_->GetType());
  }

  void CreateLinearAccelerationFusionSensor() {
    auto fusion_algorithm =
        std::make_unique<LinearAccelerationFusionAlgorithmUsingAccelerometer>();
    CreateFusionSensor(std::move(fusion_algorithm));
  }

  void CreateAbsoluteOrientationEulerAnglesFusionSensor() {
    auto fusion_algorithm = std::make_unique<
        AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();
    CreateFusionSensor(std::move(fusion_algorithm));
  }

  void CreateFusionSensor(
      std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm) {
    auto callback =
        base::Bind(&PlatformSensorFusionTest::PlatformSensorFusionCallback,
                   base::Unretained(this));
    SensorType type = fusion_algorithm->fused_type();
    PlatformSensorFusion::Create(provider_->GetSensorReadingBuffer(type),
                                 provider_.get(), std::move(fusion_algorithm),
                                 callback);
    EXPECT_TRUE(platform_sensor_fusion_callback_called_);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakePlatformSensorProvider> provider_;
  bool accelerometer_callback_called_ = false;
  scoped_refptr<FakePlatformSensor> accelerometer_;
  bool magnetometer_callback_called_ = false;
  scoped_refptr<FakePlatformSensor> magnetometer_;
  bool platform_sensor_fusion_callback_called_ = false;
  scoped_refptr<PlatformSensorFusion> fusion_sensor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorFusionTest);
};

// The following code tests creating a fusion sensor that needs one source
// sensor.

TEST_F(PlatformSensorFusionTest, SourceSensorAlreadyExists) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  // Now the source sensor already exists.
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, SourceSensorWorksSeparately) {
  CreateAccelerometer();
  EXPECT_TRUE(accelerometer_);
  EXPECT_FALSE(accelerometer_->IsActiveForTesting());

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>();
  accelerometer_->AddClient(client.get());
  accelerometer_->StartListening(client.get(), PlatformSensorConfiguration(10));
  EXPECT_TRUE(accelerometer_->IsActiveForTesting());

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());
  EXPECT_FALSE(fusion_sensor_->IsActiveForTesting());

  fusion_sensor_->AddClient(client.get());
  fusion_sensor_->StartListening(client.get(), PlatformSensorConfiguration(10));
  EXPECT_TRUE(fusion_sensor_->IsActiveForTesting());

  fusion_sensor_->StopListening(client.get(), PlatformSensorConfiguration(10));
  EXPECT_FALSE(fusion_sensor_->IsActiveForTesting());

  EXPECT_TRUE(accelerometer_->IsActiveForTesting());

  accelerometer_->RemoveClient(client.get());
  EXPECT_FALSE(accelerometer_->IsActiveForTesting());

  fusion_sensor_->RemoveClient(client.get());
}

namespace {

void CheckConfigsCountForClient(const scoped_refptr<PlatformSensor>& sensor,
                                PlatformSensor::Client* client,
                                size_t expected_count) {
  auto client_entry = sensor->GetConfigMapForTesting().find(client);
  if (sensor->GetConfigMapForTesting().end() == client_entry) {
    EXPECT_EQ(0u, expected_count);
    return;
  }
  EXPECT_EQ(expected_count, client_entry->second.size());
}

}  // namespace

TEST_F(PlatformSensorFusionTest, SourceSensorDoesNotKeepOutdatedConfigs) {
  CreateAccelerometer();
  EXPECT_TRUE(accelerometer_);

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>(
      fusion_sensor_);
  fusion_sensor_->StartListening(client.get(), PlatformSensorConfiguration(10));
  fusion_sensor_->StartListening(client.get(), PlatformSensorConfiguration(20));
  fusion_sensor_->StartListening(client.get(), PlatformSensorConfiguration(30));

  EXPECT_EQ(1u, fusion_sensor_->GetConfigMapForTesting().size());
  EXPECT_EQ(1u, accelerometer_->GetConfigMapForTesting().size());

  CheckConfigsCountForClient(fusion_sensor_, client.get(), 3u);
  // Fusion sensor is a client for its sources, however it must keep only
  // one active configuration for them at a time.
  CheckConfigsCountForClient(accelerometer_, fusion_sensor_.get(), 1u);

  fusion_sensor_->StopListening(client.get(), PlatformSensorConfiguration(30));
  fusion_sensor_->StopListening(client.get(), PlatformSensorConfiguration(20));

  CheckConfigsCountForClient(fusion_sensor_, client.get(), 1u);
  CheckConfigsCountForClient(accelerometer_, fusion_sensor_.get(), 1u);

  fusion_sensor_->StopListening(client.get(), PlatformSensorConfiguration(10));

  CheckConfigsCountForClient(fusion_sensor_, client.get(), 0u);
  CheckConfigsCountForClient(accelerometer_, fusion_sensor_.get(), 0u);
}

TEST_F(PlatformSensorFusionTest, AllSourceSensorsStoppedOnSingleSourceFailure) {
  CreateAccelerometer();
  EXPECT_TRUE(accelerometer_);

  CreateMagnetometer();
  EXPECT_TRUE(magnetometer_);
  // Magnetometer will be started after Accelerometer for the given
  // sensor fusion algorithm.
  ON_CALL(*magnetometer_, StartSensor(_)).WillByDefault(Return(false));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>(
      fusion_sensor_);
  fusion_sensor_->StartListening(client.get(), PlatformSensorConfiguration(10));

  EXPECT_FALSE(fusion_sensor_->IsActiveForTesting());
  EXPECT_FALSE(accelerometer_->IsActiveForTesting());
  EXPECT_FALSE(magnetometer_->IsActiveForTesting());
}

TEST_F(PlatformSensorFusionTest, SourceSensorNeedsToBeCreated) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, SourceSensorIsNotAvailable) {
  // Accelerometer is not available.
  ON_CALL(*provider_, DoCreateSensorInternal(SensorType::ACCELEROMETER, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateLinearAccelerationFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

// The following code tests creating a fusion sensor that needs two source
// sensors.

TEST_F(PlatformSensorFusionTest, BothSourceSensorsAlreadyExist) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));

  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));
  CreateMagnetometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::MAGNETOMETER));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, BothSourceSensorsNeedToBeCreated) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, BothSourceSensorsAreNotAvailable) {
  // Failure.
  ON_CALL(*provider_, DoCreateSensorInternal(_, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorAlreadyExistsTheOtherSourceSensorNeedsToBeCreated) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));
  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorAlreadyExistsTheOtherSourceSensorIsNotAvailable) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));

  // Magnetometer is not available.
  ON_CALL(*provider_, DoCreateSensorInternal(SensorType::MAGNETOMETER, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorNeedsToBeCreatedTheOtherSourceSensorIsNotAvailable) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  // Magnetometer is not available.
  ON_CALL(*provider_, DoCreateSensorInternal(SensorType::MAGNETOMETER, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

TEST_F(PlatformSensorFusionTest,
       FusionSensorMaximumSupportedFrequencyIsTheMaximumOfItsSourceSensors) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  scoped_refptr<PlatformSensor> accelerometer =
      provider_->GetSensor(SensorType::ACCELEROMETER);
  EXPECT_TRUE(accelerometer);
  static_cast<FakePlatformSensor*>(accelerometer.get())
      ->set_maximum_supported_frequency(30.0);

  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));
  CreateMagnetometer();
  scoped_refptr<PlatformSensor> magnetometer =
      provider_->GetSensor(SensorType::MAGNETOMETER);
  EXPECT_TRUE(magnetometer);
  static_cast<FakePlatformSensor*>(magnetometer.get())
      ->set_maximum_supported_frequency(20.0);

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
  EXPECT_EQ(30.0, fusion_sensor_->GetMaximumSupportedFrequency());
  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>(
      fusion_sensor_);
  EXPECT_TRUE(fusion_sensor_->StartListening(
      client.get(), PlatformSensorConfiguration(30.0)));
}

}  //  namespace device
