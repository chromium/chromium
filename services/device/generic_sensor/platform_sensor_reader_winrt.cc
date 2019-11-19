// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_winrt.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/math_constants.h"
#include "base/win/core_winrt_util.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

namespace {
using ABI::Windows::Devices::Sensors::Accelerometer;
using ABI::Windows::Devices::Sensors::AccelerometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::Gyrometer;
using ABI::Windows::Devices::Sensors::GyrometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IAccelerometer;
using ABI::Windows::Devices::Sensors::IAccelerometerReading;
using ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IAccelerometerStatics;
using ABI::Windows::Devices::Sensors::IGyrometer;
using ABI::Windows::Devices::Sensors::IGyrometerReading;
using ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IGyrometerStatics;
using ABI::Windows::Devices::Sensors::IInclinometer;
using ABI::Windows::Devices::Sensors::IInclinometerReading;
using ABI::Windows::Devices::Sensors::IInclinometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IInclinometerStatics;
using ABI::Windows::Devices::Sensors::ILightSensor;
using ABI::Windows::Devices::Sensors::ILightSensorReading;
using ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::ILightSensorStatics;
using ABI::Windows::Devices::Sensors::IMagnetometer;
using ABI::Windows::Devices::Sensors::IMagnetometerReading;
using ABI::Windows::Devices::Sensors::IMagnetometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IMagnetometerStatics;
using ABI::Windows::Devices::Sensors::Inclinometer;
using ABI::Windows::Devices::Sensors::InclinometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IOrientationSensor;
using ABI::Windows::Devices::Sensors::IOrientationSensorReading;
using ABI::Windows::Devices::Sensors::IOrientationSensorReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::IOrientationSensorStatics;
using ABI::Windows::Devices::Sensors::ISensorQuaternion;
using ABI::Windows::Devices::Sensors::LightSensor;
using ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::Magnetometer;
using ABI::Windows::Devices::Sensors::MagnetometerReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::OrientationSensor;
using ABI::Windows::Devices::Sensors::OrientationSensorReadingChangedEventArgs;
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

void RecordSensorStartResult(HRESULT result) {
  base::UmaHistogramSparse("Sensors.Windows.WinRT.Start.Result", result);
}

double GetAngleBetweenOrientationSamples(SensorReading reading1,
                                         SensorReading reading2) {
  auto dot_product = reading1.orientation_quat.x * reading2.orientation_quat.x +
                     reading1.orientation_quat.y * reading2.orientation_quat.y +
                     reading1.orientation_quat.z * reading2.orientation_quat.z +
                     reading1.orientation_quat.w * reading2.orientation_quat.w;

  return 2.0 * acos(dot_product);
}
}  // namespace

