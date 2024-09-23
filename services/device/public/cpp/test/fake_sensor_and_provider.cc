// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/public/cpp/test/fake_sensor_and_provider.h"

#include <memory>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint64_t kReadingBufferSize = sizeof(device::SensorReadingSharedBuffer);
const uint64_t kSharedBufferSizeInBytes =
    kReadingBufferSize *
    (static_cast<uint64_t>(device::mojom::SensorType::kMaxValue) + 1);

}  // namespace

namespace device {

bool WaiterHelper::Wait() {
  bool result = WaitInternal();
  event_received_ = false;
  return result;
}

void WaiterHelper::OnEvent() {
  event_received_ = true;
  run_loop_.Quit();
}

bool WaiterHelper::WaitInternal() {
  if (event_received_) {
    return true;
  }
  run_loop_.Run();
  return event_received_;
}

FakeSensor::FakeSensor(mojom::SensorType sensor_type,
                       SensorReadingSharedBuffer* buffer)
    : sensor_type_(sensor_type), buffer_(buffer) {}

FakeSensor::~FakeSensor() = default;

void FakeSensor::AddConfiguration(
    const PlatformSensorConfiguration& configuration,
    AddConfigurationCallback callback) {
  std::move(callback).Run(true);
  SensorReadingChanged();
}

void FakeSensor::GetDefaultConfiguration(
    GetDefaultConfigurationCallback callback) {
  std::move(callback).Run(GetDefaultConfiguration());
}

void FakeSensor::RemoveConfiguration(
    const PlatformSensorConfiguration& configuration) {}

void FakeSensor::Suspend() {
  suspend_waiter_.OnEvent();
}

void FakeSensor::Resume() {
  resume_waiter_.OnEvent();
}

bool FakeSensor::WaitForSuspend(bool suspend) {
  if (suspend) {
    return suspend_waiter_.Wait();
  }
  return resume_waiter_.Wait();
}

void FakeSensor::ConfigureReadingChangeNotifications(bool enabled) {
  reading_notification_enabled_ = enabled;
}

PlatformSensorConfiguration FakeSensor::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(GetSensorDefaultFrequency(sensor_type_));
}

mojom::ReportingMode FakeSensor::GetReportingMode() {
  return mojom::ReportingMode::ON_CHANGE;
}

double FakeSensor::GetMaximumSupportedFrequency() {
  return GetSensorMaxAllowedFrequency(sensor_type_);
}

double FakeSensor::GetMinimumSupportedFrequency() {
  return 1.0;
}

mojo::PendingReceiver<mojom::SensorClient> FakeSensor::GetClient() {
  return client_.BindNewPipeAndPassReceiver();
}

uint64_t FakeSensor::GetBufferOffset() {
  return GetSensorReadingSharedBufferOffset(sensor_type_);
}

void FakeSensor::SetReading(SensorReading reading) {
  reading_ = reading;
  SensorReadingChanged();
}

void FakeSensor::SensorReadingChanged() {
  auto& seqlock = buffer_->seqlock.value();
  seqlock.WriteBegin();
  buffer_->reading = reading_;
  seqlock.WriteEnd();

  if (client_ && reading_notification_enabled_)
    client_->SensorReadingChanged();
}

FakeSensorProvider::FakeSensorProvider() = default;

FakeSensorProvider::~FakeSensorProvider() = default;

