// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/linux/sensor_device_manager.h"
#include "services/device/generic_sensor/platform_sensor_provider_linux.h"
#include "services/device/generic_sensor/platform_sensor_util.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace device {

namespace {

using mojom::SensorType;

constexpr size_t kSensorValuesSize = 3;

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

void WriteValueToFile(const base::FilePath& path, double value) {
  const std::string str = base::NumberToString(value);
  EXPECT_TRUE(base::WriteFile(path, str));
}

std::string ReadValueFromFile(const base::FilePath& path,
                              const std::string& file) {
  base::FilePath file_path = base::FilePath(path).Append(file);
  std::string new_read_value;
  if (!base::ReadFileToString(file_path, &new_read_value))
    return std::string();
  return new_read_value;
}

double RoundAccelerometerValue(double value) {
  return RoundToMultiple(value, kAccelerometerRoundingMultiple);
}

double RoundGyroscopeValue(double value) {
  return RoundToMultiple(value, kGyroscopeRoundingMultiple);
}

double RoundMagnetometerValue(double value) {
  return RoundToMultiple(value, kMagnetometerRoundingMultiple);
}

}  // namespace

// Mock for SensorDeviceService that SensorDeviceManager owns.
// This mock is used to emulate udev events and send found sensor devices
// to SensorDeviceManager.
class MockSensorDeviceManager : public SensorDeviceManager {
 public:
  MockSensorDeviceManager(const MockSensorDeviceManager&) = delete;
  MockSensorDeviceManager& operator=(const MockSensorDeviceManager&) = delete;

  ~MockSensorDeviceManager() override = default;

  // static
  static std::unique_ptr<NiceMock<MockSensorDeviceManager>> Create(
      base::WeakPtr<SensorDeviceManager::Delegate> delegate) {
    auto device_manager =
        std::make_unique<NiceMock<MockSensorDeviceManager>>(delegate);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!device_manager->sensors_dir_.CreateUniqueTempDir())
        return nullptr;
    }

    const base::FilePath& base_path = device_manager->GetSensorsBasePath();

    ON_CALL(*device_manager, GetUdevDeviceGetSubsystem(IsNull()))
        .WillByDefault(Invoke([](udev_device*) { return "iio"; }));

    ON_CALL(*device_manager, GetUdevDeviceGetSyspath(IsNull()))
        .WillByDefault(
            Invoke([base_path](udev_device*) { return base_path.value(); }));

    ON_CALL(*device_manager, GetUdevDeviceGetDevnode(IsNull()))
        .WillByDefault(Invoke([](udev_device*) { return "/dev/test"; }));

    ON_CALL(*device_manager, GetUdevDeviceGetSysattrValue(IsNull(), _))
        .WillByDefault(
            Invoke([base_path](udev_device*, const std::string& attribute) {
              base::ScopedAllowBlockingForTesting allow_blocking;
              return ReadValueFromFile(base_path, attribute);
            }));

    return device_manager;
  }

  MOCK_METHOD1(GetUdevDeviceGetSubsystem, std::string(udev_device*));
  MOCK_METHOD1(GetUdevDeviceGetSyspath, std::string(udev_device*));
  MOCK_METHOD1(GetUdevDeviceGetDevnode, std::string(udev_device* dev));
  MOCK_METHOD2(GetUdevDeviceGetSysattrValue,
               std::string(udev_device*, const std::string&));
  MOCK_METHOD0(MaybeStartEnumeration, void());

  const base::FilePath& GetSensorsBasePath() const {
    return sensors_dir_.GetPath();
  }

  void DeviceAdded() {
    SensorDeviceManager::OnDeviceAdded(nullptr /* unused */);
  }

  void DeviceRemoved() {
    SensorDeviceManager::OnDeviceRemoved(nullptr /* unused */);
  }

 protected:
  explicit MockSensorDeviceManager(
      base::WeakPtr<SensorDeviceManager::Delegate> delegate)
      : SensorDeviceManager(std::move(delegate)) {}

 private:
  base::ScopedTempDir sensors_dir_;
};

// Mock for PlatformSensor's client interface that is used to deliver
// error and data changes notifications.
class LinuxMockPlatformSensorClient : public PlatformSensor::Client {
 public:
  explicit LinuxMockPlatformSensorClient(scoped_refptr<PlatformSensor> sensor)
      : sensor_(sensor) {
    if (sensor_)
      sensor_->AddClient(this);

    ON_CALL(*this, IsSuspended()).WillByDefault(Return(false));
  }

