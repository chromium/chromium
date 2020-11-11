// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_chromeos.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "chromeos/components/sensors/sensor_hal_dispatcher.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr int kFakeDeviceId = 1;

constexpr double kScaleValue = 10.0;
constexpr char kWrongScale[] = "10..0";

constexpr char kWrongLocation[] = "basee";

}  // namespace

class PlatformSensorProviderChromeOSTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chromeos::sensors::SensorHalDispatcher::Initialize();

    sensor_hal_server_ =
        std::make_unique<chromeos::sensors::FakeSensorHalServer>();
    provider_ = std::make_unique<PlatformSensorProviderChromeOS>();
  }

  void TearDown() override {
    chromeos::sensors::SensorHalDispatcher::Shutdown();
  }

  void AddDevice(int32_t iio_device_id,
                 chromeos::sensors::mojom::DeviceType type,
                 const base::Optional<std::string>& scale,
                 const base::Optional<std::string>& location) {
    AddDevice(iio_device_id,
              std::set<chromeos::sensors::mojom::DeviceType>{type},
              std::move(scale), std::move(location));
  }

  void AddDevice(int32_t iio_device_id,
                 std::set<chromeos::sensors::mojom::DeviceType> types,
                 const base::Optional<std::string>& scale,
                 const base::Optional<std::string>& location) {
    auto sensor_device = std::make_unique<chromeos::sensors::FakeSensorDevice>(
        std::vector<chromeos::sensors::FakeSensorDevice::ChannelData>{});

    if (scale.has_value()) {
      sensor_device->SetAttribute(chromeos::sensors::mojom::kScale,
                                  scale.value());
    }
    if (location.has_value()) {
      sensor_device->SetAttribute(chromeos::sensors::mojom::kLocation,
                                  location.value());
    }

    sensor_devices_.push_back(sensor_device.get());

    sensor_hal_server_->GetSensorService()->SetDevice(
        iio_device_id, std::move(types), std::move(sensor_device));
  }

  // Sensor creation is asynchronous, therefore inner loop is used to wait for
  // PlatformSensorProvider::CreateSensorCallback completion.
  scoped_refptr<PlatformSensor> CreateSensor(mojom::SensorType type) {
    scoped_refptr<PlatformSensor> sensor;
    base::RunLoop run_loop;
    provider_->CreateSensor(type,
                            base::BindLambdaForTesting(
                                [&](scoped_refptr<PlatformSensor> new_sensor) {
                                  if (new_sensor)
                                    EXPECT_EQ(type, new_sensor->GetType());

                                  sensor = std::move(new_sensor);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
    return sensor;
  }

  void StartConnection() {
    chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
        sensor_hal_server_->PassRemote());
  }

  void ResetClient() {
    // Similar to provider_->OnSensorHalClientFailure, but without the delay.
    provider_->ResetSensorService();
    provider_->sensor_hal_client_.reset();
    provider_->RegisterSensorClient();
  }

  std::unique_ptr<chromeos::sensors::FakeSensorHalServer> sensor_hal_server_;
  std::vector<chromeos::sensors::FakeSensorDevice*> sensor_devices_;

  std::unique_ptr<PlatformSensorProviderChromeOS> provider_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PlatformSensorProviderChromeOSTest, CheckUnsupportedTypes) {
  int fake_id = 1;
  // Containing at least one supported device type.
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++,
            std::set<chromeos::sensors::mojom::DeviceType>{
                chromeos::sensors::mojom::DeviceType::ANGLVEL,
                chromeos::sensors::mojom::DeviceType::COUNT},
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  // All device types are unsupported.
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::COUNT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++,
            std::set<chromeos::sensors::mojom::DeviceType>{
                chromeos::sensors::mojom::DeviceType::ANGL,
                chromeos::sensors::mojom::DeviceType::BARO},
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));

  EXPECT_FALSE(provider_->sensors_[1].ignored);
  EXPECT_FALSE(provider_->sensors_[2].ignored);
  EXPECT_TRUE(provider_->sensors_[3].ignored);
  EXPECT_TRUE(provider_->sensors_[4].ignored);
  EXPECT_TRUE(provider_->sensors_[5].ignored);
}

TEST_F(PlatformSensorProviderChromeOSTest, MissingScale) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            /*scale=*/base::nullopt, chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, MissingLocation) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            /*location=*/base::nullopt);

  StartConnection();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, WrongScale) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            kWrongScale, chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, WrongLocation) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue), kWrongLocation);

  StartConnection();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, CheckMainLocationBase) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  // Will not be used.
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::MAGN,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::GYROSCOPE));

  // Wait until the disconnect of the gyroscope arrives at FakeSensorDevice.
  base::RunLoop().RunUntilIdle();

  // Remote stored in |provider_|.
  EXPECT_TRUE(sensor_devices_[0]->HasReceivers());
  EXPECT_TRUE(sensor_devices_[3]->HasReceivers());
  // Removed in |provider_| as it'll never be used.
  EXPECT_FALSE(sensor_devices_[1]->HasReceivers());
}

