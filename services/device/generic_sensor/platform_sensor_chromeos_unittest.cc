// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/generic_sensor/platform_sensor_chromeos.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/platform_sensor_util.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr int kFakeDeviceId = 1;

constexpr char kAccelerometerChannels[][10] = {"accel_x", "accel_y", "accel_z"};
constexpr char kGyroscopeChannels[][10] = {"anglvel_x", "anglvel_y",
                                           "anglvel_z"};
constexpr char kMagnetometerChannels[][10] = {"magn_x", "magn_y", "magn_z"};
constexpr char kGravityChannels[][10] = {"gravity_x", "gravity_y", "gravity_z"};

constexpr double kScaleValueLightSensor = device::kAlsRoundingMultiple;

constexpr double kScaleValue = 10.0;

// The number of axes for which there are accelerometer readings.
constexpr uint32_t kNumberOfAxes = 3u;

constexpr int64_t kFakeSampleData = 1;
constexpr int64_t kFakeAxesSampleData[] = {1, 2, 3};
constexpr int64_t kFakeTimestampData = 163176689212344ll;

}  // namespace

class PlatformSensorChromeOSTestBase {
 protected:
  void SetUpBase() {
    provider_ = std::make_unique<FakePlatformSensorProvider>();

    pending_receiver_ = sensor_device_remote_.BindNewPipeAndPassReceiver();
  }

  void InitSensorDevice(
      std::vector<chromeos::sensors::FakeSensorDevice::ChannelData>
          channels_data) {
    sensor_device_ = std::make_unique<chromeos::sensors::FakeSensorDevice>(
        std::move(channels_data));
    receiver_id_ = sensor_device_->AddReceiver(std::move(pending_receiver_));
  }

  void DisableFirstChannel() {
    DCHECK(sensor_device_.get());

    sensor_device_->SetChannelsEnabledWithId(receiver_id_, {0}, false);
  }

  void OnSensorDeviceDisconnect(uint32_t custom_reason_code,
                                const std::string& description) {
    custom_reason_code_ = custom_reason_code;
  }

  std::unique_ptr<chromeos::sensors::FakeSensorDevice> sensor_device_;
  std::unique_ptr<FakePlatformSensorProvider> provider_;
  scoped_refptr<PlatformSensorChromeOS> sensor_;

  mojo::ReceiverId receiver_id_;
  mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote_;
  mojo::PendingReceiver<chromeos::sensors::mojom::SensorDevice>
      pending_receiver_;

  std::optional<uint32_t> custom_reason_code_;

  base::test::SingleThreadTaskEnvironment task_environment;
};

