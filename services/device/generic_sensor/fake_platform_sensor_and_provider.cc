// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"

#include <utility>

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

}  // namespace

namespace device {

FakePlatformSensor::FakePlatformSensor(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    base::WeakPtr<PlatformSensorProvider> provider)
    : PlatformSensor(type, reading_buffer, std::move(provider)) {
  ON_CALL(*this, StartSensor(_))
      .WillByDefault(
          Invoke([this](const PlatformSensorConfiguration& configuration) {
            SensorReading reading;
            // Only mocking the shared memory update for AMBIENT_LIGHT and
            // ACCELEROMETER types is enough.
            // Set the shared buffer value as frequency for testing purpose.
            switch (GetType()) {
              case mojom::SensorType::AMBIENT_LIGHT:
                reading.als.value = configuration.frequency();
                AddNewReading(reading);
                break;
              case mojom::SensorType::ACCELEROMETER:
                reading.accel.x = reading.accel.y = reading.accel.z =
                    configuration.frequency();
                AddNewReading(reading);
                break;
              default:
                break;
            }
            return true;
          }));
}

FakePlatformSensor::~FakePlatformSensor() = default;

bool FakePlatformSensor::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() <= GetMaximumSupportedFrequency() &&
         configuration.frequency() >= GetMinimumSupportedFrequency();
}

PlatformSensorConfiguration FakePlatformSensor::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(30.0);
}

mojom::ReportingMode FakePlatformSensor::GetReportingMode() {
  // Set the ReportingMode as ON_CHANGE, so we can test the
  // SensorReadingChanged() mojo interface.
  return mojom::ReportingMode::ON_CHANGE;
}

double FakePlatformSensor::GetMaximumSupportedFrequency() {
  return maximum_supported_frequency_;
}

double FakePlatformSensor::GetMinimumSupportedFrequency() {
  return 1.0;
}

void FakePlatformSensor::AddNewReading(const SensorReading& reading) {
  UpdateSharedBufferAndNotifyClients(reading);
}

FakePlatformSensorProvider::FakePlatformSensorProvider() {
  ON_CALL(*this, CreateSensorInternal)
      .WillByDefault(
          Invoke([this](mojom::SensorType type,
                        PlatformSensorProvider::CreateSensorCallback callback) {
            DCHECK(type >= mojom::SensorType::kMinValue &&
                   type <= mojom::SensorType::kMaxValue);
            std::move(callback).Run(
                base::MakeRefCounted<NiceMock<FakePlatformSensor>>(
                    type, GetSensorReadingBuffer(type), AsWeakPtr()));
          }));
}

FakePlatformSensorProvider::~FakePlatformSensorProvider() = default;

base::WeakPtr<PlatformSensorProvider> FakePlatformSensorProvider::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

SensorReadingSharedBuffer* FakePlatformSensorProvider::GetSensorReadingBuffer(
    mojom::SensorType type) {
  return CreateSharedBufferIfNeeded()
             ? GetSensorReadingSharedBufferForType(type)
             : nullptr;
}

MockPlatformSensorClient::MockPlatformSensorClient() {
  ON_CALL(*this, IsSuspended()).WillByDefault(Return(false));
}

MockPlatformSensorClient::MockPlatformSensorClient(
    scoped_refptr<PlatformSensor> sensor)
    : MockPlatformSensorClient() {
  DCHECK(sensor);
  sensor_ = std::move(sensor);
  sensor_->AddClient(this);
}

MockPlatformSensorClient::~MockPlatformSensorClient() {
  if (sensor_)
    sensor_->RemoveClient(this);
}

}  // namespace device
