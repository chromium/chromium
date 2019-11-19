// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_

#include <windows.devices.sensors.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <functional>
#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

namespace mojom {
enum class SensorType;
}

// Helper class used to create PlatformSensorReaderWinrt instances
class PlatformSensorReaderWinrtFactory {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create(
      mojom::SensorType type);
};

// Base class that contains common helper functions used between all low
// level sensor types based on the Windows.Devices.Sensors API. Derived
// classes will specialize the template into a specific sensor. See
// PlatformSensorReaderWinrtLightSensor as an example of what WinRT
// interfaces should be passed in. The owner of this class must guarantee
// construction and destruction occur on the same thread and that no
// other thread is accessing it during destruction.
// TODO(crbug.com/995594): Change Windows.Devices.Sensors based
//   implementation of W3C sensor API to use hardware thresholding.
template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
class PlatformSensorReaderWinrtBase : public PlatformSensorReaderWinBase {
 public:
  using GetSensorFactoryFunctor =
      base::Callback<HRESULT(ISensorWinrtStatics**)>;

  // Sets the client to notify changes about. The consumer should always
  // ensure the lifetime of the client surpasses the lifetime of this class.
  void SetClient(Client* client) override;

  // Allows tests to specify their own implementation of the underlying sensor.
  // This function should be called before Initialize().
  void InitForTesting(GetSensorFactoryFunctor get_sensor_factory_callback) {
    get_sensor_factory_callback_ = get_sensor_factory_callback;
  }

  // Returns true if the underlying WinRT sensor object is valid, meant
  // for testing purposes.
  bool IsUnderlyingWinrtObjectValidForTesting() { return sensor_; }

  bool Initialize() WARN_UNUSED_RESULT;

  bool StartSensor(const PlatformSensorConfiguration& configuration) override
      WARN_UNUSED_RESULT;
  base::TimeDelta GetMinimalReportingInterval() const override;
  void StopSensor() override;

 protected:
  PlatformSensorReaderWinrtBase();
  virtual ~PlatformSensorReaderWinrtBase();

  // Derived classes should implement this function to handle sensor specific
  // parsing of the sensor reading.
  virtual HRESULT OnReadingChangedCallback(
      ISensorWinrtClass* sensor,
      ISensorReadingChangedEventArgs* reading_changed_args) = 0;

  // Helper function which converts the DateTime timestamp format the
  // Windows.Devices.Sensors API uses to the second time ticks the
  // client expects.
  template <class ISensorReading>
  HRESULT ConvertSensorReadingTimeStamp(
      Microsoft::WRL::ComPtr<ISensorReading> sensor_reading,
      base::TimeDelta* timestamp_delta);

  // Following class member is protected by lock since SetClient,
  // StartSensor, and StopSensor can all be called from different
  // threads by PlatformSensorWin.
  base::Lock lock_;
  // Null if there is no client to notify, non-null otherwise.
  Client* client_;

  // Always report the first sample received after starting the sensor.
  bool has_received_first_sample_ = false;

 private:
  base::TimeDelta GetMinimumReportIntervalFromSensor();

  GetSensorFactoryFunctor get_sensor_factory_callback_;

  // base::nullopt if the sensor has not been started, non-empty otherwise.
  base::Optional<EventRegistrationToken> reading_callback_token_;

  base::TimeDelta minimum_report_interval_;
  Microsoft::WRL::ComPtr<ISensorWinrtClass> sensor_;
};

class PlatformSensorReaderWinrtLightSensor final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_LightSensor,
          ABI::Windows::Devices::Sensors::ILightSensorStatics,
          ABI::Windows::Devices::Sensors::ILightSensor,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::LightSensor*,
                  ABI::Windows::Devices::Sensors::
                      LightSensorReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs> {
 public:
  // Lux scales exponentially with perceived brightness so use a relative
  // threshold instead of an absolute one.
  static constexpr float kLuxPercentThreshold = 0.2f;  // 20%

  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtLightSensor();
  ~PlatformSensorReaderWinrtLightSensor() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::ILightSensor* sensor,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  float last_reported_lux_ = 0.0f;

  PlatformSensorReaderWinrtLightSensor(
      const PlatformSensorReaderWinrtLightSensor&) = delete;
  PlatformSensorReaderWinrtLightSensor& operator=(
      const PlatformSensorReaderWinrtLightSensor&) = delete;
};

