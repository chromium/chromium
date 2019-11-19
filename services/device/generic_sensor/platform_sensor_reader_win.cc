// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_win.h"

#include <Sensors.h>
#include <comdef.h>
#include <objbase.h>
#include <wrl/implements.h>

#include <iomanip>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/math_constants.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/scoped_propvariant.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

// Init params for the PlatformSensorReaderWin32.
struct ReaderInitParams {
  // ISensorDataReport::GetSensorValue is not const, therefore, report
  // cannot be passed as const ref.
  // ISensorDataReport* report - report that contains new sensor data.
  // SensorReading* reading    - out parameter that must be populated.
  // Returns HRESULT           - S_OK on success, otherwise error code.
  using ReaderFunctor = HRESULT (*)(ISensorDataReport* report,
                                    SensorReading* reading);
  SENSOR_TYPE_ID sensor_type_id;
  ReaderFunctor reader_func;
  base::TimeDelta min_reporting_interval;
};

namespace {

// Gets value from  the report for provided key.
bool GetReadingValueForProperty(REFPROPERTYKEY key,
                                ISensorDataReport* report,
                                double* value) {
  DCHECK(value);
  base::win::ScopedPropVariant variant_value;
  if (SUCCEEDED(report->GetSensorValue(key, variant_value.Receive()))) {
    if (variant_value.get().vt == VT_R8)
      *value = variant_value.get().dblVal;
    else if (variant_value.get().vt == VT_R4)
      *value = variant_value.get().fltVal;
    else
      return false;
    return true;
  }

  *value = 0;
  return false;
}

// Ambient light sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateAmbientLightReaderInitParams() {
  auto params = std::make_unique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_AMBIENT_LIGHT;
  params->reader_func = [](ISensorDataReport* report, SensorReading* reading) {
    double lux = 0.0;
    if (!GetReadingValueForProperty(SENSOR_DATA_TYPE_LIGHT_LEVEL_LUX, report,
                                    &lux)) {
      return E_FAIL;
    }
    reading->als.value = lux;
    return S_OK;
  };
  return params;
}

// Accelerometer sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateAccelerometerReaderInitParams() {
  auto params = std::make_unique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_ACCELEROMETER_3D;
  params->reader_func = [](ISensorDataReport* report, SensorReading* reading) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!GetReadingValueForProperty(SENSOR_DATA_TYPE_ACCELERATION_X_G, report,
                                    &x) ||
        !GetReadingValueForProperty(SENSOR_DATA_TYPE_ACCELERATION_Y_G, report,
                                    &y) ||
        !GetReadingValueForProperty(SENSOR_DATA_TYPE_ACCELERATION_Z_G, report,
                                    &z)) {
      return E_FAIL;
    }

    // Windows HW sensor integration requirements specify accelerometer
    // measurements conventions such as, the accelerometer sensor must expose
    // values that are proportional and in the same direction as the force of
    // gravity. Therefore, sensor hosted by the device at rest on a leveled
    // surface while the screen is facing towards the sky, must report -1G along
    // the Z axis.
    // https://msdn.microsoft.com/en-us/library/windows/hardware/dn642102(v=vs.85).aspx
    // Change sign of values, to report 'reaction force', and convert values
    // from G/s^2 to m/s^2 units.
    reading->accel.x = -x * base::kMeanGravityDouble;
    reading->accel.y = -y * base::kMeanGravityDouble;
    reading->accel.z = -z * base::kMeanGravityDouble;
    return S_OK;
  };
  return params;
}

// Gyroscope sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateGyroscopeReaderInitParams() {
  auto params = std::make_unique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_GYROMETER_3D;
  params->reader_func = [](ISensorDataReport* report, SensorReading* reading) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!GetReadingValueForProperty(
            SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND, report,
            &x) ||
        !GetReadingValueForProperty(
            SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND, report,
            &y) ||
        !GetReadingValueForProperty(
            SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND, report,
            &z)) {
      return E_FAIL;
    }

    // Values are converted from degrees to radians.
    reading->gyro.x = gfx::DegToRad(x);
    reading->gyro.y = gfx::DegToRad(y);
    reading->gyro.z = gfx::DegToRad(z);
    return S_OK;
  };
  return params;
}

