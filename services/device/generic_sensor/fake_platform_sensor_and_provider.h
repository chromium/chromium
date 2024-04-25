// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_AND_PROVIDER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_AND_PROVIDER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class FakePlatformSensor : public PlatformSensor {
 public:
  FakePlatformSensor(mojom::SensorType type,
                     SensorReadingSharedBuffer* reading_buffer,
                     base::WeakPtr<PlatformSensorProvider> provider);

  FakePlatformSensor(const FakePlatformSensor&) = delete;
  FakePlatformSensor& operator=(const FakePlatformSensor&) = delete;

  // PlatformSensor:
  MOCK_METHOD1(StartSensor,
               bool(const PlatformSensorConfiguration& configuration));
  mojom::ReportingMode GetReportingMode() override;

  void set_maximum_supported_frequency(double maximum_supported_frequency) {
    maximum_supported_frequency_ = maximum_supported_frequency;
  }

  // Public interface to UpdateSharedBufferAndNotifyClients().
  void AddNewReading(const SensorReading& reading);

 protected:
  void StopSensor() override {}

  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

  PlatformSensorConfiguration GetDefaultConfiguration() override;

  double GetMaximumSupportedFrequency() override;
  double GetMinimumSupportedFrequency() override;

  double maximum_supported_frequency_ = 50.0;

  ~FakePlatformSensor() override;
};

class FakePlatformSensorProvider : public PlatformSensorProvider {
 public:
  FakePlatformSensorProvider();

  FakePlatformSensorProvider(const FakePlatformSensorProvider&) = delete;
  FakePlatformSensorProvider& operator=(const FakePlatformSensorProvider&) =
      delete;

  ~FakePlatformSensorProvider() override;

  MOCK_METHOD0(FreeResources, void());
  MOCK_METHOD2(CreateSensorInternal,
               void(mojom::SensorType, CreateSensorCallback));

  base::WeakPtr<PlatformSensorProvider> AsWeakPtr() override;

  SensorReadingSharedBuffer* GetSensorReadingBuffer(mojom::SensorType type);

 private:
  base::WeakPtrFactory<FakePlatformSensorProvider> weak_factory_{this};
};

// Mock for PlatformSensor's client interface that is used to deliver
// error and data changes notifications.
class MockPlatformSensorClient : public PlatformSensor::Client {
 public:
  MockPlatformSensorClient();
  // For the given |sensor| this client will be automatically
  // added in the costructor and removed in the destructor.
  explicit MockPlatformSensorClient(scoped_refptr<PlatformSensor> sensor);

  MockPlatformSensorClient(const MockPlatformSensorClient&) = delete;
  MockPlatformSensorClient& operator=(const MockPlatformSensorClient&) = delete;

  ~MockPlatformSensorClient() override;

  scoped_refptr<PlatformSensor> sensor() const { return sensor_; }

  // PlatformSensor::Client:
  MOCK_METHOD1(OnSensorReadingChanged, void(mojom::SensorType type));
  MOCK_METHOD0(OnSensorError, void());
  MOCK_METHOD0(IsSuspended, bool());

 private:
  scoped_refptr<PlatformSensor> sensor_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_AND_PROVIDER_H_
