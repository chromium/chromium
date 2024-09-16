// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/generic_sensor/platform_sensor_fusion.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace device {

using mojom::SensorType;

namespace {

void ExpectNoReadingChangedEvent(MockPlatformSensorClient* sensor_client,
                                 mojom::SensorType sensor_type) {
  base::RunLoop run_loop;
  EXPECT_CALL(*sensor_client, OnSensorReadingChanged(sensor_type)).Times(0);
  run_loop.RunUntilIdle();
}

void ExpectReadingChangedEvent(MockPlatformSensorClient* sensor_client,
                               mojom::SensorType sensor_type) {
  base::RunLoop run_loop;
  EXPECT_CALL(*sensor_client, OnSensorReadingChanged(sensor_type))
      .WillOnce(Invoke([&](SensorType) { run_loop.Quit(); }));
  run_loop.Run();
}

// Attempts to add a new reading to the sensor owned by |sensor_client|, and
// asserts that it does not lead to OnSensorReadingChanged() being called (i.e.
// PlatformSensor's significance check has failed).
void AddNewReadingAndExpectNoReadingChangedEvent(
    MockPlatformSensorClient* sensor_client,
    const SensorReading& new_reading,
    mojom::SensorType sensor_type) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      static_cast<FakePlatformSensor*>(sensor_client->sensor().get());
  fake_sensor->AddNewReading(new_reading);
  ExpectNoReadingChangedEvent(sensor_client, sensor_type);
}

// Add a new reading to the sensor owned by |sensor_client|, and expect reading
// change event.
void AddNewReadingAndExpectReadingChangedEvent(
    MockPlatformSensorClient* sensor_client,
    const SensorReading& new_reading,
    mojom::SensorType sensor_type) {
  scoped_refptr<FakePlatformSensor> fake_sensor =
      static_cast<FakePlatformSensor*>(sensor_client->sensor().get());
  fake_sensor->AddNewReading(new_reading);
  ExpectReadingChangedEvent(sensor_client, sensor_type);
}

void FusionAlgorithmCopyLowLevelValues(const SensorReading& low_level_reading,
                                       SensorReading* fused_reading) {
  fused_reading->raw.values[0] = low_level_reading.raw.values[0];
  fused_reading->raw.values[1] = low_level_reading.raw.values[1];
  fused_reading->raw.values[2] = low_level_reading.raw.values[2];
}

void FusionAlgorithmSubtractEpsilonFromX(const SensorReading& low_level_reading,
                                         SensorReading* fused_reading) {
  fused_reading->raw.values[0] = low_level_reading.raw.values[0] - kEpsilon;
  fused_reading->raw.values[1] = low_level_reading.raw.values[1];
  fused_reading->raw.values[2] = low_level_reading.raw.values[2];
}

// A PlatformSensorFusionAlgorithm whose fusion algorithm can be customized
// at runtime via set_fusion_function().
class CustomizableFusionAlgorithm : public PlatformSensorFusionAlgorithm {
 public:
  using FusionFunction =
      base::RepeatingCallback<void(const SensorReading& low_level_reading,
                                   SensorReading* fused_reading)>;
  static constexpr mojom::SensorType kLowLevelSensorType =
      SensorType::ACCELEROMETER;
  static constexpr mojom::SensorType kFusionSensorType = SensorType::GRAVITY;

  CustomizableFusionAlgorithm()
      : PlatformSensorFusionAlgorithm(kFusionSensorType,
                                      {kLowLevelSensorType}) {}
  ~CustomizableFusionAlgorithm() override = default;

  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override {
    EXPECT_EQ(which_sensor_changed, kLowLevelSensorType);

    SensorReading low_level_reading;
    EXPECT_TRUE(fusion_sensor_->GetSourceReading(kLowLevelSensorType,
                                                 &low_level_reading));

    fusion_function_.Run(low_level_reading, fused_reading);
    return true;
  }

  void set_fusion_function(FusionFunction fusion_function) {
    fusion_function_ = std::move(fusion_function);
  }

 private:
  FusionFunction fusion_function_;
};

}  // namespace

class PlatformSensorFusionTest : public testing::Test {
 public:
  PlatformSensorFusionTest() {
    provider_ = std::make_unique<NiceMock<FakePlatformSensorProvider>>();
  }