// Magnetometer sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateMagnetometerReaderInitParams() {
  auto params = std::make_unique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_COMPASS_3D;
  params->reader_func = [](ISensorDataReport* report, SensorReading* reading) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!GetReadingValueForProperty(
            SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_X_MILLIGAUSS, report,
            &x) ||
        !GetReadingValueForProperty(
            SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Y_MILLIGAUSS, report,
            &y) ||
        !GetReadingValueForProperty(
            SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Z_MILLIGAUSS, report,
            &z)) {
      return E_FAIL;
    }

    // Values are converted from Milligaus to Microtesla.
    reading->magn.x = x * kMicroteslaInMilligauss;
    reading->magn.y = y * kMicroteslaInMilligauss;
    reading->magn.z = z * kMicroteslaInMilligauss;
    return S_OK;
  };
  return params;
}

// AbsoluteOrientationEulerAngles sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams>
CreateAbsoluteOrientationEulerAnglesReaderInitParams() {
  auto params = std::make_unique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_INCLINOMETER_3D;
  params->reader_func = [](ISensorDataReport* report, SensorReading* reading) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!GetReadingValueForProperty(SENSOR_DATA_TYPE_TILT_X_DEGREES, report,
                                    &x) ||
        !GetReadingValueForProperty(SENSOR_DATA_TYPE_TILT_Y_DEGREES, report,
                                    &y) ||
        !GetReadingValueForProperty(SENSOR_DATA_TYPE_TILT_Z_DEGREES, report,
                                    &z)) {
      return E_FAIL;
    }

    reading->orientation_euler.x = x;
    reading->orientation_euler.y = y;
    reading->orientation_euler.z = z;
    return S_OK;
  };
  return params;
}

// AbsoluteOrientationQuaternion sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams>
CreateAbsoluteOrientationQuaternionReaderInitParams() {
  auto params = std::make_unique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION;
  params->reader_func = [](ISensorDataReport* report, SensorReading* reading) {
    base::win::ScopedPropVariant quat_variant;
    HRESULT hr = report->GetSensorValue(SENSOR_DATA_TYPE_QUATERNION,
                                        quat_variant.Receive());
    if (FAILED(hr) || quat_variant.get().vt != (VT_VECTOR | VT_UI1) ||
        quat_variant.get().caub.cElems < 16) {
      return E_FAIL;
    }

    float* quat = reinterpret_cast<float*>(quat_variant.get().caub.pElems);

    reading->orientation_quat.x = quat[0];  // x*sin(Theta/2)
    reading->orientation_quat.y = quat[1];  // y*sin(Theta/2)
    reading->orientation_quat.z = quat[2];  // z*sin(Theta/2)
    reading->orientation_quat.w = quat[3];  // cos(Theta/2)
    return S_OK;
  };
  return params;
}

// Creates ReaderInitParams params structure. To implement support for new
// sensor types, new switch case should be added and appropriate fields must
// be set:
// sensor_type_id - GUID of the sensor supported by Windows.
// reader_func    - Functor that is responsible to populate SensorReading from
//                  ISensorDataReport data.
std::unique_ptr<ReaderInitParams> CreateReaderInitParamsForSensor(
    mojom::SensorType type) {
  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT:
      return CreateAmbientLightReaderInitParams();
    case mojom::SensorType::ACCELEROMETER:
      return CreateAccelerometerReaderInitParams();
    case mojom::SensorType::GYROSCOPE:
      return CreateGyroscopeReaderInitParams();
    case mojom::SensorType::MAGNETOMETER:
      return CreateMagnetometerReaderInitParams();
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      return CreateAbsoluteOrientationEulerAnglesReaderInitParams();
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return CreateAbsoluteOrientationQuaternionReaderInitParams();
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

}  // namespace