class PlatformSensorChromeOSOneChannelTest
    : public PlatformSensorChromeOSTestBase,
      public ::testing::TestWithParam<
          std::pair<mojom::SensorType, const char*>> {
 protected:
  void SetUp() override {
    SetUpBase();

    auto type = GetParam().first;

    sensor_ = base::MakeRefCounted<PlatformSensorChromeOS>(
        kFakeDeviceId, type, provider_->GetSensorReadingBuffer(type),
        provider_->AsWeakPtr(),
        base::BindOnce(
            &PlatformSensorChromeOSOneChannelTest::OnSensorDeviceDisconnect,
            base::Unretained(this)),
        kScaleValueLightSensor, std::move(sensor_device_remote_));

    EXPECT_EQ(sensor_->GetReportingMode(),
              type == mojom::SensorType::AMBIENT_LIGHT
                  ? mojom::ReportingMode::ON_CHANGE
                  : mojom::ReportingMode::CONTINUOUS);
    EXPECT_EQ(sensor_->GetDefaultConfiguration().frequency(),
              GetSensorMaxAllowedFrequency(type));
  }

  void SetChannels(const char channel[], bool set_first_channel) {
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data(
        set_first_channel ? 2 : 1);
    if (set_first_channel) {
      channels_data.front().id = channel;
      channels_data.front().sample_data = kFakeSampleData;
    }

    channels_data.back().id = chromeos::sensors::mojom::kTimestampChannel;
    channels_data.back().sample_data = kFakeTimestampData;

    InitSensorDevice(std::move(channels_data));
  }

  SensorReadingSingle& GetSensorReadingSingle(SensorReading& reading) {
    switch (GetParam().first) {
      case mojom::SensorType::AMBIENT_LIGHT:
        return reading.als;
      default:
        LOG(FATAL) << "Invalid type: " << GetParam().first;
    }
  }

  void GetRoundedSensorReadingSingle(SensorReadingSingle* reading_single) {
    reading_single->value = kFakeSampleData * kScaleValueLightSensor;
    reading_single->timestamp =
        base::Nanoseconds(kFakeTimestampData).InSecondsF();

    RoundIlluminanceReading(reading_single);
  }

  void WaitForAndCheckReading(
      testing::NiceMock<MockPlatformSensorClient>* client) {
    base::RunLoop loop;
    // Wait until a sample is received.
    EXPECT_CALL(*client, OnSensorReadingChanged(GetParam().first))
        .WillOnce(base::test::RunOnceClosure(loop.QuitClosure()));
    loop.Run();

    SensorReading reading;
    EXPECT_TRUE(sensor_->GetLatestReading(&reading));
    const auto& reading_single = GetSensorReadingSingle(reading);

    SensorReadingSingle rounded_reading_single;
    GetRoundedSensorReadingSingle(&rounded_reading_single);

    EXPECT_EQ(reading_single.value, rounded_reading_single.value);
    EXPECT_EQ(reading_single.timestamp, rounded_reading_single.timestamp);
  }
};

TEST_P(PlatformSensorChromeOSOneChannelTest, MissingChannels) {
  SetChannels(GetParam().second, /*set_first_channel=*/false);

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>();
  sensor_->AddClient(client.get());
  sensor_->StartListening(client.get(),
                          PlatformSensorConfiguration(
                              GetSensorMaxAllowedFrequency(GetParam().first)));
  EXPECT_TRUE(sensor_->IsActiveForTesting());

  EXPECT_CALL(*client.get(), OnSensorReadingChanged(GetParam().first)).Times(0);
  // Wait until all tasks done and no samples updated.
  base::RunLoop().RunUntilIdle();

  sensor_->RemoveClient(client.get());
}

TEST_P(PlatformSensorChromeOSOneChannelTest, GetSamples) {
  SetChannels(GetParam().second, /*set_first_channel=*/true);

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>();
  sensor_->AddClient(client.get());
  double frequency = GetSensorMaxAllowedFrequency(GetParam().first);
  sensor_->StartListening(client.get(), PlatformSensorConfiguration(frequency));
  EXPECT_TRUE(sensor_->IsActiveForTesting());

  WaitForAndCheckReading(client.get());

  sensor_->StopListening(client.get(), PlatformSensorConfiguration(frequency));
  sensor_->StartListening(client.get(), PlatformSensorConfiguration(frequency));
  WaitForAndCheckReading(client.get());

  DisableFirstChannel();

  EXPECT_CALL(*client.get(), OnSensorReadingChanged(GetParam().first)).Times(0);
  // Wait until a sample without the first channel is received.
  base::RunLoop().RunUntilIdle();
  // No reading updated.

  sensor_device_->ResetObserverRemoteWithReason(
      receiver_id_,
      chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED,
      "Device was removed");

  base::RunLoop loop;
  // Wait until the disconnect arrives at |sensor_|.
  EXPECT_CALL(*client.get(), OnSensorError())
      .WillOnce(base::test::RunOnceClosure(loop.QuitClosure()));
  loop.Run();

  EXPECT_EQ(
      custom_reason_code_,
      static_cast<uint32_t>(chromeos::sensors::mojom::
                                SensorDeviceDisconnectReason::DEVICE_REMOVED));
  sensor_->RemoveClient(client.get());
}

