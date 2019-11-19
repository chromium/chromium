// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/linux/sensor_device_manager.h"
#include "services/device/generic_sensor/platform_sensor_provider_linux.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/angle_conversions.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace device {

namespace {

using mojom::SensorType;

// Zero value can mean whether value is not being not used or zero value.
constexpr double kZero = 0.0;

constexpr double kAmbientLightFrequencyValue = 10.0;

constexpr double kAccelerometerFrequencyValue = 10.0;
constexpr double kAccelerometerOffsetValue = 1.0;
constexpr double kAccelerometerScalingValue = 0.009806;

constexpr double kGyroscopeFrequencyValue = 6.0;
constexpr double kGyroscopeOffsetValue = 2.0;
constexpr double kGyroscopeScalingValue = 0.000017;

constexpr double kMagnetometerFrequencyValue = 7.0;
constexpr double kMagnetometerOffsetValue = 3.0;
constexpr double kMagnetometerScalingValue = 0.000001;

void DeleteFile(const base::FilePath& file) {
  EXPECT_TRUE(base::DeleteFile(file, true));
}

void WriteValueToFile(const base::FilePath& path, double value) {
  const std::string str = base::NumberToString(value);
  int bytes_written = base::WriteFile(path, str.data(), str.size());
  EXPECT_EQ(static_cast<size_t>(bytes_written), str.size());
}

std::string ReadValueFromFile(const base::FilePath& path,
                              const std::string& file) {
  base::FilePath file_path = base::FilePath(path).Append(file);
  std::string new_read_value;
  if (!base::ReadFileToString(file_path, &new_read_value))
    return std::string();
  return new_read_value;
}

}  // namespace

// Mock for SensorDeviceService that SensorDeviceManager owns.
// This mock is used to emulate udev events and send found sensor devices
// to SensorDeviceManager.
class MockSensorDeviceManager : public SensorDeviceManager {
 public:
  MockSensorDeviceManager(base::WeakPtr<SensorDeviceManager::Delegate> delegate)
      : SensorDeviceManager(std::move(delegate)) {}
  ~MockSensorDeviceManager() override {}

  MOCK_METHOD1(GetUdevDeviceGetSubsystem, std::string(udev_device*));
  MOCK_METHOD1(GetUdevDeviceGetSyspath, std::string(udev_device*));
  MOCK_METHOD1(GetUdevDeviceGetDevnode, std::string(udev_device* dev));
  MOCK_METHOD2(GetUdevDeviceGetSysattrValue,
               std::string(udev_device*, const std::string&));
  MOCK_METHOD0(Start, void());

  void EnumerationReady() {
    bool success = delegate_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SensorDeviceManager::Delegate::OnSensorNodesEnumerated,
                       delegate_));
    ASSERT_TRUE(success);
  }

  void DeviceAdded() {
    SensorDeviceManager::OnDeviceAdded(nullptr /* unused */);
  }

  void DeviceRemoved() {
    SensorDeviceManager::OnDeviceRemoved(nullptr /* unused */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSensorDeviceManager);
};

// Mock for PlatformSensor's client interface that is used to deliver
// error and data changes notifications.
class LinuxMockPlatformSensorClient : public PlatformSensor::Client {
 public:
  LinuxMockPlatformSensorClient() = default;
  explicit LinuxMockPlatformSensorClient(scoped_refptr<PlatformSensor> sensor)
      : sensor_(sensor) {
    if (sensor_)
      sensor_->AddClient(this);

    ON_CALL(*this, IsSuspended()).WillByDefault(Return(false));
  }

  ~LinuxMockPlatformSensorClient() override {
    if (sensor_)
      sensor_->RemoveClient(this);
  }

  // PlatformSensor::Client interface.
  MOCK_METHOD1(OnSensorReadingChanged, void(mojom::SensorType type));
  MOCK_METHOD0(OnSensorError, void());
  MOCK_METHOD0(IsSuspended, bool());

 private:
  scoped_refptr<PlatformSensor> sensor_;

  DISALLOW_COPY_AND_ASSIGN(LinuxMockPlatformSensorClient);
};

