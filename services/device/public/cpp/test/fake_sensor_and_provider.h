// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SENSOR_AND_PROVIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SENSOR_AND_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

template <class T>
struct SensorReadingSharedBufferImpl;
using SensorReadingSharedBuffer = SensorReadingSharedBufferImpl<void>;

// This encapsulates the pattern of waiting for an event and returning whether
// that event was received from `Wait`. This makes it easy to do the right thing
// in Wait, i.e. return with `[[nodiscard]]`.
class WaiterHelper {
 public:
  // Wait until OnEvent is called. Will return true if ended by OnEvent or false
  // if ended for some other reason (e.g. timeout).
  [[nodiscard]] bool Wait();
  // Stops the waiting.
  void OnEvent();

 private:
  [[nodiscard]] bool WaitInternal();
  base::RunLoop run_loop_;
  bool event_received_ = false;
};

class FakeSensor : public mojom::Sensor {
 public:
  FakeSensor(mojom::SensorType sensor_type, SensorReadingSharedBuffer* buffer);

  FakeSensor(const FakeSensor&) = delete;
  FakeSensor& operator=(const FakeSensor&) = delete;

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

  bool WaitForSuspend(bool suspend);

 private:
  void SensorReadingChanged();

  mojom::SensorType sensor_type_;
  raw_ptr<SensorReadingSharedBuffer> buffer_;
  bool reading_notification_enabled_ = true;
  mojo::Remote<mojom::SensorClient> client_;
  SensorReading reading_;
  WaiterHelper suspend_waiter_;
  WaiterHelper resume_waiter_;
  base::OnceCallback<void()> suspend_callback_;
};

class FakeSensorProvider : public mojom::SensorProvider {
 public:
  FakeSensorProvider();

  FakeSensorProvider(const FakeSensorProvider&) = delete;
  FakeSensorProvider& operator=(const FakeSensorProvider&) = delete;

  ~FakeSensorProvider() override;

  // mojom::sensorProvider:
  void GetSensor(mojom::SensorType type, GetSensorCallback callback) override;
  void CreateVirtualSensor(
      mojom::SensorType type,
      mojom::VirtualSensorMetadataPtr metadata,
      mojom::SensorProvider::CreateVirtualSensorCallback callback) override {}
  void UpdateVirtualSensor(
      mojom::SensorType type,
      const SensorReading& reading,
      mojom::SensorProvider::UpdateVirtualSensorCallback callback) override {}
  void RemoveVirtualSensor(
      mojom::SensorType type,
      mojom::SensorProvider::RemoveVirtualSensorCallback callback) override {}
  void GetVirtualSensorInformation(
      mojom::SensorType type,
      mojom::SensorProvider::GetVirtualSensorInformationCallback callback)
      override {}

  void Bind(mojo::PendingReceiver<mojom::SensorProvider> receiver);
  bool is_bound() const;

  // Configures a callback which is invoked when GetSensor() is called, allowing
  // tests to take an action before the response callback is invoked.
  void set_sensor_requested_callback(
      base::OnceCallback<void(mojom::SensorType)> callback) {
    sensor_requested_callback_ = std::move(callback);
  }

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
  void set_gravity_sensor_is_available(bool gravity_sensor_is_available) {
    gravity_sensor_is_available_ = gravity_sensor_is_available;
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
  void SetGravitySensorData(double x, double y, double z);
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
  void UpdateGravitySensorData(double x, double y, double z);
  void UpdateGyroscopeData(double x, double y, double z);
  void UpdateRelativeOrientationSensorData(double alpha,
                                           double beta,
                                           double gamma);
  void UpdateAbsoluteOrientationSensorData(double alpha,
                                           double beta,
                                           double gamma);

  bool WaitForAccelerometerSuspend(bool suspend);
  bool WaitForAmbientLightSensorSuspend(bool suspend);
  bool WaitForLinearAccelerationSensorSuspend(bool suspend);
  bool WaitForGravitySensorSuspend(bool suspend);
  bool WaitForGyroscopeSuspend(bool suspend);

 private:
  bool CreateSharedBufferIfNeeded();
  SensorReadingSharedBuffer* GetSensorReadingSharedBufferForType(
      mojom::SensorType type);

  // The following sensor pointers are owned by the caller of
  // FakeSensorProvider::GetSensor().
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged> ambient_light_sensor_ =
      nullptr;
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged> accelerometer_ = nullptr;
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged>
      linear_acceleration_sensor_ = nullptr;
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged> gravity_sensor_ = nullptr;
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged> gyroscope_ = nullptr;
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged>
      relative_orientation_sensor_ = nullptr;
  raw_ptr<FakeSensor, AcrossTasksDanglingUntriaged>
      absolute_orientation_sensor_ = nullptr;

  SensorReading ambient_light_sensor_reading_;
  SensorReading accelerometer_reading_;
  SensorReading linear_acceleration_sensor_reading_;
  SensorReading gravity_sensor_reading_;
  SensorReading gyroscope_reading_;
  SensorReading relative_orientation_sensor_reading_;
  SensorReading absolute_orientation_sensor_reading_;
  base::OnceCallback<void(mojom::SensorType)> sensor_requested_callback_;
  bool ambient_light_sensor_is_available_ = true;
  bool accelerometer_is_available_ = true;
  bool linear_acceleration_sensor_is_available_ = true;
  bool gravity_sensor_is_available_ = true;
  bool gyroscope_is_available_ = true;
  bool relative_orientation_sensor_is_available_ = true;
  bool absolute_orientation_sensor_is_available_ = true;
  mojo::ReceiverSet<mojom::SensorProvider> receivers_{};
  base::MappedReadOnlyRegion mapped_region_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SENSOR_AND_PROVIDER_H_