  LinuxMockPlatformSensorClient(const LinuxMockPlatformSensorClient&) = delete;
  LinuxMockPlatformSensorClient& operator=(
      const LinuxMockPlatformSensorClient&) = delete;

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
};

class PlatformSensorAndProviderLinuxTest : public ::testing::Test {
 public:
  void SetUp() override {
    provider_ = base::WrapUnique(new PlatformSensorProviderLinux);
    provider_->SetSensorDeviceManagerForTesting(MockSensorDeviceManager::Create(
        provider_->weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  MockSensorDeviceManager* mock_sensor_device_manager() const {
    return static_cast<MockSensorDeviceManager*>(
        provider_->sensor_device_manager_.get());
  }

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
                                 double values[kSensorValuesSize]) {
    SensorPathsLinux data;
    EXPECT_TRUE(InitSensorData(type, &data));

    {
      base::ScopedAllowBlockingForTesting allow_blocking;

      const base::FilePath& sensor_dir =
          mock_sensor_device_manager()->GetSensorsBasePath();
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

      // |data.sensor_file_names| is a vector of std::vector<std::string>s. It
      // is expected to hold at most kSensorValuesSize entries, and each value
      // at position N in |values| will be written to the first entry of the
      // inner vector at position N in |sensor_file_names|.
      EXPECT_LE(data.sensor_file_names.size(), kSensorValuesSize);
      for (size_t i = 0;
           i < std::min(kSensorValuesSize, data.sensor_file_names.size());
           i++) {
        const auto& paths = data.sensor_file_names[i];
        if (paths.empty())
          continue;
        // We write to paths[0] simply because we do not need to write to the
        // other entries in any of the existing tests. This could be changed and
        // parameterized in the future if necessary.
        const auto sensor_file = base::FilePath(sensor_dir).Append(paths[0]);
        WriteValueToFile(sensor_file, values[i]);
      }
    }
  }

  // Emulates device enumerations and initial udev events. Once all
  // devices are added, tells manager its ready.
  void SetServiceStart() {
    auto* manager = mock_sensor_device_manager();
    EXPECT_CALL(*manager, MaybeStartEnumeration())
        .WillOnce(Invoke([manager]() { manager->DeviceAdded(); }))
        .WillRepeatedly(Return());
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
        FROM_HERE,
        base::BindOnce(&MockSensorDeviceManager::DeviceAdded,
                       base::Unretained(mock_sensor_device_manager())));
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
      EXPECT_TRUE(base::DeletePathRecursively(sensor_dir));
    }
    bool success = provider_->blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MockSensorDeviceManager::DeviceRemoved,
                       base::Unretained(mock_sensor_device_manager())));
    ASSERT_TRUE(success);
    // Make sure all tasks have been delivered (including SensorDeviceManager
    // notifying PlatformSensorProviderLinux of a device removal).
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<PlatformSensorProviderLinux> provider_;

  // Used to simulate the non-test scenario where we're running in an IO thread
  // that forbids blocking operations.
  base::ScopedDisallowBlocking disallow_blocking_;
};

// Tests sensor is not returned if not implemented.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorIsNotImplemented) {
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();
  EXPECT_FALSE(CreateSensor(SensorType::ACCELEROMETER));
}

// Tests sensor is not returned if not supported by hardware.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorIsNotSupported) {
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();
  EXPECT_FALSE(CreateSensor(SensorType::ACCELEROMETER));
}

// Tests sensor is returned if supported.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorIsSupported) {
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor->GetType());
}

// Tests that PlatformSensor::StartListening fails when provided reporting
// frequency is above hardware capabilities.
TEST_F(PlatformSensorAndProviderLinuxTest, StartFails) {
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
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
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
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
  double sensor_value[kSensorValuesSize] = {1};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);

  auto client =
      std::make_unique<NiceMock<LinuxMockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(5);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  GenerateDeviceRemovedEvent(
      mock_sensor_device_manager()->GetSensorsBasePath());
  WaitOnSensorErrorEvent(client.get());
}

// Tests that sensor is not returned if not connected and
// is created after it has been added.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorAddedAndRemoved) {
  double sensor_value[kSensorValuesSize] = {1, 2, 4};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
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
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
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
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
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
  double sensor_value[kSensorValuesSize] = {5};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor->GetType());
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(),
              SensorTraits<SensorType::AMBIENT_LIGHT>::kDefaultFrequency);
}