class PlatformSensorAndProviderLinuxTest : public ::testing::Test {
 public:
  void SetUp() override {
    provider_ = base::WrapUnique(new PlatformSensorProviderLinux);

    auto manager = std::make_unique<NiceMock<MockSensorDeviceManager>>(
        provider_->weak_ptr_factory_.GetWeakPtr());
    manager_ = manager.get();
    provider_->SetSensorDeviceManagerForTesting(std::move(manager));

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(sensors_dir_.CreateUniqueTempDir());
    }
  }

  void TearDown() override {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(sensors_dir_.Delete());
    }
    base::RunLoop().RunUntilIdle();
  }

 protected:
  // Sensor creation is asynchronous, therefore inner loop is used to wait for
  // PlatformSensorProvider::CreateSensorCallback completion.
  scoped_refptr<PlatformSensor> CreateSensor(mojom::SensorType type) {
    scoped_refptr<PlatformSensor> sensor;
    base::RunLoop run_loop;
    provider_->CreateSensor(type,
                            base::BindLambdaForTesting(
                                [&](scoped_refptr<PlatformSensor> new_sensor) {
                                  sensor = std::move(new_sensor);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
    return sensor;
  }

  // Creates sensor files according to SensorPathsLinux.
  // Existence of sensor read files mean existence of a sensor.
  // If |frequency| or |scaling| is zero, the corresponding file is not created.
  void InitializeSupportedSensor(SensorType type,
                                 double frequency,
                                 double offset,
                                 double scaling,
                                 double values[3]) {
    SensorPathsLinux data;
    EXPECT_TRUE(InitSensorData(type, &data));

    {
      base::ScopedAllowBlockingForTesting allow_blocking;

      base::FilePath sensor_dir = sensors_dir_.GetPath();
      if (!data.sensor_scale_name.empty() && scaling != 0) {
        base::FilePath sensor_scale_file =
            base::FilePath(sensor_dir).Append(data.sensor_scale_name);
        WriteValueToFile(sensor_scale_file, scaling);
      }

      if (!data.sensor_offset_file_name.empty()) {
        base::FilePath sensor_offset_file =
            base::FilePath(sensor_dir).Append(data.sensor_offset_file_name);
        WriteValueToFile(sensor_offset_file, offset);
      }

      if (!data.sensor_frequency_file_name.empty() && frequency != 0) {
        base::FilePath sensor_frequency_file =
            base::FilePath(sensor_dir).Append(data.sensor_frequency_file_name);
        WriteValueToFile(sensor_frequency_file, frequency);
      }

      uint32_t i = 0;
      for (const auto& file_names : data.sensor_file_names) {
        for (const auto& name : file_names) {
          base::FilePath sensor_file = base::FilePath(sensor_dir).Append(name);
          WriteValueToFile(sensor_file, values[i++]);
          break;
        }
      }
    }
  }

  // Initializes mock udev methods that emulate system methods by
  // just reading values from files, which SensorDeviceService has specified
  // calling udev methods.
  void InitializeMockUdevMethods(const base::FilePath& sensor_dir) {
    ON_CALL(*manager_, GetUdevDeviceGetSubsystem(IsNull()))
        .WillByDefault(Invoke([](udev_device* dev) { return "iio"; }));

    ON_CALL(*manager_, GetUdevDeviceGetSyspath(IsNull()))
        .WillByDefault(Invoke(
            [sensor_dir](udev_device* dev) { return sensor_dir.value(); }));

    ON_CALL(*manager_, GetUdevDeviceGetDevnode(IsNull()))
        .WillByDefault(Invoke([](udev_device* dev) { return "/dev/test"; }));

    ON_CALL(*manager_, GetUdevDeviceGetSysattrValue(IsNull(), _))
        .WillByDefault(Invoke(
            [sensor_dir](udev_device* dev, const std::string& attribute) {
              base::ScopedAllowBlockingForTesting allow_blocking;
              return ReadValueFromFile(sensor_dir, attribute);
            }));
  }

  // Emulates device enumerations and initial udev events. Once all
  // devices are added, tells manager its ready.
  void SetServiceStart() {
    EXPECT_CALL(*manager_, Start()).WillOnce(Invoke([this]() {
      manager_->DeviceAdded();
      manager_->EnumerationReady();
    }));
  }

  // Waits before OnSensorReadingChanged is called.
  void WaitOnSensorReadingChangedEvent(LinuxMockPlatformSensorClient* client,
                                       mojom::SensorType type) {
    base::RunLoop run_loop;
    EXPECT_CALL(*client, OnSensorReadingChanged(type))
        .WillOnce(Invoke([&](mojom::SensorType type) { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Waits before OnSensorError is called.
  void WaitOnSensorErrorEvent(LinuxMockPlatformSensorClient* client) {
    base::RunLoop run_loop;
    EXPECT_CALL(*client, OnSensorError()).WillOnce(Invoke([&]() {
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Uses the right task runner to notify SensorDeviceManager that a device has
  // been added.
  void GenerateDeviceAddedEvent() {
    bool success = provider_->blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MockSensorDeviceManager::DeviceAdded,
                                  base::Unretained(manager_)));
    ASSERT_TRUE(success);
    // Make sure all tasks have been delivered (including SensorDeviceManager
    // notifying PlatformSensorProviderLinux of a device addition).
    task_environment_.RunUntilIdle();
  }

  // Generates a "remove device" event by removed sensors' directory and
  // notifies the mock service about "removed" event.
  void GenerateDeviceRemovedEvent(const base::FilePath& sensor_dir) {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      DeleteFile(sensor_dir);
    }
    bool success = provider_->blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MockSensorDeviceManager::DeviceRemoved,
                                  base::Unretained(manager_)));
    ASSERT_TRUE(success);
    // Make sure all tasks have been delivered (including SensorDeviceManager
    // notifying PlatformSensorProviderLinux of a device removal).
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  MockSensorDeviceManager* manager_;
  std::unique_ptr<PlatformSensorProviderLinux> provider_;
  // Holds base dir where a sensor dir is located.
  base::ScopedTempDir sensors_dir_;

  // Used to simulate the non-test scenario where we're running in an IO thread
  // that forbids blocking operations.
  base::ScopedDisallowBlocking disallow_blocking_;
};

// Tests sensor is not returned if not implemented.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorIsNotImplemented) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();
  EXPECT_FALSE(CreateSensor(SensorType::PROXIMITY));
}

// Tests sensor is not returned if not supported by hardware.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorIsNotSupported) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();
  EXPECT_FALSE(CreateSensor(SensorType::ACCELEROMETER));
}

// Tests sensor is returned if supported.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorIsSupported) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor->GetType());
}

// Tests that PlatformSensor::StartListening fails when provided reporting
// frequency is above hardware capabilities.
TEST_F(PlatformSensorAndProviderLinuxTest, StartFails) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_FALSE(sensor->StartListening(client.get(), configuration));
}

// Tests that PlatformSensor::StartListening succeeds and notification about
// modified sensor reading is sent to the PlatformSensor::Client interface.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorStarted) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(5);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that OnSensorError is called when sensor is disconnected.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorRemoved) {
  double sensor_value[3] = {1};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(5);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  GenerateDeviceRemovedEvent(sensors_dir_.GetPath());
  WaitOnSensorErrorEvent(client.get());
}

// Tests that sensor is not returned if not connected and
// is created after it has been added.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorAddedAndRemoved) {
  double sensor_value[3] = {1, 2, 4};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto als_sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(als_sensor);
  auto gyro_sensor = CreateSensor(SensorType::GYROSCOPE);
  EXPECT_FALSE(gyro_sensor);

  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  GenerateDeviceAddedEvent();
  gyro_sensor = CreateSensor(SensorType::GYROSCOPE);
  EXPECT_TRUE(gyro_sensor);
  EXPECT_EQ(gyro_sensor->GetType(), SensorType::GYROSCOPE);
}

