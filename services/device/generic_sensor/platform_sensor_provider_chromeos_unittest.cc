// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/generic_sensor/platform_sensor_provider_chromeos.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "services/device/generic_sensor/sensor_impl.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace device {

namespace {

constexpr int kFakeDeviceId = 1;

constexpr double kScaleValue = 10.0;
constexpr char kWrongScale[] = "10..0";

constexpr char kWrongLocation[] = "basee";

constexpr int64_t kFakeSampleData = 1;
constexpr int64_t kFakeTimestampData = 163176689212344ll;

// The number of axes for which there are accelerometer and gyroscope readings.
constexpr uint32_t kNumberOfAxes = 3u;

constexpr char kAccelerometerChannels[][10] = {"accel_x", "accel_y", "accel_z"};
constexpr char kGyroscopeChannels[][10] = {"anglvel_x", "anglvel_y",
                                           "anglvel_z"};

class FakeClient : public PlatformSensor::Client {
 public:
  explicit FakeClient(PlatformSensor* platform_sensor)
      : platform_sensor_(platform_sensor) {}
  ~FakeClient() override {}

  void OnSensorReadingChanged(mojom::SensorType type) override {}
  void OnSensorError() override { platform_sensor_->RemoveClient(this); }
  bool IsSuspended() override { return false; }

 private:
  raw_ptr<PlatformSensor> platform_sensor_;
};

}  // namespace

class PlatformSensorProviderChromeOSTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::sensors::SensorHalDispatcher::Initialize();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    sensor_hal_server_ =
        std::make_unique<chromeos::sensors::FakeSensorHalServer>();
    provider_ = std::make_unique<PlatformSensorProviderChromeOS>();
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::sensors::SensorHalDispatcher::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void AddDevice(int32_t iio_device_id,
                 chromeos::sensors::mojom::DeviceType type,
                 const std::optional<std::string>& scale,
                 const std::optional<std::string>& location,
                 std::vector<chromeos::sensors::FakeSensorDevice::ChannelData>
                     channels_data = {}) {
    AddDevice(iio_device_id,
              std::set<chromeos::sensors::mojom::DeviceType>{type},
              std::move(scale), std::move(location), std::move(channels_data));
  }

  void AddDevice(int32_t iio_device_id,
                 std::set<chromeos::sensors::mojom::DeviceType> types,
                 const std::optional<std::string>& scale,
                 const std::optional<std::string>& location,
                 std::vector<chromeos::sensors::FakeSensorDevice::ChannelData>
                     channels_data = {}) {
    auto sensor_device = std::make_unique<chromeos::sensors::FakeSensorDevice>(
        std::move(channels_data));

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

  std::vector<chromeos::sensors::FakeSensorDevice::ChannelData>
  GetChannelsWithAxes(const char channels[][10]) {
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data(
        kNumberOfAxes + 1);
    for (uint32_t i = 0; i < kNumberOfAxes; ++i) {
      channels_data[i].id = channels[i];
      channels_data[i].sample_data = kFakeSampleData;
    }
    channels_data.back().id = chromeos::sensors::mojom::kTimestampChannel;
    channels_data.back().sample_data = kFakeTimestampData;

    return channels_data;
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

  void RegisterSensorHalServer() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // MojoConnectionServiceProvider::BootstrapMojoConnectionForIioService is
    // responsible for calling this outside unit tests.
    // This will eventually call PlatformSensorProviderChromeOS::SetUpChannel().
    chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
        sensor_hal_server_->PassRemote());
#else
    // As SensorHalDispatcher is only defined in ash, manually setting up Mojo
    // connection between |fake_sensor_hal_server_| and |provider_|.
    // This code is duplicating what SensorHalDispatcher::EstablishMojoChannel()
    // does.
    mojo::PendingRemote<chromeos::sensors::mojom::SensorService> pending_remote;
    sensor_hal_server_->CreateChannel(
        pending_remote.InitWithNewPipeAndPassReceiver());
    provider_->SetUpChannel(std::move(pending_remote));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  std::unique_ptr<chromeos::sensors::FakeSensorHalServer> sensor_hal_server_;
  std::vector<raw_ptr<chromeos::sensors::FakeSensorDevice, VectorExperimental>>
      sensor_devices_;

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

  RegisterSensorHalServer();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));

  EXPECT_FALSE(provider_->sensors_[1].ignored);
  EXPECT_FALSE(provider_->sensors_[2].ignored);
  EXPECT_TRUE(provider_->sensors_[3].ignored);
  EXPECT_TRUE(provider_->sensors_[4].ignored);
  EXPECT_TRUE(provider_->sensors_[5].ignored);
}