TEST_P(PlatformSensorChromeOSOneChannelTest, ResetOnTooManyFailures) {
  SetChannels(GetParam().second, /*set_first_channel=*/true);

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>();
  sensor_->AddClient(client.get());
  sensor_->StartListening(client.get(),
                          PlatformSensorConfiguration(
                              GetSensorMaxAllowedFrequency(GetParam().first)));
  EXPECT_TRUE(sensor_->IsActiveForTesting());

  WaitForAndCheckReading(client.get());

  EXPECT_CALL(*client.get(), OnSensorError()).Times(0);
  for (size_t i = 0;
       i < PlatformSensorChromeOS::kNumFailedReadsBeforeGivingUp - 1; ++i) {
    sensor_->OnErrorOccurred(
        chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);
  }

  base::flat_map<int32_t, int64_t> sample;
  sample[0] = kFakeSampleData;
  sample[1] = kFakeTimestampData;

  for (size_t i = 0; i < PlatformSensorChromeOS::kNumRecoveryReads; ++i)
    sensor_->OnSampleUpdated(sample);

  // |num_failed_reads_| is recovered by 1.
  sensor_->OnErrorOccurred(
      chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);

  EXPECT_CALL(*client.get(), OnSensorError()).Times(1);

  sensor_->OnErrorOccurred(
      chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);

  sensor_->RemoveClient(client.get());
}

INSTANTIATE_TEST_SUITE_P(
    PlatformSensorChromeOSOneChannelTestRun,
    PlatformSensorChromeOSOneChannelTest,
    ::testing::Values(std::make_pair(mojom::SensorType::AMBIENT_LIGHT,
                                     chromeos::sensors::mojom::kLightChannel)));

class PlatformSensorChromeOSAxesTest
    : public PlatformSensorChromeOSTestBase,
      public ::testing::TestWithParam<
          std::pair<mojom::SensorType, const char (*)[10]>> {
 protected:
  void SetUp() override {
    SetUpBase();

    auto type = GetParam().first;

    sensor_ = base::MakeRefCounted<PlatformSensorChromeOS>(
        kFakeDeviceId, type, provider_->GetSensorReadingBuffer(type), nullptr,
        base::BindOnce(
            &PlatformSensorChromeOSAxesTest::OnSensorDeviceDisconnect,
            base::Unretained(this)),
        kScaleValue, std::move(sensor_device_remote_));

    EXPECT_EQ(sensor_->GetReportingMode(), mojom::ReportingMode::CONTINUOUS);
    EXPECT_EQ(sensor_->GetDefaultConfiguration().frequency(),
              GetSensorMaxAllowedFrequency(type));
  }

  void SetChannelsWithAxes(const char channels[][10], uint32_t num_of_axes) {
    CHECK_LE(num_of_axes, kNumberOfAxes);
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data(
        num_of_axes + 1);
    for (uint32_t i = 0; i < num_of_axes; ++i) {
      channels_data[i].id = channels[i];
      channels_data[i].sample_data = kFakeAxesSampleData[i];
    }
    channels_data.back().id = chromeos::sensors::mojom::kTimestampChannel;
    channels_data.back().sample_data = kFakeTimestampData;

    InitSensorDevice(std::move(channels_data));
  }

  SensorReadingXYZ& GetSensorReadingXYZ(SensorReading& reading) {
    switch (GetParam().first) {
      case mojom::SensorType::ACCELEROMETER:
      case mojom::SensorType::GRAVITY:
        return reading.accel;
      case mojom::SensorType::GYROSCOPE:
        return reading.gyro;
      case mojom::SensorType::MAGNETOMETER:
        return reading.magn;
      default:
        LOG(FATAL) << "Invalid type: " << GetParam().first;
    }
  }

  void GetRoundedSensorReadingXYZ(SensorReadingXYZ* reading_xyz) {
    reading_xyz->x = kFakeAxesSampleData[0] * kScaleValue;
    reading_xyz->y = kFakeAxesSampleData[1] * kScaleValue;
    reading_xyz->z = kFakeAxesSampleData[2] * kScaleValue;
    reading_xyz->timestamp = base::Nanoseconds(kFakeTimestampData).InSecondsF();

    switch (GetParam().first) {
      case mojom::SensorType::ACCELEROMETER:
      case mojom::SensorType::GRAVITY:
        RoundAccelerometerReading(reading_xyz);
        break;
      case mojom::SensorType::GYROSCOPE:
        RoundGyroscopeReading(reading_xyz);
        break;
      case mojom::SensorType::MAGNETOMETER:
        RoundMagnetometerReading(reading_xyz);
        break;
      default:
        LOG(FATAL) << "Invalid type: " << GetParam().first;
    }
  }

  void WaitForAndCheckReading(
      testing::NiceMock<MockPlatformSensorClient>* client) {
    base::RunLoop loop;
    // Wait until a sample is received.
    EXPECT_CALL(*client, OnSensorReadingChanged(GetParam().first))
        .WillOnce(base::test::RunOnceClosure(loop.QuitClosure()));
    loop.Run();

    SensorReading reading;
    EXPECT_TRUE(sensor_->GetLatestReading(&reading));
    const auto& reading_xyz = GetSensorReadingXYZ(reading);

    SensorReadingXYZ rounded_reading_xyz;
    GetRoundedSensorReadingXYZ(&rounded_reading_xyz);

    EXPECT_EQ(reading_xyz.x, rounded_reading_xyz.x);
    EXPECT_EQ(reading_xyz.y, rounded_reading_xyz.y);
    EXPECT_EQ(reading_xyz.z, rounded_reading_xyz.z);
    EXPECT_EQ(reading_xyz.timestamp, rounded_reading_xyz.timestamp);
  }
};