// Checks the main fields of all sensors and initialized right.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckAllSupportedSensors) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto als_sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(als_sensor);
  EXPECT_EQ(als_sensor->GetType(), SensorType::AMBIENT_LIGHT);
  EXPECT_THAT(als_sensor->GetDefaultConfiguration().frequency(),
              SensorTraits<SensorType::AMBIENT_LIGHT>::kDefaultFrequency);

  auto accel_sensor = CreateSensor(SensorType::ACCELEROMETER);
  EXPECT_TRUE(accel_sensor);
  EXPECT_EQ(accel_sensor->GetType(), SensorType::ACCELEROMETER);
  EXPECT_THAT(accel_sensor->GetDefaultConfiguration().frequency(),
              kAccelerometerFrequencyValue);

  auto gyro_sensor = CreateSensor(SensorType::GYROSCOPE);
  EXPECT_TRUE(gyro_sensor);
  EXPECT_EQ(gyro_sensor->GetType(), SensorType::GYROSCOPE);
  EXPECT_THAT(gyro_sensor->GetDefaultConfiguration().frequency(),
              kGyroscopeFrequencyValue);

  auto magn_sensor = CreateSensor(SensorType::MAGNETOMETER);
  EXPECT_TRUE(magn_sensor);
  EXPECT_EQ(magn_sensor->GetType(), SensorType::MAGNETOMETER);
  EXPECT_THAT(magn_sensor->GetDefaultConfiguration().frequency(),
              kMagnetometerFrequencyValue);
}