TEST_F(PlatformSensorProviderChromeOSTest, MissingScale) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            /*scale=*/std::nullopt, chromeos::sensors::mojom::kLocationBase);

  RegisterSensorHalServer();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, MissingLocation) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            /*location=*/std::nullopt);

  RegisterSensorHalServer();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, WrongScale) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            kWrongScale, chromeos::sensors::mojom::kLocationBase);

  RegisterSensorHalServer();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, WrongLocation) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue), kWrongLocation);

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::AMBIENT_LIGHT));
}

TEST_F(PlatformSensorProviderChromeOSTest, SensorDeviceDisconnect) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid,
            GetChannelsWithAxes(kAccelerometerChannels));
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  RegisterSensorHalServer();

  auto accel_lid = CreateSensor(mojom::SensorType::ACCELEROMETER);
  EXPECT_TRUE(accel_lid);

  // Wait until all tasks are done and no failures occur in |provider_| or
  // |accel_lid|.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(sensor_devices_.front()->HasReceivers());

  // Simulate a disconnection of an existing SensorDevice in |provider_|, which
  // triggers PlatformSensorProviderChromeOS::OnSensorDeviceDisconnect(). The
  // default reason is IIOSERVICE_CRASHED.
  sensor_devices_.back()->ClearReceivers();

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();
  // PlatformSensorProviderChromeOS::OnSensorDeviceDisconnect() resets the
  // SensorService Mojo channel.
  EXPECT_FALSE(sensor_hal_server_->GetSensorService()->HasReceivers());

  // The existing PlatformSensors will also be reset.
  EXPECT_FALSE(sensor_devices_.front()->HasReceivers());
}

TEST_F(PlatformSensorProviderChromeOSTest, SensorDeviceDisconnectWithReason) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid,
            GetChannelsWithAxes(kAccelerometerChannels));
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid,
            GetChannelsWithAxes(kGyroscopeChannels));
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase,
            GetChannelsWithAxes(kAccelerometerChannels));
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  RegisterSensorHalServer();

  auto accel_lid = CreateSensor(mojom::SensorType::ACCELEROMETER);
  EXPECT_TRUE(accel_lid);
  base::RunLoop().RunUntilIdle();

  // The accelerometer created is on the lid.
  EXPECT_TRUE(sensor_devices_[0]->HasReceivers());
  EXPECT_FALSE(sensor_devices_[2]->HasReceivers());

  auto gyro_lid = CreateSensor(mojom::SensorType::GYROSCOPE);
  EXPECT_TRUE(gyro_lid);
  FakeClient client(gyro_lid.get());
  gyro_lid->AddClient(&client);
  PlatformSensorConfiguration config;
  config.set_frequency(100);
  gyro_lid->StartListening(&client, config);

  // Wait until all tasks are done and |gyro_lid| is reading samples.
  base::RunLoop().RunUntilIdle();

  // Simulate a disconnection of the gyro_lid in |provider_|, which triggers
  // PlatformSensorProviderChromeOS::OnSensorDeviceDisconnect(). As the mojo
  // pipe is reset with reason: DEVICE_REMOVED, |provider_| will only remove the
  // SensorDevice instead of the entire SensorService and the corresponding mojo
  // pipes.
  EXPECT_TRUE(sensor_devices_[1]->HasReceivers());
  EXPECT_FALSE(sensor_devices_[3]->HasReceivers());
  sensor_devices_[1]->ClearReceiversWithReason(
      chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED,
      "Device was removed");

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();

  // PlatformSensorProviderChromeOS::OnSensorDeviceDisconnect() doesn't reset
  // the SensorService Mojo channel with the reason: DEVICE_REMOVED.
  EXPECT_TRUE(sensor_hal_server_->GetSensorService()->HasReceivers());

  auto accel_base = CreateSensor(mojom::SensorType::ACCELEROMETER);
  base::RunLoop().RunUntilIdle();

  // The new accelerometer created is on the base, as there are more motion
  // sensors on the base now.
  EXPECT_FALSE(sensor_devices_[0]->HasReceivers());
  EXPECT_TRUE(sensor_devices_[2]->HasReceivers());

  EXPECT_FALSE(base::Contains(provider_->sensors_, 2 /* gyro_lid's id */));
  EXPECT_EQ(provider_->sensor_id_by_type_[mojom::SensorType::ACCELEROMETER],
            3 /* accel_base's id */);
  EXPECT_EQ(provider_->sensor_id_by_type_[mojom::SensorType::GYROSCOPE],
            4 /* accel_base's id */);
}