std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtFactory::Create(mojom::SensorType type) {
  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT:
      return PlatformSensorReaderWinrtLightSensor::Create();
    case mojom::SensorType::ACCELEROMETER:
      return PlatformSensorReaderWinrtAccelerometer::Create();
    case mojom::SensorType::GYROSCOPE:
      return PlatformSensorReaderWinrtGyrometer::Create();
    case mojom::SensorType::MAGNETOMETER:
      return PlatformSensorReaderWinrtMagnetometer::Create();
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      return PlatformSensorReaderWinrtAbsOrientationEulerAngles::Create();
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return PlatformSensorReaderWinrtAbsOrientationQuaternion::Create();
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::PlatformSensorReaderWinrtBase() {
  get_sensor_factory_callback_ =
      base::Bind([](ISensorWinrtStatics** sensor_factory) -> HRESULT {
        return base::win::GetActivationFactory<ISensorWinrtStatics,
                                               runtime_class_id>(
            sensor_factory);
      });
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
void PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::SetClient(Client* client) {
  base::AutoLock autolock(lock_);
  client_ = client;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
template <class ISensorReading>
HRESULT PlatformSensorReaderWinrtBase<runtime_class_id,
                                      ISensorWinrtStatics,
                                      ISensorWinrtClass,
                                      ISensorReadingChangedHandler,
                                      ISensorReadingChangedEventArgs>::
    ConvertSensorReadingTimeStamp(ComPtr<ISensorReading> sensor_reading,
                                  base::TimeDelta* timestamp_delta) {
  DateTime timestamp;
  HRESULT hr = sensor_reading->get_Timestamp(&timestamp);
  if (FAILED(hr))
    return hr;

  *timestamp_delta = base::TimeDelta::FromWinrtDateTime(timestamp);

  return S_OK;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
bool PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::Initialize() {
  ComPtr<ISensorWinrtStatics> sensor_statics;

  HRESULT hr = get_sensor_factory_callback_.Run(&sensor_statics);

  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get sensor activation factory: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = sensor_statics->GetDefault(&sensor_);
  if (FAILED(hr)) {
    base::UmaHistogramSparse("Sensors.Windows.WinRT.Activation.Result", hr);
    DLOG(ERROR) << "Failed to query default sensor: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  // GetDefault() returns null if the sensor does not exist
  if (!sensor_) {
    // https://docs.microsoft.com/en-us/windows/win32/api/sensorsapi/nf-sensorsapi-isensormanager-getsensorsbytype
    // The Win32 flavor returns HRESULT_FROM_WIN32(ERROR_NOT_FOUND) when the
    // sensor is not found so log the same error result here as well.
    base::UmaHistogramSparse("Sensors.Windows.WinRT.Activation.Result",
                             HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    VLOG(1) << "Sensor does not exist on system";
    return false;
  }

  base::UmaHistogramSparse("Sensors.Windows.WinRT.Activation.Result", S_OK);

  minimum_report_interval_ = GetMinimumReportIntervalFromSensor();

  if (minimum_report_interval_.is_zero())
    DLOG(WARNING) << "Failed to get sensor minimum report interval";

  return true;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
base::TimeDelta PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::GetMinimumReportIntervalFromSensor() {
  UINT32 minimum_report_interval_ms = 0;
  HRESULT hr = sensor_->get_MinimumReportInterval(&minimum_report_interval_ms);

  // Failing to query is not fatal, consumer should be able to gracefully
  // handle a 0 return.
  if (FAILED(hr)) {
    DLOG(WARNING) << "Failed to query sensor minimum report interval: "
                  << logging::SystemErrorCodeToString(hr);

    return base::TimeDelta();
  }

  return base::TimeDelta::FromMilliseconds(minimum_report_interval_ms);
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
base::TimeDelta PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::GetMinimalReportingInterval() const {
  return minimum_report_interval_;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
bool PlatformSensorReaderWinrtBase<runtime_class_id,
                                   ISensorWinrtStatics,
                                   ISensorWinrtClass,
                                   ISensorReadingChangedHandler,
                                   ISensorReadingChangedEventArgs>::
    StartSensor(const PlatformSensorConfiguration& configuration) {
  base::AutoLock autolock(lock_);

  if (!reading_callback_token_) {
    // Convert from frequency to interval in milliseconds since that is
    // what the Windows.Devices.Sensors API uses.
    unsigned int interval =
        (1 / configuration.frequency()) * base::Time::kMillisecondsPerSecond;

    auto hr = sensor_->put_ReportInterval(interval);

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to set report interval: "
                  << logging::SystemErrorCodeToString(hr);
      RecordSensorStartResult(hr);
      return false;
    }

    auto reading_changed_handler = Callback<ISensorReadingChangedHandler>(
        this, &PlatformSensorReaderWinrtBase::OnReadingChangedCallback);

    EventRegistrationToken event_token;
    hr = sensor_->add_ReadingChanged(reading_changed_handler.Get(),
                                     &event_token);

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to add reading callback handler: "
                  << logging::SystemErrorCodeToString(hr);
      RecordSensorStartResult(hr);
      return false;
    }

    reading_callback_token_ = event_token;
    RecordSensorStartResult(hr);
  }

  return true;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
void PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::StopSensor() {
  base::AutoLock autolock(lock_);

  if (reading_callback_token_) {
    HRESULT hr =
        sensor_->remove_ReadingChanged(reading_callback_token_.value());

    base::UmaHistogramSparse("Sensors.Windows.WinRT.Stop.Result", hr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to remove ALS reading callback handler: "
                  << logging::SystemErrorCodeToString(hr);
    }

    reading_callback_token_ = base::nullopt;
  }
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::~PlatformSensorReaderWinrtBase() {
  StopSensor();
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtLightSensor::Create() {
  auto light_sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  if (light_sensor->Initialize()) {
    return light_sensor;
  }
  return nullptr;
}

PlatformSensorReaderWinrtLightSensor::PlatformSensorReaderWinrtLightSensor() =
    default;

HRESULT PlatformSensorReaderWinrtLightSensor::OnReadingChangedCallback(
    ILightSensor* light_sensor,
    ILightSensorReadingChangedEventArgs* reading_changed_args) {
  ComPtr<ILightSensorReading> light_sensor_reading;
  HRESULT hr = reading_changed_args->get_Reading(&light_sensor_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get the sensor reading: "
                << logging::SystemErrorCodeToString(hr);
    // Failing to parse a reading sample should not be fatal so always
    // return S_OK.
    return S_OK;
  }

  float lux = 0.0f;
  hr = light_sensor_reading->get_IlluminanceInLux(&lux);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get the lux level: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(light_sensor_reading, &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get sensor reading timestamp: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  if (!has_received_first_sample_ ||
      (abs(lux - last_reported_lux_) >=
       (last_reported_lux_ * kLuxPercentThreshold))) {
    SensorReading reading;
    reading.als.value = lux;
    reading.als.timestamp = timestamp_delta.InSecondsF();
    client_->OnReadingUpdated(reading);

    last_reported_lux_ = lux;
    has_received_first_sample_ = true;
  }

  return S_OK;
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtAccelerometer::Create() {
  auto accelerometer =
      std::make_unique<PlatformSensorReaderWinrtAccelerometer>();
  if (accelerometer->Initialize()) {
    return accelerometer;
  }
  return nullptr;
}

PlatformSensorReaderWinrtAccelerometer::
    PlatformSensorReaderWinrtAccelerometer() = default;

HRESULT PlatformSensorReaderWinrtAccelerometer::OnReadingChangedCallback(
    IAccelerometer* accelerometer,
    IAccelerometerReadingChangedEventArgs* reading_changed_args) {
  ComPtr<IAccelerometerReading> accelerometer_reading;
  HRESULT hr = reading_changed_args->get_Reading(&accelerometer_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get acc reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  double x = 0.0;
  hr = accelerometer_reading->get_AccelerationX(&x);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get x axis from acc reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  double y = 0.0;
  hr = accelerometer_reading->get_AccelerationY(&y);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get y axis from acc reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  double z = 0.0;
  hr = accelerometer_reading->get_AccelerationZ(&z);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get z axis from acc reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(accelerometer_reading, &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get sensor reading timestamp: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  if (!has_received_first_sample_ ||
      (abs(x - last_reported_x_) >= kAxisThreshold) ||
      (abs(y - last_reported_y_) >= kAxisThreshold) ||
      (abs(z - last_reported_z_) >= kAxisThreshold)) {
    // Windows.Devices.Sensors.Accelerometer exposes acceleration as
    // proportional and in the same direction as the force of gravity.
    // The generic sensor interface exposes acceleration simply as
    // m/s^2, so the data must be converted.
    SensorReading reading;
    reading.accel.x = -x * base::kMeanGravityDouble;
    reading.accel.y = -y * base::kMeanGravityDouble;
    reading.accel.z = -z * base::kMeanGravityDouble;
    reading.accel.timestamp = timestamp_delta.InSecondsF();
    client_->OnReadingUpdated(reading);

    last_reported_x_ = x;
    last_reported_y_ = y;
    last_reported_z_ = z;
    has_received_first_sample_ = true;
  }

  return S_OK;
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtGyrometer::Create() {
  auto gyrometer = std::make_unique<PlatformSensorReaderWinrtGyrometer>();
  if (gyrometer->Initialize()) {
    return gyrometer;
  }
  return nullptr;
}

PlatformSensorReaderWinrtGyrometer::PlatformSensorReaderWinrtGyrometer() =
    default;

HRESULT PlatformSensorReaderWinrtGyrometer::OnReadingChangedCallback(
    IGyrometer* gyrometer,
    IGyrometerReadingChangedEventArgs* reading_changed_args) {
  ComPtr<IGyrometerReading> gyrometer_reading;
  HRESULT hr = reading_changed_args->get_Reading(&gyrometer_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to gyro reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  double x = 0.0;
  hr = gyrometer_reading->get_AngularVelocityX(&x);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get x axis from gyro reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  double y = 0.0;
  hr = gyrometer_reading->get_AngularVelocityY(&y);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get y axis from gyro reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  double z = 0.0;
  hr = gyrometer_reading->get_AngularVelocityZ(&z);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get z axis from gyro reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(gyrometer_reading, &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get timestamp from gyro reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  if (!has_received_first_sample_ ||
      (abs(x - last_reported_x_) >= kDegreeThreshold) ||
      (abs(y - last_reported_y_) >= kDegreeThreshold) ||
      (abs(z - last_reported_z_) >= kDegreeThreshold)) {
    // Windows.Devices.Sensors.Gyrometer exposes angular velocity as degrees,
    // but the generic sensor interface uses radians so the data must be
    // converted.
    SensorReading reading;
    reading.gyro.x = gfx::DegToRad(x);
    reading.gyro.y = gfx::DegToRad(y);
    reading.gyro.z = gfx::DegToRad(z);
    reading.gyro.timestamp = timestamp_delta.InSecondsF();
    client_->OnReadingUpdated(reading);

    last_reported_x_ = x;
    last_reported_y_ = y;
    last_reported_z_ = z;
    has_received_first_sample_ = true;
  }

  return S_OK;
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtMagnetometer::Create() {
  auto magnetometer = std::make_unique<PlatformSensorReaderWinrtMagnetometer>();
  if (magnetometer->Initialize()) {
    return magnetometer;
  }
  return nullptr;
}

PlatformSensorReaderWinrtMagnetometer::PlatformSensorReaderWinrtMagnetometer() =
    default;

HRESULT PlatformSensorReaderWinrtMagnetometer::OnReadingChangedCallback(
    IMagnetometer* magnetometer,
    IMagnetometerReadingChangedEventArgs* reading_changed_args) {
  ComPtr<IMagnetometerReading> magnetometer_reading;
  HRESULT hr = reading_changed_args->get_Reading(&magnetometer_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get mag reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float x = 0.0;
  hr = magnetometer_reading->get_MagneticFieldX(&x);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get x axis from mag reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float y = 0.0;
  hr = magnetometer_reading->get_MagneticFieldY(&y);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get y axis from mag reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float z = 0.0;
  hr = magnetometer_reading->get_MagneticFieldZ(&z);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get z axis from mag reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(magnetometer_reading, &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get timestamp from mag reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  if (!has_received_first_sample_ ||
      (abs(x - last_reported_x_) >= kMicroteslaThreshold) ||
      (abs(y - last_reported_y_) >= kMicroteslaThreshold) ||
      (abs(z - last_reported_z_) >= kMicroteslaThreshold)) {
    SensorReading reading;
    reading.magn.x = x;
    reading.magn.y = y;
    reading.magn.z = z;
    reading.magn.timestamp = timestamp_delta.InSecondsF();
    client_->OnReadingUpdated(reading);

    last_reported_x_ = x;
    last_reported_y_ = y;
    last_reported_z_ = z;
    has_received_first_sample_ = true;
  }

  return S_OK;
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtAbsOrientationEulerAngles::Create() {
  auto inclinometer =
      std::make_unique<PlatformSensorReaderWinrtAbsOrientationEulerAngles>();
  if (inclinometer->Initialize()) {
    return inclinometer;
  }
  return nullptr;
}

PlatformSensorReaderWinrtAbsOrientationEulerAngles::
    PlatformSensorReaderWinrtAbsOrientationEulerAngles() = default;

HRESULT
PlatformSensorReaderWinrtAbsOrientationEulerAngles::OnReadingChangedCallback(
    IInclinometer* inclinometer,
    IInclinometerReadingChangedEventArgs* reading_changed_args) {
  ComPtr<IInclinometerReading> inclinometer_reading;
  HRESULT hr = reading_changed_args->get_Reading(&inclinometer_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get inclinometer reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float x = 0.0;
  hr = inclinometer_reading->get_PitchDegrees(&x);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get pitch from inclinometer reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float y = 0.0;
  hr = inclinometer_reading->get_RollDegrees(&y);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get roll from inclinometer reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float z = 0.0;
  hr = inclinometer_reading->get_YawDegrees(&z);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get yaw from inclinometer reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(inclinometer_reading, &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get timestamp from inclinometer reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  if (!has_received_first_sample_ ||
      (abs(x - last_reported_x_) >= kDegreeThreshold) ||
      (abs(y - last_reported_y_) >= kDegreeThreshold) ||
      (abs(z - last_reported_z_) >= kDegreeThreshold)) {
    SensorReading reading;
    reading.orientation_euler.x = x;
    reading.orientation_euler.y = y;
    reading.orientation_euler.z = z;
    reading.orientation_euler.timestamp = timestamp_delta.InSecondsF();
    client_->OnReadingUpdated(reading);

    last_reported_x_ = x;
    last_reported_y_ = y;
    last_reported_z_ = z;
    has_received_first_sample_ = true;
  }

  return S_OK;
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtAbsOrientationQuaternion::Create() {
  auto orientation =
      std::make_unique<PlatformSensorReaderWinrtAbsOrientationQuaternion>();
  if (orientation->Initialize()) {
    return orientation;
  }
  return nullptr;
}

PlatformSensorReaderWinrtAbsOrientationQuaternion::
    PlatformSensorReaderWinrtAbsOrientationQuaternion() = default;

PlatformSensorReaderWinrtAbsOrientationQuaternion::
    ~PlatformSensorReaderWinrtAbsOrientationQuaternion() = default;

HRESULT
PlatformSensorReaderWinrtAbsOrientationQuaternion::OnReadingChangedCallback(
    IOrientationSensor* orientation_sensor,
    IOrientationSensorReadingChangedEventArgs* reading_changed_args) {
  ComPtr<IOrientationSensorReading> orientation_sensor_reading;
  HRESULT hr = reading_changed_args->get_Reading(&orientation_sensor_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  ComPtr<ISensorQuaternion> quaternion;
  hr = orientation_sensor_reading->get_Quaternion(&quaternion);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get quaternion from orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float w = 0.0;
  hr = quaternion->get_W(&w);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get w component of orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float x = 0.0;
  hr = quaternion->get_X(&x);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get x component of orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float y = 0.0;
  hr = quaternion->get_Y(&y);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get y component of orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  float z = 0.0;
  hr = quaternion->get_Z(&z);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get the z component of orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(orientation_sensor_reading,
                                     &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get timestamp from orientation reading: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  SensorReading reading;
  reading.orientation_quat.w = w;
  reading.orientation_quat.x = x;
  reading.orientation_quat.y = y;
  reading.orientation_quat.z = z;
  reading.orientation_quat.timestamp = timestamp_delta.InSecondsF();

  // As per
  // https://docs.microsoft.com/en-us/windows-hardware/drivers/sensors/orientation-sensor-thresholds,
  // thresholding should be done on angle between two quaternions:
  // 2 * cos-1(dot_product(q1, q2))
  auto angle =
      abs(GetAngleBetweenOrientationSamples(reading, last_reported_sample));
  if (!has_received_first_sample_ || (angle >= kRadianThreshold)) {
    client_->OnReadingUpdated(reading);

    last_reported_sample = reading;
    has_received_first_sample_ = true;
  }

  return S_OK;
}

}  // namespace device