// Tests that GetMaximumSupportedFrequency provides correct value.
TEST_F(PlatformSensorAndProviderLinuxTest, GetMaximumSupportedFrequency) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::ACCELEROMETER);
  ASSERT_TRUE(sensor);
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(),
              kAccelerometerFrequencyValue);
}

// Tests that GetMaximumSupportedFrequency provides correct value when
// OS does not provide any information about frequency.
TEST_F(PlatformSensorAndProviderLinuxTest,
       GetMaximumSupportedFrequencyDefault) {
  double sensor_value[3] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor->GetType());
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(),
              SensorTraits<SensorType::AMBIENT_LIGHT>::kDefaultFrequency);
}

// Tests that Ambient Light sensor is correctly read.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckAmbientLightReadings) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::AMBIENT_LIGHT));

  double sensor_value[3] = {22};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);

  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(sensor->GetReportingMode(), mojom::ReportingMode::ON_CHANGE);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(
      sensor->GetMaximumSupportedFrequency());
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.als.value, sensor_value[0]);

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Accelerometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAccelerometerReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::ACCELEROMETER));

  // As long as WaitOnSensorReadingChangedEvent() waits until client gets a
  // a notification about readings changed, the frequency file must not be
  // created to make the sensor device manager identify this sensor with
  // ON_CHANGE reporting mode. This can be done by sending |kZero| as a
  // frequency value, which means a file is not created.
  // This will allow the LinuxMockPlatformSensorClient to
  // receive a notification and test if reading values are right. Otherwise
  // the test will not know when data is ready.
  double sensor_values[3] = {4.5, -2.45, -3.29};
  InitializeSupportedSensor(SensorType::ACCELEROMETER, kZero,
                            kAccelerometerOffsetValue,
                            kAccelerometerScalingValue, sensor_values);

  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::ACCELEROMETER);
  ASSERT_TRUE(sensor);
  // The reporting mode is ON_CHANGE only for this test.
  EXPECT_EQ(sensor->GetReportingMode(), mojom::ReportingMode::ON_CHANGE);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
#if defined(OS_CHROMEOS)
  double scaling = base::kMeanGravityDouble / kAccelerometerScalingValue;
  EXPECT_THAT(buffer->reading.accel.x, scaling * sensor_values[0]);
  EXPECT_THAT(buffer->reading.accel.y, scaling * sensor_values[1]);
  EXPECT_THAT(buffer->reading.accel.z, scaling * sensor_values[2]);
#else
  double scaling = kAccelerometerScalingValue;
  EXPECT_THAT(buffer->reading.accel.x,
              -scaling * (sensor_values[0] + kAccelerometerOffsetValue));
  EXPECT_THAT(buffer->reading.accel.y,
              -scaling * (sensor_values[1] + kAccelerometerOffsetValue));
  EXPECT_THAT(buffer->reading.accel.z,
              -scaling * (sensor_values[2] + kAccelerometerOffsetValue));
#endif

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that LinearAcceleration sensor is not created if its source sensor is
// not available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckLinearAccelerationSensorNotCreatedIfNoAccelerometer) {
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::LINEAR_ACCELERATION);
  EXPECT_FALSE(sensor);
}

// Tests that LinearAcceleration sensor is successfully created and works.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckLinearAcceleration) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::LINEAR_ACCELERATION));
#if defined(OS_CHROMEOS)
  // CrOS has a different axes plane and scale, see crbug.com/501184.
  double sensor_values[3] = {0, 0, 1};