// Tests that Ambient Light sensor is correctly read.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckAmbientLightReadings) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(
      GetSensorReadingSharedBufferOffset(mojom::SensorType::AMBIENT_LIGHT),
      sizeof(SensorReadingSharedBuffer));

  double sensor_value[kSensorValuesSize] = {50};
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT, kZero, kZero, kZero,
                            sensor_value);

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

  const SensorReadingSharedBuffer* buffer =
      mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  EXPECT_THAT(buffer->reading.als.value, 50);

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Accelerometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAccelerometerReadingConversion) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(
      GetSensorReadingSharedBufferOffset(SensorType::ACCELEROMETER),
      sizeof(SensorReadingSharedBuffer));

  // As long as WaitOnSensorReadingChangedEvent() waits until client gets a
  // a notification about readings changed, the frequency file must not be
  // created to make the sensor device manager identify this sensor with
  // ON_CHANGE reporting mode. This can be done by sending |kZero| as a
  // frequency value, which means a file is not created.
  // This will allow the LinuxMockPlatformSensorClient to
  // receive a notification and test if reading values are right. Otherwise
  // the test will not know when data is ready.
  double sensor_values[kSensorValuesSize] = {4.5, -2.45, -3.29};
  InitializeSupportedSensor(SensorType::ACCELEROMETER, kZero,
                            kAccelerometerOffsetValue,
                            kAccelerometerScalingValue, sensor_values);

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

  const SensorReadingSharedBuffer* buffer =
      mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  double scaling = kAccelerometerScalingValue;
  EXPECT_THAT(buffer->reading.accel.x,
              RoundAccelerometerValue(
                  -scaling * (sensor_values[0] + kAccelerometerOffsetValue)));
  EXPECT_THAT(buffer->reading.accel.y,
              RoundAccelerometerValue(
                  -scaling * (sensor_values[1] + kAccelerometerOffsetValue)));
  EXPECT_THAT(buffer->reading.accel.z,
              RoundAccelerometerValue(
                  -scaling * (sensor_values[2] + kAccelerometerOffsetValue)));

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that LinearAcceleration sensor is not created if its source sensor is
// not available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckLinearAccelerationSensorNotCreatedIfNoAccelerometer) {
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::LINEAR_ACCELERATION);
  EXPECT_FALSE(sensor);
}