TEST_F(PlatformSensorProviderChromeOSTest, CheckMainLocationLid) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::MAGN,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationCamera);

  StartConnection();

  // Wait until the disconnect of the first gyroscope arrives at
  // FakeSensorDevice.
  base::RunLoop().RunUntilIdle();

  // Removed in |provider_| as they'll never be used.
  EXPECT_FALSE(sensor_devices_[0]->HasReceivers());
  EXPECT_FALSE(sensor_devices_[2]->HasReceivers());
  EXPECT_FALSE(sensor_devices_[4]->HasReceivers());
  // Remote stored in |provider_|.
  EXPECT_TRUE(sensor_devices_[1]->HasReceivers());
  EXPECT_TRUE(sensor_devices_[3]->HasReceivers());
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckMainLocationLidWithMultitypeSensor) {
  int fake_id = 1;
  AddDevice(fake_id++,
            std::set<chromeos::sensors::mojom::DeviceType>{
                chromeos::sensors::mojom::DeviceType::ACCEL,
                chromeos::sensors::mojom::DeviceType::ANGLVEL},
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  // Wait until the disconnect of the gyroscope arrives at FakeSensorDevice.
  base::RunLoop().RunUntilIdle();

  // Remote stored in |provider_|.
  EXPECT_TRUE(sensor_devices_[0]->HasReceivers());
  // Removed in |provider_| as it'll never be used.
  EXPECT_FALSE(sensor_devices_[1]->HasReceivers());

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));
  EXPECT_TRUE(CreateSensor(mojom::SensorType::GYROSCOPE));
}

TEST_F(PlatformSensorProviderChromeOSTest, CheckAmbientLightSensorLocationLid) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::LIGHT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::LIGHT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  StartConnection();

  // Wait until the disconnect of the first ambient light sensor arrives at
  // FakeSensorDevice.
  base::RunLoop().RunUntilIdle();

  // Removed in |provider_| as it'll never be used.
  EXPECT_FALSE(sensor_devices_[0]->HasReceivers());
  // Remote stored in |provider_|.
  EXPECT_TRUE(sensor_devices_[1]->HasReceivers());
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckAmbientLightSensorLocationBase) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::LIGHT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::AMBIENT_LIGHT));
}

TEST_F(PlatformSensorProviderChromeOSTest, Reconnect) {
  int fake_id = 1;

  // Will not be used.
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::LIGHT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));

  // Simulate a disconnection between |provider_| and the dispatcher.
  ResetClient();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::GYROSCOPE));

  // Simulate a disconnection of IIO Service.
  sensor_hal_server_->GetSensorService()->OnServiceDisconnect();
  sensor_hal_server_->OnServerDisconnect();
  // Remove the stored Mojo remote of the ambient light sensor.
  sensor_devices_.back()->ClearReceivers();

  // Wait until the disconnect arrives at the dispatcher.
  base::RunLoop().RunUntilIdle();

  StartConnection();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::AMBIENT_LIGHT));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckLinearAccelerationSensorNotCreatedIfNoAccelerometer) {
  StartConnection();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::LINEAR_ACCELERATION));
}

TEST_F(PlatformSensorProviderChromeOSTest, CheckLinearAcceleration) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::LINEAR_ACCELERATION));
}

TEST_F(
    PlatformSensorProviderChromeOSTest,
    CheckAbsoluteOrientationSensorNotCreatedIfNoAccelerometerAndNoMagnetometer) {
  StartConnection();

  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckAbsoluteOrientationSensorNotCreatedIfNoAccelerometer) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::MAGN,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckAbsoluteOrientationSensorNotCreatedIfNoMagnetometer) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest, CheckAbsoluteOrientationSensors) {
  int fake_id = 1;

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::MAGN,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
}

TEST_F(
    PlatformSensorProviderChromeOSTest,
    CheckRelativeOrientationSensorNotCreatedIfNoAccelerometerAndNoGyroscope) {
  StartConnection();

  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckRelativeOrientationSensorNotCreatedIfNoAccelerometer) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
  EXPECT_FALSE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckRelativeOrientationSensorUsingAccelerometer) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckRelativeOrientationSensorUsingAccelerometerAndGyroscope) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  StartConnection();

  EXPECT_TRUE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION));
}

}  // namespace device