// Class that implements ISensorEvents used by the ISensor interface to dispatch
// state and data change events.
class EventListener
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISensorEvents> {
 public:
  explicit EventListener(PlatformSensorReaderWin32* platform_sensor_reader)
      : platform_sensor_reader_(platform_sensor_reader) {
    DCHECK(platform_sensor_reader_);
  }

  static Microsoft::WRL::ComPtr<ISensorEvents> CreateInstance(
      PlatformSensorReaderWin32* platform_sensor_reader) {
    Microsoft::WRL::ComPtr<EventListener> event_listener =
        Microsoft::WRL::Make<EventListener>(platform_sensor_reader);
    Microsoft::WRL::ComPtr<ISensorEvents> sensor_events;
    HRESULT hr = event_listener.As(&sensor_events);
    DCHECK(SUCCEEDED(hr));
    return sensor_events;
  }

 protected:
  ~EventListener() override = default;

  // ISensorEvents interface
  IFACEMETHODIMP OnEvent(ISensor*, REFGUID, IPortableDeviceValues*) override {
    return S_OK;
  }

  IFACEMETHODIMP OnLeave(REFSENSOR_ID sensor_id) override {
    // If event listener is active and sensor is disconnected, notify client
    // about the error.
    platform_sensor_reader_->SensorError();
    platform_sensor_reader_->StopSensor();
    return S_OK;
  }

  IFACEMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) override {
    if (sensor == nullptr)
      return E_INVALIDARG;

    if (state != SensorState::SENSOR_STATE_READY &&
        state != SensorState::SENSOR_STATE_INITIALIZING) {
      platform_sensor_reader_->SensorError();
      platform_sensor_reader_->StopSensor();
    }
    return S_OK;
  }

  IFACEMETHODIMP OnDataUpdated(ISensor* sensor,
                               ISensorDataReport* report) override {
    if (sensor == nullptr || report == nullptr)
      return E_INVALIDARG;

    // To get precise timestamp, we need to get delta between timestamp
    // provided in the report and current system time. Then the delta in
    // milliseconds is substracted from current high resolution timestamp.
    SYSTEMTIME report_time;
    HRESULT hr = report->GetTimestamp(&report_time);
    if (FAILED(hr))
      return hr;

    base::TimeTicks ticks_now = base::TimeTicks::Now();
    base::Time time_now = base::Time::NowFromSystemTime();

    base::Time::Exploded exploded;
    exploded.year = report_time.wYear;
    exploded.month = report_time.wMonth;
    exploded.day_of_week = report_time.wDayOfWeek;
    exploded.day_of_month = report_time.wDay;
    exploded.hour = report_time.wHour;
    exploded.minute = report_time.wMinute;
    exploded.second = report_time.wSecond;
    exploded.millisecond = report_time.wMilliseconds;

    base::Time timestamp;
    if (!base::Time::FromUTCExploded(exploded, &timestamp))
      return E_FAIL;

    base::TimeDelta delta = time_now - timestamp;

    SensorReading reading;
    reading.raw.timestamp =
        ((ticks_now - delta) - base::TimeTicks()).InSecondsF();

    // Discard update events that have non-monotonically increasing timestamp.
    if (last_sensor_reading_.raw.timestamp > reading.timestamp())
      return E_FAIL;

    hr = platform_sensor_reader_->SensorReadingChanged(report, &reading);
    if (SUCCEEDED(hr))
      last_sensor_reading_ = reading;
    return hr;
  }

 private:
  PlatformSensorReaderWin32* const platform_sensor_reader_;
  SensorReading last_sensor_reading_;

  DISALLOW_COPY_AND_ASSIGN(EventListener);
};

// static
std::unique_ptr<PlatformSensorReaderWinBase> PlatformSensorReaderWin32::Create(
    mojom::SensorType type,
    Microsoft::WRL::ComPtr<ISensorManager> sensor_manager) {
  DCHECK(sensor_manager);

  auto params = CreateReaderInitParamsForSensor(type);
  if (!params)
    return nullptr;

  auto sensor = GetSensorForType(params->sensor_type_id, sensor_manager);
  if (!sensor)
    return nullptr;

  base::win::ScopedPropVariant min_interval;
  HRESULT hr = sensor->GetProperty(SENSOR_PROPERTY_MIN_REPORT_INTERVAL,
                                   min_interval.Receive());
  if (SUCCEEDED(hr) && min_interval.get().vt == VT_UI4) {
    params->min_reporting_interval =
        base::TimeDelta::FromMilliseconds(min_interval.get().ulVal);
  }

  GUID interests[] = {SENSOR_EVENT_STATE_CHANGED, SENSOR_EVENT_DATA_UPDATED};
  hr = sensor->SetEventInterest(interests, base::size(interests));
  if (FAILED(hr))
    return nullptr;

  return base::WrapUnique(
      new PlatformSensorReaderWin32(sensor, std::move(params)));
}