// Tests that LinearAcceleration sensor is successfully created and works.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckLinearAcceleration) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(
      GetSensorReadingSharedBufferOffset(SensorType::LINEAR_ACCELERATION),
      sizeof(SensorReadingSharedBuffer));
  double sensor_values[kSensorValuesSize] = {0, 0, -base::kMeanGravityDouble};
  InitializeSupportedSensor(SensorType::ACCELEROMETER,
                            kAccelerometerFrequencyValue, kZero, kZero,
                            sensor_values);

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

  const SensorReadingSharedBuffer* buffer =
      mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  EXPECT_THAT(buffer->reading.accel.x, 0.0);
  EXPECT_THAT(buffer->reading.accel.y, 0.0);
  EXPECT_THAT(static_cast<int>(buffer->reading.accel.z),
              kApproximateExpectedAcceleration);

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Gyroscope readings are correctly converted.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckGyroscopeReadingConversion) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping =
      region.MapAt(GetSensorReadingSharedBufferOffset(SensorType::GYROSCOPE),
                   sizeof(SensorReadingSharedBuffer));

  // As long as WaitOnSensorReadingChangedEvent() waits until client gets a
  // a notification about readings changed, the frequency file must not be
  // created to make the sensor device manager identify this sensor with
  // ON_CHANGE reporting mode. This can be done by sending |kZero| as a
  // frequency value, which means a file is not created.
  // This will allow the LinuxMockPlatformSensorClient to
  // receive a notification and test if reading values are right. Otherwise
  // the test will not know when data is ready.
  double sensor_values[kSensorValuesSize] = {2.2, -3.8, -108.7};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kZero, kGyroscopeOffsetValue,
                            kGyroscopeScalingValue, sensor_values);

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

  const SensorReadingSharedBuffer* buffer =
      mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  double scaling = kGyroscopeScalingValue;
  EXPECT_THAT(buffer->reading.gyro.x,
              RoundGyroscopeValue(scaling *
                                  (sensor_values[0] + kGyroscopeOffsetValue)));
  EXPECT_THAT(buffer->reading.gyro.y,
              RoundGyroscopeValue(scaling *
                                  (sensor_values[1] + kGyroscopeOffsetValue)));
  EXPECT_THAT(buffer->reading.gyro.z,
              RoundGyroscopeValue(scaling *
                                  (sensor_values[2] + kGyroscopeOffsetValue)));

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Magnetometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderLinuxTest, CheckMagnetometerReadingConversion) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping =
      region.MapAt(GetSensorReadingSharedBufferOffset(SensorType::MAGNETOMETER),
                   sizeof(SensorReadingSharedBuffer));

  // As long as WaitOnSensorReadingChangedEvent() waits until client gets a
  // a notification about readings changed, the frequency file must not be
  // created to make the sensor device manager identify this sensor with
  // ON_CHANGE reporting mode. This can be done by sending |kZero| as a
  // frequency value, which means a file is not created.
  // This will allow the LinuxMockPlatformSensorClient to
  // receive a notification and test if reading values are right. Otherwise
  // the test will not know when data is ready.
  double sensor_values[kSensorValuesSize] = {2.2, -3.8, -108.7};
  InitializeSupportedSensor(SensorType::MAGNETOMETER, kZero,
                            kMagnetometerOffsetValue, kMagnetometerScalingValue,
                            sensor_values);

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

  const SensorReadingSharedBuffer* buffer =
      mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  double scaling = kMagnetometerScalingValue * kMicroteslaInGauss;
  EXPECT_THAT(buffer->reading.magn.x,
              RoundMagnetometerValue(
                  scaling * (sensor_values[0] + kMagnetometerOffsetValue)));
  EXPECT_THAT(buffer->reading.magn.y,
              RoundMagnetometerValue(
                  scaling * (sensor_values[1] + kMagnetometerOffsetValue)));
  EXPECT_THAT(buffer->reading.magn.z,
              RoundMagnetometerValue(
                  scaling * (sensor_values[2] + kMagnetometerOffsetValue)));

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Ambient Light sensor client's OnSensorReadingChanged() is called
// when the Ambient Light sensor's reporting mode is
// mojom::ReportingMode::CONTINUOUS.
TEST_F(PlatformSensorAndProviderLinuxTest,
       SensorClientGetReadingChangedNotificationWhenSensorIsInContinuousMode) {
  base::ReadOnlySharedMemoryRegion region =
      provider_->CloneSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.MapAt(
      GetSensorReadingSharedBufferOffset(SensorType::AMBIENT_LIGHT),
      sizeof(SensorReadingSharedBuffer));

  double sensor_value[kSensorValuesSize] = {50};
  // Set a non-zero frequency here and sensor's reporting mode will be
  // mojom::ReportingMode::CONTINUOUS.
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT,
                            kAmbientLightFrequencyValue, kZero, kZero,
                            sensor_value);

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

  const SensorReadingSharedBuffer* buffer =
      mapping.GetMemoryAs<SensorReadingSharedBuffer>();
  EXPECT_THAT(buffer->reading.als.value, 50);

  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that ABSOLUTE_ORIENTATION_EULER_ANGLES/ABSOLUTE_ORIENTATION_QUATERNION
// sensor is not created if both of its source sensors are not available.
TEST_F(
    PlatformSensorAndProviderLinuxTest,
    CheckAbsoluteOrientationSensorNotCreatedIfNoAccelerometerAndNoMagnetometer) {
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
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
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
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
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
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that ABSOLUTE_ORIENTATION_QUATERNION sensor is successfully created.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckAbsoluteOrientationQuaternionSensor) {
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  InitializeSupportedSensor(
      SensorType::MAGNETOMETER, kMagnetometerFrequencyValue,
      kMagnetometerOffsetValue, kMagnetometerScalingValue, sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_EULER_ANGLES/RELATIVE_ORIENTATION_QUATERNION
// sensor is not created if both accelerometer and gyroscope are not available.
TEST_F(
    PlatformSensorAndProviderLinuxTest,
    CheckRelativeOrientationSensorNotCreatedIfNoAccelerometerAndNoGyroscope) {
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
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
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
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_EULER_ANGLES sensor is successfully created
// if only accelerometer is available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationEulerAnglesSensorUsingAccelerometer) {
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_QUATERNION sensor is successfully created if
// both accelerometer and gyroscope are available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationQuaternionSensorUsingAccelerometerAndGyroscope) {
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::GYROSCOPE, kGyroscopeFrequencyValue,
                            kGyroscopeOffsetValue, kGyroscopeScalingValue,
                            sensor_value);
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);
}