  PlatformSensorFusionTest(const PlatformSensorFusionTest&) = delete;
  PlatformSensorFusionTest& operator=(const PlatformSensorFusionTest&) = delete;

 protected:
  void CreateAccelerometer() {
    TestFuture<scoped_refptr<PlatformSensor>> future;
    provider_->CreateSensor(SensorType::ACCELEROMETER, future.GetCallback());
    accelerometer_ = static_cast<FakePlatformSensor*>(future.Get().get());
    EXPECT_TRUE(accelerometer_);
    EXPECT_EQ(SensorType::ACCELEROMETER, accelerometer_->GetType());
  }

  void CreateMagnetometer() {
    TestFuture<scoped_refptr<PlatformSensor>> future;
    provider_->CreateSensor(SensorType::MAGNETOMETER, future.GetCallback());
    magnetometer_ = static_cast<FakePlatformSensor*>(future.Get().get());
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
    TestFuture<scoped_refptr<PlatformSensor>> future;
    PlatformSensorFusion::Create(provider_->AsWeakPtr(),
                                 std::move(fusion_algorithm),
                                 future.GetCallback());
    fusion_sensor_ = static_cast<PlatformSensorFusion*>(future.Get().get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NiceMock<FakePlatformSensorProvider>> provider_;
  scoped_refptr<FakePlatformSensor> accelerometer_;
  scoped_refptr<FakePlatformSensor> magnetometer_;
  scoped_refptr<PlatformSensorFusion> fusion_sensor_;
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

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>();
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

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fusion_sensor_);
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

  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fusion_sensor_);
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
  ON_CALL(*provider_, CreateSensorInternal(SensorType::ACCELEROMETER, _))
      .WillByDefault(
          Invoke([](mojom::SensorType,
                    FakePlatformSensorProvider::CreateSensorCallback callback) {
            std::move(callback).Run(nullptr);
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
  ON_CALL(*provider_, CreateSensorInternal)
      .WillByDefault(
          Invoke([](mojom::SensorType,
                    FakePlatformSensorProvider::CreateSensorCallback callback) {
            std::move(callback).Run(nullptr);
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
  ON_CALL(*provider_, CreateSensorInternal(SensorType::MAGNETOMETER, _))
      .WillByDefault(
          Invoke([](mojom::SensorType,
                    FakePlatformSensorProvider::CreateSensorCallback callback) {
            std::move(callback).Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorNeedsToBeCreatedTheOtherSourceSensorIsNotAvailable) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  // Magnetometer is not available.
  ON_CALL(*provider_, CreateSensorInternal(SensorType::MAGNETOMETER, _))
      .WillByDefault(
          Invoke([](mojom::SensorType,
                    FakePlatformSensorProvider::CreateSensorCallback callback) {
            std::move(callback).Run(nullptr);
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
  auto client =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fusion_sensor_);
  EXPECT_TRUE(fusion_sensor_->StartListening(
      client.get(), PlatformSensorConfiguration(30.0)));
}

TEST_F(PlatformSensorFusionTest, FusionIsSignificantlyDifferent) {
  // Due to inaccuracy of calculations between doubles, difference between two
  // input values has to be bigger than the threshold value used in
  // significantly different check.
  CreateLinearAccelerationFusionSensor();
  const auto* const fusion_algorithm = fusion_sensor_->fusion_algorithm();

  const double kValueToFlipThreshold = fusion_algorithm->threshold() + kEpsilon;
  const double kValueNotToFlipThreshold =
      fusion_algorithm->threshold() - kEpsilon;
  SensorReading reading1;
  SensorReading reading2;
  // Made up test values.
  reading1.accel.x = reading2.accel.x = 0.1;
  reading1.accel.y = reading2.accel.y = 0.5;
  reading1.accel.z = reading2.accel.z = 10.0;

  // Compared values are same.
  // reading1: 0.1, 0.5, 10.0
  // reading2: 0.1, 0.5, 10.0
  EXPECT_FALSE(fusion_sensor_->IsSignificantlyDifferent(
      reading1, reading2, fusion_sensor_->GetType()));

  // Compared values do not significantly differ from each other.
  // reading1: 0.1, 0.5, 10.0
  // reading2: 0.1, 0.5, 10.00001
  reading2.accel.z = reading2.accel.z + kValueNotToFlipThreshold;
  EXPECT_FALSE(fusion_sensor_->IsSignificantlyDifferent(
      reading1, reading2, fusion_sensor_->GetType()));

  // Compared values significantly differ from each other.
  // reading1: 0.1, 0.5, 10.0
  // reading2: 0.1, 0.5, 10.11001
  reading2.accel.z = reading2.accel.z + kValueToFlipThreshold;
  EXPECT_TRUE(fusion_sensor_->IsSignificantlyDifferent(
      reading1, reading2, fusion_sensor_->GetType()));
}

TEST_F(PlatformSensorFusionTest, OnSensorReadingChanged) {
  // Accelerometer is selected as low-level sensor.
  CreateAccelerometer();
  EXPECT_TRUE(accelerometer_);

  ON_CALL(*accelerometer_, StartSensor(_)).WillByDefault(Return(true));

  auto client_low_level_ =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(accelerometer_);

  CreateFusionSensor(std::make_unique<CustomizableFusionAlgorithm>());
  ASSERT_TRUE(fusion_sensor_);
  auto* fusion_algorithm = static_cast<CustomizableFusionAlgorithm*>(
      fusion_sensor_->fusion_algorithm());
  EXPECT_EQ(CustomizableFusionAlgorithm::kFusionSensorType,
            fusion_sensor_->GetType());

  auto client_fusion =
      std::make_unique<NiceMock<MockPlatformSensorClient>>(fusion_sensor_);
  fusion_sensor_->StartListening(client_fusion.get(),
                                 PlatformSensorConfiguration(10));

  // Made up test values.
  const double kTestValueX = 0.6;
  const double kTestValueY = 0.9;
  const double kTestValueZ = 1.1;
  const double kValueToFlipThreshold = fusion_algorithm->threshold() + kEpsilon;
  const double kValueToFlipThresholdRounded = fusion_algorithm->threshold();

  struct TestSensorReading {
    const struct {
      double x;
      double y;
      double z;
    } input, expected;
    const bool expect_reading_changed_event;
  };

  const struct {
    TestSensorReading low_level;
    TestSensorReading fusion;
    CustomizableFusionAlgorithm::FusionFunction fusion_function;
  } kTestSteps[] = {
      // Test set 1
      // Triggers low-level and fusion reading as initial sensor
      // values are zero.
      {// Low-level sensor
       {{kTestValueX, kTestValueY, kTestValueZ},
        {kTestValueX, kTestValueY, kTestValueZ},
        true},
       // Fusion sensor
       {{kTestValueX, kTestValueY, kTestValueZ},
        {kTestValueX, kTestValueY, kTestValueZ},
        true},
       base::BindRepeating(&FusionAlgorithmCopyLowLevelValues)},

      // Test set 2
      // Doesn't trigger low-level reading event as rounded value is same as
      // earlier. Because of that fusion sensor event is not either triggered.
      {// Low-level sensor
       {{kTestValueX + kEpsilon, kTestValueY, kTestValueZ},
        {kTestValueX, kTestValueY, kTestValueZ},
        false},
       // Fusion sensor
       {{kTestValueX, kTestValueY, kTestValueZ},
        {kTestValueX, kTestValueY, kTestValueZ},
        false},
       base::BindRepeating(&FusionAlgorithmSubtractEpsilonFromX)},

      // Test set 3
      // In current code as fusion sensor values are rounded before
      // PlatformSensorFusionAlgorithm::IsReadingSignificantlyDifferent() call
      // the difference between values must much bigger than threshold value.
      {// Low-level sensor
       {{kTestValueX + kValueToFlipThreshold, kTestValueY, kTestValueZ},
        {kTestValueX + kValueToFlipThresholdRounded, kTestValueY, kTestValueZ},
        true},
       // Fusion sensor
       {{kTestValueX + kValueToFlipThreshold, kTestValueY, kTestValueZ},
        {kTestValueX + kValueToFlipThresholdRounded, kTestValueY, kTestValueZ},
        true},
       base::BindRepeating(&FusionAlgorithmCopyLowLevelValues)},
  };

  for (const auto& test_step : kTestSteps) {
    fusion_algorithm->set_fusion_function(test_step.fusion_function);

    // First add low-level sensor readings.
    SensorReading reading;
    reading.accel.x = test_step.low_level.input.x;
    reading.accel.y = test_step.low_level.input.y;
    reading.accel.z = test_step.low_level.input.z;

    // Code checks if PlatformSensor::OnSensorReadingChanged() is called
    // or not called as expected.
    if (test_step.low_level.expect_reading_changed_event) {
      AddNewReadingAndExpectReadingChangedEvent(
          client_low_level_.get(), reading,
          CustomizableFusionAlgorithm::kLowLevelSensorType);
    } else {
      AddNewReadingAndExpectNoReadingChangedEvent(
          client_low_level_.get(), reading,
          CustomizableFusionAlgorithm::kLowLevelSensorType);
    }

    if (test_step.fusion.expect_reading_changed_event) {
      ExpectReadingChangedEvent(client_fusion.get(),
                                CustomizableFusionAlgorithm::kFusionSensorType);
    } else {
      ExpectNoReadingChangedEvent(
          client_fusion.get(), CustomizableFusionAlgorithm::kFusionSensorType);
    }

    // Once new values are added, we can check that low-level sensors and
    // fusion sensor have correct values.
    // Check rounded low-level sensor values.
    EXPECT_TRUE(accelerometer_->GetLatestReading(&reading));
    EXPECT_DOUBLE_EQ(test_step.low_level.expected.x, reading.accel.x);
    EXPECT_DOUBLE_EQ(test_step.low_level.expected.y, reading.accel.y);
    EXPECT_DOUBLE_EQ(test_step.low_level.expected.z, reading.accel.z);

    // Check raw low-level sensor values.
    EXPECT_TRUE(accelerometer_->GetLatestRawReading(&reading));
    EXPECT_DOUBLE_EQ(test_step.low_level.input.x, reading.accel.x);
    EXPECT_DOUBLE_EQ(test_step.low_level.input.y, reading.accel.y);
    EXPECT_DOUBLE_EQ(test_step.low_level.input.z, reading.accel.z);

    // Check rounded fusion sensor values.
    EXPECT_TRUE(fusion_sensor_->GetLatestReading(&reading));
    EXPECT_DOUBLE_EQ(test_step.fusion.expected.x, reading.accel.x);
    EXPECT_DOUBLE_EQ(test_step.fusion.expected.y, reading.accel.y);
    EXPECT_DOUBLE_EQ(test_step.fusion.expected.z, reading.accel.z);

    // Check raw fusion sensor values.
    EXPECT_TRUE(fusion_sensor_->GetLatestRawReading(&reading));
    EXPECT_DOUBLE_EQ(test_step.fusion.input.x, reading.accel.x);
    EXPECT_DOUBLE_EQ(test_step.fusion.input.y, reading.accel.y);
    EXPECT_DOUBLE_EQ(test_step.fusion.input.z, reading.accel.z);
  }
}

TEST_F(PlatformSensorFusionTest, ProviderDestroyedWhileCreatingFusedSensor) {
  // In a real PlatformSensorProvider implementation, creating the sensor is an
  // asynchronous task. Delay invoking the callback to simulate that behavior.
  TestFuture<base::OnceClosure> finish_create_sensor_future;
  EXPECT_CALL(*provider_, CreateSensorInternal)
      .WillOnce([&](mojom::SensorType type,
                    PlatformSensorProvider::CreateSensorCallback callback) {
        finish_create_sensor_future.SetValue(
            base::BindOnce(std::move(callback),
                           base::MakeRefCounted<FakePlatformSensor>(
                               type, provider_->GetSensorReadingBuffer(type),
                               provider_->AsWeakPtr())));
      });

  TestFuture<scoped_refptr<PlatformSensor>> create_future;
  PlatformSensorFusion::Create(
      provider_->AsWeakPtr(),
      std::make_unique<LinearAccelerationFusionAlgorithmUsingAccelerometer>(),
      create_future.GetCallback());
  ASSERT_TRUE(finish_create_sensor_future.Wait());
  EXPECT_FALSE(create_future.IsReady());

  // Reset the sensor provider before the fused sensor is created and check that
  // the CreateSensor callback is invoked with nullptr.
  provider_.reset();
  EXPECT_FALSE(create_future.Get());

  // Run the delayed callback. Nothing should happen since the provider is
  // already destroyed.
  finish_create_sensor_future.Take().Run();
}

}  //  namespace device