#else
  double sensor_values[3] = {0, 0, -base::kMeanGravityDouble};
#endif
  InitializeSupportedSensor(SensorType::ACCELEROMETER,
                            kAccelerometerFrequencyValue, kZero, kZero,
                            sensor_values);

  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::LINEAR_ACCELERATION);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(sensor->GetReportingMode(), mojom::ReportingMode::CONTINUOUS);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));

  // The actual accceration is around 0 but the algorithm needs several
  // iterations to isolate gravity properly.
  int kApproximateExpectedAcceleration = 6;
  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.accel.x, 0.0);
  EXPECT_THAT(buffer->reading.accel.y, 0.0);
  EXPECT_THAT(static_cast<int>(buffer->reading.accel.z),
              kApproximateExpectedAcceleration);

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Gyroscope readings are correctly converted.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckGyroscopeReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::GYROSCOPE));

  // As long as WaitOnSensorReadingChangedEvent() waits until client gets a
  // a notification about readings changed, the frequency file must not be
  // created to make the sensor device manager identify this sensor with
  // ON_CHANGE reporting mode. This can be done by sending |kZero| as a
  // frequency value, which means a file is not created.
  // This will allow the LinuxMockPlatformSensorClient to
  // receive a notification and test if reading values are right. Otherwise
  // the test will not know when data is ready.
  double sensor_values[3] = {2.2, -3.8, -108.7};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kZero, kGyroscopeOffsetValue,
                            kGyroscopeScalingValue, sensor_values);

  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::GYROSCOPE);
  ASSERT_TRUE(sensor);
  // The reporting mode is ON_CHANGE only for this test.
  EXPECT_EQ(sensor->GetReportingMode(), mojom::ReportingMode::ON_CHANGE);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
#if defined(OS_CHROMEOS)
  double scaling =
      gfx::DegToRad(base::kMeanGravityDouble) / kGyroscopeScalingValue;
  EXPECT_THAT(buffer->reading.gyro.x, -scaling * sensor_values[0]);
  EXPECT_THAT(buffer->reading.gyro.y, -scaling * sensor_values[1]);
  EXPECT_THAT(buffer->reading.gyro.z, -scaling * sensor_values[2]);
#else
  double scaling = kGyroscopeScalingValue;
  EXPECT_THAT(buffer->reading.gyro.x,
              scaling * (sensor_values[0] + kGyroscopeOffsetValue));
  EXPECT_THAT(buffer->reading.gyro.y,
              scaling * (sensor_values[1] + kGyroscopeOffsetValue));
  EXPECT_THAT(buffer->reading.gyro.z,
              scaling * (sensor_values[2] + kGyroscopeOffsetValue));
#endif

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Magnetometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckMagnetometerReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::MAGNETOMETER));

  // As long as WaitOnSensorReadingChangedEvent() waits until client gets a
  // a notification about readings changed, the frequency file must not be
  // created to make the sensor device manager identify this sensor with
  // ON_CHANGE reporting mode. This can be done by sending |kZero| as a
  // frequency value, which means a file is not created.
  // This will allow the LinuxMockPlatformSensorClient to
  // receive a notification and test if reading values are right. Otherwise
  // the test will not know when data is ready.
  double sensor_values[3] = {2.2, -3.8, -108.7};
  InitializeSupportedSensor(SensorType::MAGNETOMETER, kZero,
                            kMagnetometerOffsetValue, kMagnetometerScalingValue,
                            sensor_values);

  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::MAGNETOMETER);
  ASSERT_TRUE(sensor);
  // The reporting mode is ON_CHANGE only for this test.
  EXPECT_EQ(sensor->GetReportingMode(), mojom::ReportingMode::ON_CHANGE);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  double scaling = kMagnetometerScalingValue * kMicroteslaInGauss;
  EXPECT_THAT(buffer->reading.magn.x,
              scaling * (sensor_values[0] + kMagnetometerOffsetValue));
  EXPECT_THAT(buffer->reading.magn.y,
              scaling * (sensor_values[1] + kMagnetometerOffsetValue));
  EXPECT_THAT(buffer->reading.magn.z,
              scaling * (sensor_values[2] + kMagnetometerOffsetValue));

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Ambient Light sensor client's OnSensorReadingChanged() is called
// when the Ambient Light sensor's reporting mode is
// mojom::ReportingMode::CONTINUOUS.
TEST_F(PlatformSensorAndProviderLinuxTest,
       SensorClientGetReadingChangedNotificationWhenSensorIsInContinuousMode) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::AMBIENT_LIGHT));

  double sensor_value[3] = {22};
  // Set a non-zero frequency here and sensor's reporting mode will be
  // mojom::ReportingMode::CONTINUOUS.
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT,
                            kAmbientLightFrequencyValue, kZero, kZero,
                            sensor_value);

  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(mojom::ReportingMode::CONTINUOUS, sensor->GetReportingMode());

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);

  PlatformSensorConfiguration configuration(
      sensor->GetMaximumSupportedFrequency());
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));

  WaitOnSensorReadingChangedEvent(client.get(), sensor->GetType());

  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.als.value, sensor_value[0]);

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that ABSOLUTE_ORIENTATION_EULER_ANGLES/ABSOLUTE_ORIENTATION_QUATERNION
// sensor is not created if both of its source sensors are not available.
TEST_F(
    PlatformSensorAndProviderLinuxTest,
    CheckAbsoluteOrientationSensorNotCreatedIfNoAccelerometerAndNoMagnetometer) {
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  {
    auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
    EXPECT_FALSE(sensor);
  }

  {
    auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
    EXPECT_FALSE(sensor);
  }
}

