// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SENSOR_AND_PROVIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SENSOR_AND_PROVIDER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

class FakeSensor : public mojom::Sensor {
 public:
  FakeSensor(mojom::SensorType sensor_type, SensorReadingSharedBuffer* buffer);
  ~FakeSensor() override;

  // mojom::Sensor:
  void AddConfiguration(const PlatformSensorConfiguration& configuration,
                        AddConfigurationCallback callback) override;
  void GetDefaultConfiguration(
      GetDefaultConfigurationCallback callback) override;
  void RemoveConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  void Suspend() override;
  void Resume() override;
  void ConfigureReadingChangeNotifications(bool enabled) override;

  PlatformSensorConfiguration GetDefaultConfiguration();
  mojom::ReportingMode GetReportingMode();
  double GetMaximumSupportedFrequency();
  double GetMinimumSupportedFrequency();
  mojo::PendingReceiver<mojom::SensorClient> GetClient();
  mojo::ScopedSharedBufferHandle GetSharedBufferHandle();
  uint64_t GetBufferOffset();
  void SetReading(SensorReading reading);

 private:
  void SensorReadingChanged();

  mojom::SensorType sensor_type_;
  SensorReadingSharedBuffer* buffer_;
  bool reading_notification_enabled_ = true;
  mojo::Remote<mojom::SensorClient> client_;
  SensorReading reading_;

  DISALLOW_COPY_AND_ASSIGN(FakeSensor);
};

class FakeSensorProvider : public mojom::SensorProvider {
 public:
  FakeSensorProvider();
  ~FakeSensorProvider() override;

  // mojom::sensorProvider:
  void GetSensor(mojom::SensorType type, GetSensorCallback callback) override;

  void Bind(mojo::PendingReceiver<mojom::SensorProvider> receiver);

  void set_ambient_light_sensor_is_available(
      bool ambient_light_sensor_is_available) {
    ambient_light_sensor_is_available_ = ambient_light_sensor_is_available;
  }
  void set_accelerometer_is_available(bool accelerometer_is_available) {
    accelerometer_is_available_ = accelerometer_is_available;
  }
  void set_linear_acceleration_sensor_is_available(
      bool linear_acceleration_sensor_is_available) {
    linear_acceleration_sensor_is_available_ =
        linear_acceleration_sensor_is_available;
  }
  void set_gyroscope_is_available(bool gyroscope_is_available) {
    gyroscope_is_available_ = gyroscope_is_available;
  }
  void set_relative_orientation_sensor_is_available(
      bool relative_orientation_sensor_is_available) {
    relative_orientation_sensor_is_available_ =
        relative_orientation_sensor_is_available;
  }
  void set_absolute_orientation_sensor_is_available(
      bool absolute_orientation_sensor_is_available) {
    absolute_orientation_sensor_is_available_ =
        absolute_orientation_sensor_is_available;
  }

  void SetAmbientLightSensorData(double value);
  void SetAccelerometerData(double x, double y, double z);
  void SetLinearAccelerationSensorData(double x, double y, double z);
  void SetGyroscopeData(double x, double y, double z);
  void SetRelativeOrientationSensorData(double alpha,
                                        double beta,
                                        double gamma);
  void SetAbsoluteOrientationSensorData(double alpha,
                                        double beta,
                                        double gamma);

  // The Update* functions here write the sensor data to the shared memory and
  // notify sensor's client that the sensor data has changed. The Set*
  // functions above only set |*_reading_| member variable for corresponding
  // sensor which will be the value when the sensor is first created.
  void UpdateAmbientLightSensorData(double value);
  void UpdateAccelerometerData(double x, double y, double z);
  void UpdateLinearAccelerationSensorData(double x, double y, double z);
  void UpdateGyroscopeData(double x, double y, double z);
  void UpdateRelativeOrientationSensorData(double alpha,
                                           double beta,
                                           double gamma);
  void UpdateAbsoluteOrientationSensorData(double alpha,
                                           double beta,
                                           double gamma);

 private:
  bool CreateSharedBufferIfNeeded();
  SensorReadingSharedBuffer* GetSensorReadingSharedBufferForType(
      mojom::SensorType type);

  // The following sensor pointers are owned by the caller of
  // FakeSensorProvider::GetSensor().
  FakeSensor* ambient_light_sensor_ = nullptr;
  FakeSensor* accelerometer_ = nullptr;
  FakeSensor* linear_acceleration_sensor_ = nullptr;
  FakeSensor* gyroscope_ = nullptr;
  FakeSensor* relative_orientation_sensor_ = nullptr;
  FakeSensor* absolute_orientation_sensor_ = nullptr;

  SensorReading ambient_light_sensor_reading_;
  SensorReading accelerometer_reading_;
  SensorReading linear_acceleration_sensor_reading_;
  SensorReading gyroscope_reading_;
  SensorReading relative_orientation_sensor_reading_;
  SensorReading absolute_orientation_sensor_reading_;
  bool ambient_light_sensor_is_available_ = true;
  bool accelerometer_is_available_ = true;
  bool linear_acceleration_sensor_is_available_ = true;
  bool gyroscope_is_available_ = true;
  bool relative_orientation_sensor_is_available_ = true;
  bool absolute_orientation_sensor_is_available_ = true;
  mojo::ReceiverSet<mojom::SensorProvider> receivers_{};
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping shared_buffer_mapping_;

  DISALLOW_COPY_AND_ASSIGN(FakeSensorProvider);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SENSOR_AND_PROVIDER_H_