TEST_F(PlatformSensorProviderChromeOSTest, ReconnectClient) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  RegisterSensorHalServer();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));

  // Simulate a disconnection between |provider_| and SensorHalDispatcher.
  provider_->OnSensorHalClientFailure(base::TimeDelta());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Need to manually re-connect the Mojo as SensorHalDispatcher doesn't exist
  // in Lacros-Chrome.
  RegisterSensorHalServer();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest, ReconnectServer) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid);

  RegisterSensorHalServer();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));

  sensor_hal_server_->OnServerDisconnect();
  sensor_hal_server_->GetSensorService()->ClearReceivers();

  base::RunLoop().RunUntilIdle();
  // Finished simulating a disconnection with IIO Service.
  EXPECT_FALSE(provider_->GetSensor(mojom::SensorType::ACCELEROMETER));

  RegisterSensorHalServer();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::ACCELEROMETER));
}

TEST_F(PlatformSensorProviderChromeOSTest,
       CheckLinearAccelerationSensorNotCreatedIfNoAccelerometer) {
  RegisterSensorHalServer();

  EXPECT_FALSE(CreateSensor(mojom::SensorType::LINEAR_ACCELERATION));
}

TEST_F(PlatformSensorProviderChromeOSTest, CheckLinearAcceleration) {
  AddDevice(kFakeDeviceId, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase);

  RegisterSensorHalServer();

  EXPECT_TRUE(CreateSensor(mojom::SensorType::LINEAR_ACCELERATION));
}