// Tests that RELATIVE_ORIENTATION_QUATERNION sensor is successfully created if
// only accelerometer is available.
TEST_F(PlatformSensorAndProviderLinuxTest,
       CheckRelativeOrientationQuaternionSensorUsingAccelerometer) {
  double sensor_value[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(
      SensorType::ACCELEROMETER, kAccelerometerFrequencyValue,
      kAccelerometerOffsetValue, kAccelerometerScalingValue, sensor_value);
  SetServiceStart();

  auto sensor = CreateSensor(SensorType::RELATIVE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);
}

// https://crbug.com/1254396: Make sure sensor enumeration steps happen in the
// right order. This could be converted into a web test in the future if we
// stop using mocks there (just setting window.ondevicemotion is enough to
// trigger similar behavior).
TEST_F(PlatformSensorAndProviderLinuxTest,
       AccelerometerAndLinearAccelerationEnumeration) {
  double sensor_values[kSensorValuesSize] = {0, 0, -base::kMeanGravityDouble};
  InitializeSupportedSensor(SensorType::ACCELEROMETER,
                            kAccelerometerFrequencyValue, kZero, kZero,
                            sensor_values);

  SetServiceStart();

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  // We cannot call PlatformSensorAndProviderLinuxTest::CreateSensor() like the
  // other tests because we need more control over the RunLoop; both calls to
  // PlatformSensorProvider::CreateSensor() must happen before the RunLoop
  // runs (and therefore before sensor enumeration finishes).
  scoped_refptr<PlatformSensor> accelerometer;
  provider_->CreateSensor(
      SensorType::ACCELEROMETER,
      base::BindLambdaForTesting([&](scoped_refptr<PlatformSensor> sensor) {
        accelerometer = std::move(sensor);
        barrier_closure.Run();
      }));
  scoped_refptr<PlatformSensor> linear_acceleration;
  provider_->CreateSensor(
      SensorType::LINEAR_ACCELERATION,
      base::BindLambdaForTesting([&](scoped_refptr<PlatformSensor> sensor) {
        linear_acceleration = std::move(sensor);
        barrier_closure.Run();
      }));

  run_loop.Run();

  ASSERT_TRUE(accelerometer);
  ASSERT_TRUE(linear_acceleration);
}

// Tests that queued sensor creation requests are all processed.
TEST_F(PlatformSensorAndProviderLinuxTest, SensorCreationQueueManagement) {
  double sensor_values[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::ACCELEROMETER,
                            kAccelerometerFrequencyValue, kZero, kZero,
                            sensor_values);
  InitializeSupportedSensor(SensorType::AMBIENT_LIGHT,
                            kAccelerometerFrequencyValue, kZero, kZero,
                            sensor_values);

  SetServiceStart();

  using CreateSensorFuture = TestFuture<scoped_refptr<PlatformSensor>>;

  CreateSensorFuture accelerometer1;
  CreateSensorFuture accelerometer2;
  CreateSensorFuture als;
  provider_->CreateSensor(SensorType::ACCELEROMETER,
                          accelerometer1.GetCallback());
  provider_->CreateSensor(SensorType::ACCELEROMETER,
                          accelerometer2.GetCallback());
  provider_->CreateSensor(SensorType::AMBIENT_LIGHT, als.GetCallback());

  // Sensor creation is asynchronous. Expect false until polling the futures.
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  EXPECT_FALSE(provider_->GetSensor(SensorType::AMBIENT_LIGHT));

  ASSERT_TRUE(accelerometer1.Get());
  ASSERT_TRUE(accelerometer2.Get());
  ASSERT_TRUE(als.Get());
  ASSERT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));
  ASSERT_TRUE(provider_->GetSensor(SensorType::AMBIENT_LIGHT));
}

// Tests that there are no crashes if PlatformSensorProviderLinux is destroyed
// before DidEnumerateSensors() is called.
TEST_F(PlatformSensorAndProviderLinuxTest, EarlyProviderDeletion) {
  double sensor_values[kSensorValuesSize] = {1, 2, 3};
  InitializeSupportedSensor(SensorType::ACCELEROMETER,
                            kAccelerometerFrequencyValue, kZero, kZero,
                            sensor_values);
  SetServiceStart();

  TestFuture<scoped_refptr<PlatformSensor>> sensor_future;
  provider_->CreateSensor(SensorType::ACCELEROMETER,
                          sensor_future.GetCallback());
  EXPECT_FALSE(sensor_future.IsReady());

  // Delete the provider synchronously before DidEnumerateSensors() is called
  // and run all pending asynchronous tasks.
  provider_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(sensor_future.Get());
}

}  // namespace device