// Tests that ABSOLUTE_ORIENTATION_EULER_ANGLES/ABSOLUTE_ORIENTATION_QUATERNION
// sensor is not created if accelerometer is not available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAbsoluteOrientationSensorNotCreatedIfNoAccelerometer) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  {
    auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
    EXPECT_FALSE(sensor);
  }

  {
    auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
    EXPECT_FALSE(sensor);
  }
}

// Tests that ABSOLUTE_ORIENTATION_EULER_ANGLES/ABSOLUTE_ORIENTATION_QUATERNION
// sensor is not created if magnetometer is not available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAbsoluteOrientationSensorNotCreatedIfNoMagnetometer) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  {
    auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
    EXPECT_FALSE(sensor);
  }

  {
    auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
    EXPECT_FALSE(sensor);
  }
}

// Tests that ABSOLUTE_ORIENTATION_EULER_ANGLES sensor is successfully created.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAbsoluteOrientationEulerAnglesSensor) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that ABSOLUTE_ORIENTATION_QUATERNION sensor is successfully created.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAbsoluteOrientationQuaternionSensor) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_EULER_ANGLES/RELATIVE_ORIENTATION_QUATERNION
// sensor is not created if both accelerometer and gyroscope are not available.
TEST_F(
    PlatformSensorAndProviderLinuxTest,
    CheckRelativeOrientationSensorNotCreatedIfNoAccelerometerAndNoGyroscope) {
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  {
    auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
    EXPECT_FALSE(sensor);
  }

  {
    auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_QUATERNION);
    EXPECT_FALSE(sensor);
  }
}

// Tests that RELATIVE_ORIENTATION_EULER_ANGLES/RELATIVE_ORIENTATION_QUATERNION
// sensor is not created if accelerometer is not available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationSensorNotCreatedIfNoAccelerometer) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  {
    auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
    EXPECT_FALSE(sensor);
  }

  {
    auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_QUATERNION);
    EXPECT_FALSE(sensor);
  }
}

// Tests that RELATIVE_ORIENTATION_EULER_ANGLES sensor is successfully created
// if both accelerometer and gyroscope are available.
TEST_F(
    PlatformSensorAndProviderLinuxTest,
    CheckRelativeOrientationEulerAnglesSensorUsingAccelerometerAndGyroscope) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_EULER_ANGLES sensor is successfully created
// if only accelerometer is available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationEulerAnglesSensorUsingAccelerometer) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_QUATERNION sensor is successfully created if
// both accelerometer and gyroscope are available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationQuaternionSensorUsingAccelerometerAndGyroscope) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_QUATERNION sensor is successfully created if
// only accelerometer is available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationQuaternionSensorUsingAccelerometer) {
  double sensor_value[3] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeMockUdevMethods(sensors_dir_.GetPath());
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);
}

}  // namespace device