void FakeSensorProvider::GetSensor(mojom::SensorType type,
                                   GetSensorCallback callback) {
  if (!CreateSharedBufferIfNeeded()) {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
    return;
  }

  if (sensor_requested_callback_) {
    std::move(sensor_requested_callback_).Run(type);
  }

  SensorReadingSharedBuffer* buffer = GetSensorReadingSharedBufferForType(type);

  std::unique_ptr<FakeSensor> sensor;

  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT:
      if (ambient_light_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(mojom::SensorType::AMBIENT_LIGHT,
                                              buffer);
        ambient_light_sensor_ = sensor.get();
        ambient_light_sensor_->SetReading(ambient_light_sensor_reading_);
      }
      break;
    case mojom::SensorType::ACCELEROMETER:
      if (accelerometer_is_available_) {
        sensor = std::make_unique<FakeSensor>(mojom::SensorType::ACCELEROMETER,
                                              buffer);
        accelerometer_ = sensor.get();
        accelerometer_->SetReading(accelerometer_reading_);
      }
      break;
    case mojom::SensorType::LINEAR_ACCELERATION:
      if (linear_acceleration_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            mojom::SensorType::LINEAR_ACCELERATION, buffer);
        linear_acceleration_sensor_ = sensor.get();
        linear_acceleration_sensor_->SetReading(
            linear_acceleration_sensor_reading_);
      }
      break;
    case mojom::SensorType::GRAVITY:
      if (gravity_sensor_is_available_) {
        sensor =
            std::make_unique<FakeSensor>(mojom::SensorType::GRAVITY, buffer);
        gravity_sensor_ = sensor.get();
        gravity_sensor_->SetReading(gravity_sensor_reading_);
      }
      break;
    case mojom::SensorType::GYROSCOPE:
      if (gyroscope_is_available_) {
        sensor =
            std::make_unique<FakeSensor>(mojom::SensorType::GYROSCOPE, buffer);
        gyroscope_ = sensor.get();
        gyroscope_->SetReading(gyroscope_reading_);
      }
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      if (relative_orientation_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES, buffer);
        relative_orientation_sensor_ = sensor.get();
        relative_orientation_sensor_->SetReading(
            relative_orientation_sensor_reading_);
      }
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      if (absolute_orientation_sensor_is_available_) {
        sensor = std::make_unique<FakeSensor>(
            mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES, buffer);
        absolute_orientation_sensor_ = sensor.get();
        absolute_orientation_sensor_->SetReading(
            absolute_orientation_sensor_reading_);
      }
      break;
    default:
      NOTIMPLEMENTED();
  }

  if (sensor) {
    auto init_params = mojom::SensorInitParams::New();
    init_params->client_receiver = sensor->GetClient();
    init_params->memory = mapped_region_.region.Duplicate();
    init_params->buffer_offset = sensor->GetBufferOffset();
    init_params->default_configuration = sensor->GetDefaultConfiguration();
    init_params->maximum_frequency = sensor->GetMaximumSupportedFrequency();
    init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();

    mojo::MakeSelfOwnedReceiver(
        std::move(sensor),
        init_params->sensor.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(mojom::SensorCreationResult::SUCCESS,
                            std::move(init_params));
  } else {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
  }
}