TEST_P(PlatformSensorChromeOSAxesTest, MissingChannels) {
  SetChannelsWithAxes(GetParam().second, kNumberOfAxes - 1);

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>();
  sensor_->AddClient(client.get());
  sensor_->StartListening(client.get(),
                          PlatformSensorConfiguration(
                              GetSensorMaxAllowedFrequency(GetParam().first)));
  EXPECT_TRUE(sensor_->IsActiveForTesting());

  EXPECT_CALL(*client.get(), OnSensorReadingChanged(GetParam().first)).Times(0);
  // Wait until all tasks done and no samples updated.
  base::RunLoop().RunUntilIdle();

  sensor_->RemoveClient(client.get());
}

TEST_P(PlatformSensorChromeOSAxesTest, GetSamples) {
  SetChannelsWithAxes(GetParam().second, kNumberOfAxes);

  auto client = std::make_unique<testing::NiceMock<MockPlatformSensorClient>>();
  sensor_->AddClient(client.get());
  double frequency = GetSensorMaxAllowedFrequency(GetParam().first);
  sensor_->StartListening(client.get(), PlatformSensorConfiguration(frequency));
  EXPECT_TRUE(sensor_->IsActiveForTesting());

  WaitForAndCheckReading(client.get());

  sensor_->StopListening(client.get(), PlatformSensorConfiguration(frequency));
  sensor_->StartListening(client.get(), PlatformSensorConfiguration(frequency));
  WaitForAndCheckReading(client.get());

  DisableFirstChannel();

  EXPECT_CALL(*client.get(), OnSensorReadingChanged(GetParam().first)).Times(0);
  // Wait until a sample without the first channel is received.
  base::RunLoop().RunUntilIdle();
  // No reading updated.

  sensor_device_->RemoveReceiver(receiver_id_);

  base::RunLoop loop;
  // Wait until the disconnect arrives at |sensor_|.
  EXPECT_CALL(*client.get(), OnSensorError())
      .WillOnce(base::test::RunOnceClosure(loop.QuitClosure()));
  loop.Run();

  sensor_->RemoveClient(client.get());
}

INSTANTIATE_TEST_SUITE_P(
    PlatformSensorChromeOSAxesTestRun,
    PlatformSensorChromeOSAxesTest,
    ::testing::Values(
        std::make_pair(mojom::SensorType::ACCELEROMETER,
                       kAccelerometerChannels),
        std::make_pair(mojom::SensorType::GYROSCOPE, kGyroscopeChannels),
        std::make_pair(mojom::SensorType::MAGNETOMETER, kMagnetometerChannels),
        std::make_pair(mojom::SensorType::GRAVITY, kGravityChannels)));

}  // namespace device