TEST_F(
    PlatformSensorProviderChromeOSTest,
    CheckAbsoluteOrientationSensorNotCreatedIfNoAccelerometerAndNoMagnetometer) {
  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

  EXPECT_TRUE(
      CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(CreateSensor(mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
}

TEST_F(
    PlatformSensorProviderChromeOSTest,
    CheckRelativeOrientationSensorNotCreatedIfNoAccelerometerAndNoGyroscope) {
  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

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

  RegisterSensorHalServer();

  EXPECT_TRUE(
      CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(CreateSensor(mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION));
}

TEST_F(PlatformSensorProviderChromeOSTest, LatePresentMotionSensors) {
  int fake_id = 1;
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase,
            GetChannelsWithAxes(kAccelerometerChannels));
  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid,
            GetChannelsWithAxes(kAccelerometerChannels));

  RegisterSensorHalServer();

  // Wait until the disconnect of the accelerometer_base arrives at
  // FakeSensorDevice.
  base::RunLoop().RunUntilIdle();

  // Removed in |provider_| as it'll never be used.
  EXPECT_FALSE(sensor_devices_[0]->HasReceivers());
  // Remote stored in |provider_|.
  EXPECT_TRUE(sensor_devices_[1]->HasReceivers());

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase,
            GetChannelsWithAxes(kGyroscopeChannels));

  // Wait until |provider_| is notifies that the device is present.
  base::RunLoop().RunUntilIdle();

  auto accel_base = CreateSensor(mojom::SensorType::ACCELEROMETER);
  EXPECT_TRUE(accel_base);

  // Wait until all tasks are done and no failures occur in |provider_| or
  // |accel_base|.
  base::RunLoop().RunUntilIdle();

  // Motion sensors on base are used. Accelerometer on lid is reset.
  EXPECT_TRUE(sensor_devices_[0]->HasReceivers());
  EXPECT_FALSE(sensor_devices_[1]->HasReceivers());
  EXPECT_TRUE(sensor_devices_[2]->HasReceivers());

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::ANGLVEL,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid,
            GetChannelsWithAxes(kGyroscopeChannels));

  // Wait until |provider_| is notifies that the device is present.
  base::RunLoop().RunUntilIdle();

  auto accel_lid = CreateSensor(mojom::SensorType::ACCELEROMETER);
  EXPECT_TRUE(accel_lid);

  // Wait until all tasks are done and no failures occur in |provider_| or
  // |accel_lid|.
  base::RunLoop().RunUntilIdle();

  // Motion sensors on lid are used. Gyroscope on base is reset.
  EXPECT_FALSE(sensor_devices_[0]->HasReceivers());
  EXPECT_TRUE(sensor_devices_[1]->HasReceivers());
  EXPECT_FALSE(sensor_devices_[2]->HasReceivers());
  EXPECT_TRUE(sensor_devices_[3]->HasReceivers());

  accel_base.reset();

  // Wait until all tasks are done and no failures occur.
  base::RunLoop().RunUntilIdle();
}

TEST_F(PlatformSensorProviderChromeOSTest, LatePresentLightSensors) {
  int fake_id = 1;
  std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data(
      2);
  channels_data.front().id = chromeos::sensors::mojom::kLightChannel;
  channels_data.front().sample_data = kFakeSampleData;
  channels_data.back().id = chromeos::sensors::mojom::kTimestampChannel;
  channels_data.back().sample_data = kFakeTimestampData;

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::LIGHT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationBase, channels_data);

  RegisterSensorHalServer();

  auto light_base = CreateSensor(mojom::SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(light_base);
  EXPECT_TRUE(sensor_devices_[0]->HasReceivers());

  AddDevice(fake_id++, chromeos::sensors::mojom::DeviceType::LIGHT,
            base::NumberToString(kScaleValue),
            chromeos::sensors::mojom::kLocationLid, channels_data);

  // Wait until |provider_| finishes processing the new device.
  base::RunLoop().RunUntilIdle();

  // Test PlatformSensorProvider::NotifySensorCreated on different sensors
  // of the same type.
  auto light_lid = CreateSensor(mojom::SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(light_lid);

  // Wait until all tasks are done and no failures occur in |provider_| or
  // |light_lid|.
  base::RunLoop().RunUntilIdle();

  // The light sensor on lid is used in |light_lid|.
  EXPECT_TRUE(sensor_devices_[1]->HasReceivers());

  // The light sensor on base is not used after being overridden in
  // |light_base|.
  EXPECT_FALSE(sensor_devices_[0]->HasReceivers());

  // Test the usage of |light_base->reading_buffer_|.
  SensorReading result;
  EXPECT_FALSE(light_base->GetLatestReading(&result));

  // Test PlatformSensorProvider::RemoveSensor on different sensors of the
  // same type.
  light_base.reset();

  // Wait until all tasks are done and no failures occur.
  base::RunLoop().RunUntilIdle();
}

}  // namespace device