void FakeSensorProvider::Bind(
    mojo::PendingReceiver<mojom::SensorProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

bool FakeSensorProvider::is_bound() const {
  return !receivers_.empty();
}

void FakeSensorProvider::SetAmbientLightSensorData(double value) {
  ambient_light_sensor_reading_.als.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  ambient_light_sensor_reading_.als.value = value;
}

void FakeSensorProvider::SetAccelerometerData(double x, double y, double z) {
  accelerometer_reading_.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  accelerometer_reading_.accel.x = x;
  accelerometer_reading_.accel.y = y;
  accelerometer_reading_.accel.z = z;
}

void FakeSensorProvider::SetLinearAccelerationSensorData(double x,
                                                         double y,
                                                         double z) {
  linear_acceleration_sensor_reading_.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  linear_acceleration_sensor_reading_.accel.x = x;
  linear_acceleration_sensor_reading_.accel.y = y;
  linear_acceleration_sensor_reading_.accel.z = z;
}

void FakeSensorProvider::SetGravitySensorData(double x, double y, double z) {
  gravity_sensor_reading_.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  gravity_sensor_reading_.accel.x = x;
  gravity_sensor_reading_.accel.y = y;
  gravity_sensor_reading_.accel.z = z;
}

void FakeSensorProvider::SetGyroscopeData(double x, double y, double z) {
  gyroscope_reading_.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  gyroscope_reading_.gyro.x = x;
  gyroscope_reading_.gyro.y = y;
  gyroscope_reading_.gyro.z = z;
}

void FakeSensorProvider::SetRelativeOrientationSensorData(double alpha,
                                                          double beta,
                                                          double gamma) {
  relative_orientation_sensor_reading_.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  relative_orientation_sensor_reading_.orientation_euler.x = beta;
  relative_orientation_sensor_reading_.orientation_euler.y = gamma;
  relative_orientation_sensor_reading_.orientation_euler.z = alpha;
}

void FakeSensorProvider::SetAbsoluteOrientationSensorData(double alpha,
                                                          double beta,
                                                          double gamma) {
  absolute_orientation_sensor_reading_.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  absolute_orientation_sensor_reading_.orientation_euler.x = beta;
  absolute_orientation_sensor_reading_.orientation_euler.y = gamma;
  absolute_orientation_sensor_reading_.orientation_euler.z = alpha;
}

void FakeSensorProvider::UpdateAmbientLightSensorData(double value) {
  SetAmbientLightSensorData(value);
  EXPECT_TRUE(ambient_light_sensor_);
  ambient_light_sensor_->SetReading(ambient_light_sensor_reading_);
}

void FakeSensorProvider::UpdateAccelerometerData(double x, double y, double z) {
  SetAccelerometerData(x, y, z);
  EXPECT_TRUE(accelerometer_);
  accelerometer_->SetReading(accelerometer_reading_);
}

void FakeSensorProvider::UpdateLinearAccelerationSensorData(double x,
                                                            double y,
                                                            double z) {
  SetLinearAccelerationSensorData(x, y, z);
  EXPECT_TRUE(linear_acceleration_sensor_);
  linear_acceleration_sensor_->SetReading(linear_acceleration_sensor_reading_);
}

void FakeSensorProvider::UpdateGravitySensorData(double x, double y, double z) {
  SetGravitySensorData(x, y, z);
  EXPECT_TRUE(gravity_sensor_);
  gravity_sensor_->SetReading(gravity_sensor_reading_);
}

void FakeSensorProvider::UpdateGyroscopeData(double x, double y, double z) {
  SetGyroscopeData(x, y, z);
  EXPECT_TRUE(gyroscope_);
  gyroscope_->SetReading(gyroscope_reading_);
}

void FakeSensorProvider::UpdateRelativeOrientationSensorData(double alpha,
                                                             double beta,
                                                             double gamma) {
  SetRelativeOrientationSensorData(alpha, beta, gamma);
  EXPECT_TRUE(relative_orientation_sensor_);
  relative_orientation_sensor_->SetReading(
      relative_orientation_sensor_reading_);
}

void FakeSensorProvider::UpdateAbsoluteOrientationSensorData(double alpha,
                                                             double beta,
                                                             double gamma) {
  SetAbsoluteOrientationSensorData(alpha, beta, gamma);
  EXPECT_TRUE(absolute_orientation_sensor_);
  absolute_orientation_sensor_->SetReading(
      absolute_orientation_sensor_reading_);
}

bool FakeSensorProvider::CreateSharedBufferIfNeeded() {
  if (mapped_region_.IsValid())
    return true;

  mapped_region_ =
      base::ReadOnlySharedMemoryRegion::Create(kSharedBufferSizeInBytes);
  return mapped_region_.IsValid();
}

SensorReadingSharedBuffer*
FakeSensorProvider::GetSensorReadingSharedBufferForType(
    mojom::SensorType type) {
  base::span<SensorReadingSharedBuffer> buffers =
      mapped_region_.mapping.GetMemoryAsSpan<SensorReadingSharedBuffer>();
  if (buffers.empty()) {
    return nullptr;
  }

  size_t offset = GetSensorReadingSharedBufferOffset(type);
  CHECK(offset % sizeof(SensorReadingSharedBuffer) == 0u);

  SensorReadingSharedBuffer& buffer =
      buffers[offset / sizeof(SensorReadingSharedBuffer)];
  std::ranges::fill(base::byte_span_from_ref(buffer), 0u);
  return &buffer;
}

bool FakeSensorProvider::WaitForAccelerometerSuspend(bool suspend) {
  CHECK(accelerometer_is_available_);
  return accelerometer_->WaitForSuspend(suspend);
}

bool FakeSensorProvider::WaitForAmbientLightSensorSuspend(bool suspend) {
  CHECK(ambient_light_sensor_is_available_);
  return ambient_light_sensor_->WaitForSuspend(suspend);
}

bool FakeSensorProvider::WaitForLinearAccelerationSensorSuspend(bool suspend) {
  CHECK(linear_acceleration_sensor_is_available_);
  return linear_acceleration_sensor_->WaitForSuspend(suspend);
}

bool FakeSensorProvider::WaitForGravitySensorSuspend(bool suspend) {
  CHECK(gravity_sensor_is_available_);
  return gravity_sensor_->WaitForSuspend(suspend);
}

bool FakeSensorProvider::WaitForGyroscopeSuspend(bool suspend) {
  CHECK(gyroscope_is_available_);
  return gyroscope_->WaitForSuspend(suspend);
}

}  // namespace device