// static
Microsoft::WRL::ComPtr<ISensor> PlatformSensorReaderWin32::GetSensorForType(
    REFSENSOR_TYPE_ID sensor_type,
    Microsoft::WRL::ComPtr<ISensorManager> sensor_manager) {
  Microsoft::WRL::ComPtr<ISensor> sensor;
  Microsoft::WRL::ComPtr<ISensorCollection> sensor_collection;
  HRESULT hr = sensor_manager->GetSensorsByType(
      sensor_type, sensor_collection.GetAddressOf());
  base::UmaHistogramSparse("Sensors.Windows.ISensor.Activation.Result", hr);
  if (FAILED(hr) || !sensor_collection)
    return sensor;

  ULONG count = 0;
  hr = sensor_collection->GetCount(&count);
  if (SUCCEEDED(hr) && count > 0)
    sensor_collection->GetAt(0, sensor.GetAddressOf());
  return sensor;
}

PlatformSensorReaderWin32::PlatformSensorReaderWin32(
    Microsoft::WRL::ComPtr<ISensor> sensor,
    std::unique_ptr<ReaderInitParams> params)
    : init_params_(std::move(params)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      sensor_active_(false),
      client_(nullptr),
      sensor_(sensor),
      event_listener_(EventListener::CreateInstance(this)) {
  DCHECK(init_params_);
  DCHECK(init_params_->reader_func);
  DCHECK(sensor_);
}

void PlatformSensorReaderWin32::SetClient(Client* client) {
  base::AutoLock autolock(lock_);
  // Can be null.
  client_ = client;
}

void PlatformSensorReaderWin32::StopSensor() {
  base::AutoLock autolock(lock_);
  if (sensor_active_) {
    HRESULT hr = sensor_->SetEventSink(nullptr);
    base::UmaHistogramSparse("Sensors.Windows.ISensor.Stop.Result", hr);
    sensor_active_ = false;
  }
}

PlatformSensorReaderWin32::~PlatformSensorReaderWin32() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

bool PlatformSensorReaderWin32::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  base::AutoLock autolock(lock_);

  if (!SetReportingInterval(configuration))
    return false;

  if (!sensor_active_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PlatformSensorReaderWin32::ListenSensorEvent,
                                  weak_factory_.GetWeakPtr()));
    sensor_active_ = true;
  }

  return true;
}

void PlatformSensorReaderWin32::ListenSensorEvent() {
  // Set event listener.
  HRESULT hr = sensor_->SetEventSink(event_listener_.Get());
  base::UmaHistogramSparse("Sensors.Windows.ISensor.Start.Result", hr);
  if (FAILED(hr)) {
    SensorError();
    StopSensor();
  }
}

bool PlatformSensorReaderWin32::SetReportingInterval(
    const PlatformSensorConfiguration& configuration) {
  Microsoft::WRL::ComPtr<IPortableDeviceValues> props;
  HRESULT hr = ::CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&props));
  if (FAILED(hr)) {
    static bool logged_failure = false;
    if (!logged_failure) {
      LOG(ERROR) << "Unable to create instance of PortableDeviceValues: "
                 << _com_error(hr).ErrorMessage() << " (0x" << std::hex
                 << std::uppercase << std::setfill('0') << std::setw(8) << hr
                 << ")";
      logged_failure = true;
    }
    return false;
  }

  unsigned interval =
      (1 / configuration.frequency()) * base::Time::kMillisecondsPerSecond;
  hr = props->SetUnsignedIntegerValue(SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL,
                                      interval);
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IPortableDeviceValues> return_props;
  hr = sensor_->SetProperties(props.Get(), return_props.GetAddressOf());
  return SUCCEEDED(hr);
}

HRESULT PlatformSensorReaderWin32::SensorReadingChanged(
    ISensorDataReport* report,
    SensorReading* reading) const {
  if (!client_)
    return E_FAIL;

  HRESULT hr = init_params_->reader_func(report, reading);
  if (SUCCEEDED(hr))
    client_->OnReadingUpdated(*reading);
  return hr;
}

void PlatformSensorReaderWin32::SensorError() {
  if (client_)
    client_->OnSensorError();
}

base::TimeDelta PlatformSensorReaderWin32::GetMinimalReportingInterval() const {
  return init_params_->min_reporting_interval;
}

}  // namespace device