class PlatformSensorReaderWinrtAccelerometer final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Accelerometer,
          ABI::Windows::Devices::Sensors::IAccelerometerStatics,
          ABI::Windows::Devices::Sensors::IAccelerometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Accelerometer*,
                  ABI::Windows::Devices::Sensors::
                      AccelerometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IAccelerometerReadingChangedEventArgs> {
 public:
  static constexpr double kAxisThreshold = 0.1f;

  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtAccelerometer();
  ~PlatformSensorReaderWinrtAccelerometer() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IAccelerometer* sensor,
      ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  double last_reported_x_ = 0.0;
  double last_reported_y_ = 0.0;
  double last_reported_z_ = 0.0;

  PlatformSensorReaderWinrtAccelerometer(
      const PlatformSensorReaderWinrtAccelerometer&) = delete;
  PlatformSensorReaderWinrtAccelerometer& operator=(
      const PlatformSensorReaderWinrtAccelerometer&) = delete;
};

class PlatformSensorReaderWinrtGyrometer final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Gyrometer,
          ABI::Windows::Devices::Sensors::IGyrometerStatics,
          ABI::Windows::Devices::Sensors::IGyrometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Gyrometer*,
                  ABI::Windows::Devices::Sensors::
                      GyrometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs> {
 public:
  static constexpr double kDegreeThreshold = 5.0;

  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtGyrometer();
  ~PlatformSensorReaderWinrtGyrometer() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IGyrometer* sensor,
      ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  double last_reported_x_ = 0.0;
  double last_reported_y_ = 0.0;
  double last_reported_z_ = 0.0;

  PlatformSensorReaderWinrtGyrometer(
      const PlatformSensorReaderWinrtGyrometer&) = delete;
  PlatformSensorReaderWinrtGyrometer& operator=(
      const PlatformSensorReaderWinrtGyrometer&) = delete;
};

class PlatformSensorReaderWinrtMagnetometer final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Magnetometer,
          ABI::Windows::Devices::Sensors::IMagnetometerStatics,
          ABI::Windows::Devices::Sensors::IMagnetometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Magnetometer*,
                  ABI::Windows::Devices::Sensors::
                      MagnetometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IMagnetometerReadingChangedEventArgs> {
 public:
  static constexpr double kMicroteslaThreshold = 5.0f;

  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtMagnetometer();
  ~PlatformSensorReaderWinrtMagnetometer() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IMagnetometer* sensor,
      ABI::Windows::Devices::Sensors::IMagnetometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  double last_reported_x_ = 0.0;
  double last_reported_y_ = 0.0;
  double last_reported_z_ = 0.0;

  PlatformSensorReaderWinrtMagnetometer(
      const PlatformSensorReaderWinrtMagnetometer&) = delete;
  PlatformSensorReaderWinrtMagnetometer& operator=(
      const PlatformSensorReaderWinrtMagnetometer&) = delete;
};

class PlatformSensorReaderWinrtAbsOrientationEulerAngles final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Inclinometer,
          ABI::Windows::Devices::Sensors::IInclinometerStatics,
          ABI::Windows::Devices::Sensors::IInclinometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Inclinometer*,
                  ABI::Windows::Devices::Sensors::
                      InclinometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IInclinometerReadingChangedEventArgs> {
 public:
  static constexpr double kDegreeThreshold = 5.0f;

  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtAbsOrientationEulerAngles();
  ~PlatformSensorReaderWinrtAbsOrientationEulerAngles() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IInclinometer* sensor,
      ABI::Windows::Devices::Sensors::IInclinometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  double last_reported_x_ = 0.0;
  double last_reported_y_ = 0.0;
  double last_reported_z_ = 0.0;

  PlatformSensorReaderWinrtAbsOrientationEulerAngles(
      const PlatformSensorReaderWinrtAbsOrientationEulerAngles&) = delete;
  PlatformSensorReaderWinrtAbsOrientationEulerAngles& operator=(
      const PlatformSensorReaderWinrtAbsOrientationEulerAngles&) = delete;
};

class PlatformSensorReaderWinrtAbsOrientationQuaternion final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_OrientationSensor,
          ABI::Windows::Devices::Sensors::IOrientationSensorStatics,
          ABI::Windows::Devices::Sensors::IOrientationSensor,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::OrientationSensor*,
                  ABI::Windows::Devices::Sensors::
                      OrientationSensorReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IOrientationSensorReadingChangedEventArgs> {
 public:
  static constexpr double kRadianThreshold = gfx::DegToRad(5.0);

  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtAbsOrientationQuaternion();
  ~PlatformSensorReaderWinrtAbsOrientationQuaternion() override;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IOrientationSensor* sensor,
      ABI::Windows::Devices::Sensors::IOrientationSensorReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  SensorReading last_reported_sample{};

  PlatformSensorReaderWinrtAbsOrientationQuaternion(
      const PlatformSensorReaderWinrtAbsOrientationQuaternion&) = delete;
  PlatformSensorReaderWinrtAbsOrientationQuaternion& operator=(
      const PlatformSensorReaderWinrtAbsOrientationQuaternion&) = delete;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_