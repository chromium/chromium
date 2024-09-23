// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_winrt.h"

#include <objbase.h>

#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_com_initializer.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace device {

static constexpr unsigned long kExpectedMinimumReportInterval = 213;
static constexpr double kExpectedReportFrequencySet = 25.0;
static constexpr unsigned long kExpectedReportIntervalSet = 40;

// Base mock class for the Windows::Devices::Sensors::I*SensorReadings
template <class ISensorReading>
class FakeSensorReadingWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensorReading> {
 public:
  FakeSensorReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp)
      : time_stamp_(time_stamp) {}

  IFACEMETHODIMP get_Timestamp(
      ABI::Windows::Foundation::DateTime* time_stamp) override {
    *time_stamp = time_stamp_;
    return get_timestamp_return_code_;
  }

  void SetGetTimestampReturnCode(HRESULT return_code) {
    get_timestamp_return_code_ = return_code;
  }

 protected:
  ABI::Windows::Foundation::DateTime time_stamp_;
  HRESULT get_timestamp_return_code_ = S_OK;
};

class FakeLightSensorReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::ILightSensorReading> {
 public:
  FakeLightSensorReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp,
                              float lux)
      : FakeSensorReadingWinrt(time_stamp), lux_(lux) {}

  ~FakeLightSensorReadingWinrt() override = default;

  IFACEMETHODIMP get_IlluminanceInLux(float* lux) override {
    *lux = lux_;
    return get_illuminance_in_lux_return_code_;
  }

  void SetGetIlluminanceInLuxReturnCode(HRESULT return_code) {
    get_illuminance_in_lux_return_code_ = return_code;
  }

 private:
  float lux_;
  HRESULT get_illuminance_in_lux_return_code_ = S_OK;
};

class FakeAccelerometerReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::IAccelerometerReading> {
 public:
  FakeAccelerometerReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp,
                                double x,
                                double y,
                                double z)
      : FakeSensorReadingWinrt(time_stamp), x_(x), y_(y), z_(z) {}
  ~FakeAccelerometerReadingWinrt() override = default;

  IFACEMETHODIMP get_AccelerationX(double* x) override {
    *x = x_;
    return get_x_return_code_;
  }

  IFACEMETHODIMP get_AccelerationY(double* y) override {
    *y = y_;
    return get_y_return_code_;
  }

  IFACEMETHODIMP get_AccelerationZ(double* z) override {
    *z = z_;
    return get_z_return_code_;
  }

  void SetGetXReturnCode(HRESULT return_code) {
    get_x_return_code_ = return_code;
  }

  void SetGetYReturnCode(HRESULT return_code) {
    get_y_return_code_ = return_code;
  }

  void SetGetZReturnCode(HRESULT return_code) {
    get_z_return_code_ = return_code;
  }

 private:
  double x_;
  double y_;
  double z_;

  HRESULT get_x_return_code_ = S_OK;
  HRESULT get_y_return_code_ = S_OK;
  HRESULT get_z_return_code_ = S_OK;
};

class FakeGyrometerReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::IGyrometerReading> {
 public:
  FakeGyrometerReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp,
                            double x,
                            double y,
                            double z)
      : FakeSensorReadingWinrt(time_stamp), x_(x), y_(y), z_(z) {}
  ~FakeGyrometerReadingWinrt() override = default;

  IFACEMETHODIMP get_AngularVelocityX(double* x) override {
    *x = x_;
    return get_x_return_code_;
  }

  IFACEMETHODIMP get_AngularVelocityY(double* y) override {
    *y = y_;
    return get_y_return_code_;
  }

  IFACEMETHODIMP get_AngularVelocityZ(double* z) override {
    *z = z_;
    return get_z_return_code_;
  }

  void SetGetXReturnCode(HRESULT return_code) {
    get_x_return_code_ = return_code;
  }

  void SetGetYReturnCode(HRESULT return_code) {
    get_y_return_code_ = return_code;
  }

  void SetGetZReturnCode(HRESULT return_code) {
    get_z_return_code_ = return_code;
  }

 private:
  double x_;
  double y_;
  double z_;

  HRESULT get_x_return_code_ = S_OK;
  HRESULT get_y_return_code_ = S_OK;
  HRESULT get_z_return_code_ = S_OK;
};

class FakeInclinometerReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::IInclinometerReading> {
 public:
  FakeInclinometerReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp,
                               float x,
                               float y,
                               float z)
      : FakeSensorReadingWinrt(time_stamp), x_(x), y_(y), z_(z) {}
  ~FakeInclinometerReadingWinrt() override = default;

  IFACEMETHODIMP get_PitchDegrees(float* x) override {
    *x = x_;
    return get_x_return_code_;
  }

  IFACEMETHODIMP get_RollDegrees(float* y) override {
    *y = y_;
    return get_y_return_code_;
  }

  IFACEMETHODIMP get_YawDegrees(float* z) override {
    *z = z_;
    return get_z_return_code_;
  }

  void SetGetXReturnCode(HRESULT return_code) {
    get_x_return_code_ = return_code;
  }

  void SetGetYReturnCode(HRESULT return_code) {
    get_y_return_code_ = return_code;
  }

  void SetGetZReturnCode(HRESULT return_code) {
    get_z_return_code_ = return_code;
  }

 private:
  float x_;
  float y_;
  float z_;

  HRESULT get_x_return_code_ = S_OK;
  HRESULT get_y_return_code_ = S_OK;
  HRESULT get_z_return_code_ = S_OK;
};

class FakeMagnetometerReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::IMagnetometerReading> {
 public:
  FakeMagnetometerReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp,
                               float x,
                               float y,
                               float z)
      : FakeSensorReadingWinrt(time_stamp), x_(x), y_(y), z_(z) {}
  ~FakeMagnetometerReadingWinrt() override = default;

  IFACEMETHODIMP get_MagneticFieldX(float* x) override {
    *x = x_;
    return get_x_return_code_;
  }

  IFACEMETHODIMP get_MagneticFieldY(float* y) override {
    *y = y_;
    return get_y_return_code_;
  }

  IFACEMETHODIMP get_MagneticFieldZ(float* z) override {
    *z = z_;
    return get_z_return_code_;
  }

  IFACEMETHODIMP get_DirectionalAccuracy(
      ABI::Windows::Devices::Sensors::MagnetometerAccuracy*) override {
    return E_NOTIMPL;
  }

  void SetGetXReturnCode(HRESULT return_code) {
    get_x_return_code_ = return_code;
  }

  void SetGetYReturnCode(HRESULT return_code) {
    get_y_return_code_ = return_code;
  }

  void SetGetZReturnCode(HRESULT return_code) {
    get_z_return_code_ = return_code;
  }

 private:
  float x_;
  float y_;
  float z_;

  HRESULT get_x_return_code_ = S_OK;
  HRESULT get_y_return_code_ = S_OK;
  HRESULT get_z_return_code_ = S_OK;
};

class FakeSensorQuaternion
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Sensors::ISensorQuaternion> {
 public:
  FakeSensorQuaternion(float w, float x, float y, float z)
      : w_(w), x_(x), y_(y), z_(z) {}
  ~FakeSensorQuaternion() override = default;

  IFACEMETHODIMP get_W(float* w) override {
    *w = w_;
    return S_OK;
  }

  IFACEMETHODIMP get_X(float* x) override {
    *x = x_;
    return S_OK;
  }

  IFACEMETHODIMP get_Y(float* y) override {
    *y = y_;
    return S_OK;
  }

  IFACEMETHODIMP get_Z(float* z) override {
    *z = z_;
    return S_OK;
  }

 private:
  float w_;
  float x_;
  float y_;
  float z_;
};

class FakeOrientationSensorReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::IOrientationSensorReading> {
 public:
  FakeOrientationSensorReadingWinrt(
      ABI::Windows::Foundation::DateTime time_stamp,
      float w,
      float x,
      float y,
      float z)
      : FakeSensorReadingWinrt(time_stamp) {
    quaternion_ = Microsoft::WRL::Make<FakeSensorQuaternion>(w, x, y, z);
  }
  ~FakeOrientationSensorReadingWinrt() override = default;

  IFACEMETHODIMP get_Quaternion(
      ABI::Windows::Devices::Sensors::ISensorQuaternion** quaternion) override {
    quaternion_.CopyTo(quaternion);
    return S_OK;
  }

  IFACEMETHODIMP get_RotationMatrix(
      ABI::Windows::Devices::Sensors::ISensorRotationMatrix** ppMatrix)
      override {
    return E_NOTIMPL;
  }

  void SetGetQuaternionReturnCode(HRESULT return_code) {
    get_quaternion_return_code_ = return_code;
  }

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Sensors::ISensorQuaternion>
      quaternion_;

  HRESULT get_quaternion_return_code_ = S_OK;
};

// Mock class for Windows::Devices::Sensors::ISensorReadingChangedEventArgs,
// allows reading changed events to return mock sensor readings.
template <class ISensorReading, class ISensorReadingChangedEventArgs>
class FakeSensorReadingChangedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensorReadingChangedEventArgs> {
 public:
  FakeSensorReadingChangedEventArgsWinrt(
      const Microsoft::WRL::ComPtr<ISensorReading>& reading)
      : reading_(reading) {}

  ~FakeSensorReadingChangedEventArgsWinrt() override = default;

  IFACEMETHODIMP get_Reading(ISensorReading** reading) override {
    return reading_.CopyTo(reading);
  }

 private:
  Microsoft::WRL::ComPtr<ISensorReading> reading_;
};

// Mock client used to receive sensor data callbacks
class MockClient : public PlatformSensorReaderWinBase::Client {
 public:
  MOCK_METHOD1(OnReadingUpdated, void(const SensorReading& reading));
  MOCK_METHOD0(OnSensorError, void());

 protected:
  ~MockClient() override = default;
};

// Base mock class for Windows.Devices.Sensors.ISensor*, injected into
// PlatformSensorReaderWinrt* to mock out the underlying
// Windows.Devices.Sensors dependency.
template <class ISensor,
          class Sensor,
          class ISensorReading,
          class ISensorReadingChangedEventArgs,
          class SensorReadingChangedEventArgs>
class FakeSensorWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensor> {
 public:
  ~FakeSensorWinrt() override = default;

  IFACEMETHODIMP GetCurrentReading(ISensorReading** ppReading) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP get_MinimumReportInterval(UINT32* pValue) override {
    *pValue = kExpectedMinimumReportInterval;
    return get_minimum_report_interval_return_code_;
  }

  IFACEMETHODIMP get_ReportInterval(UINT32* pValue) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP put_ReportInterval(UINT32 value) override {
    EXPECT_EQ(value, kExpectedReportIntervalSet);
    return put_report_interval_return_code_;
  }

  IFACEMETHODIMP add_ReadingChanged(
      ABI::Windows::Foundation::
          ITypedEventHandler<Sensor*, SensorReadingChangedEventArgs*>* pHandler,
      EventRegistrationToken* pToken) override {
    handler_ = pHandler;
    return add_reading_changed_return_code_;
  }

  IFACEMETHODIMP remove_ReadingChanged(EventRegistrationToken iToken) override {
    handler_.Reset();
    return remove_reading_changed_return_code_;
  }

  // Invokes the handler added via add_ReadingChanged() with the reading
  // described by |reading|.
  //
  // The invocation is asynchronous to better simulate real behavior, where
  // Windows delivers the reading notifications in a separate MTA thread.
  void TriggerFakeSensorReading(
      Microsoft::WRL::ComPtr<ISensorReading> reading) {
    Microsoft::WRL::ComPtr<ISensorReadingChangedEventArgs> reading_event_args =
        Microsoft::WRL::Make<FakeSensorReadingChangedEventArgsWinrt<
            ISensorReading, ISensorReadingChangedEventArgs>>(reading);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindLambdaForTesting(
            // Copy |handler_| and |reading_event_args| to ensure they do not
            // lose their values when TriggerFakeSensorReading() exits.
            [this, handler = handler_, reading_event_args]() {
              ASSERT_TRUE(handler);
              EXPECT_HRESULT_SUCCEEDED(
                  handler->Invoke(this, reading_event_args.Get()));
            }));
  }

  // Returns true if any clients are registered for readings via
  // add_ReadingChanged(), false otherwise.
  bool IsSensorStarted() { return handler_; }

  void SetGetMinimumReportIntervalReturnCode(HRESULT return_code) {
    get_minimum_report_interval_return_code_ = return_code;
  }

  void SetPutReportIntervalReturnCode(HRESULT return_code) {
    put_report_interval_return_code_ = return_code;
  }

  void SetAddReadingChangedReturnCode(HRESULT return_code) {
    add_reading_changed_return_code_ = return_code;
  }

  void SetRemoveReadingChangedReturnCode(HRESULT return_code) {
    remove_reading_changed_return_code_ = return_code;
  }

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      Sensor*,
      SensorReadingChangedEventArgs*>>
      handler_;

  HRESULT put_report_interval_return_code_ = S_OK;
  HRESULT get_minimum_report_interval_return_code_ = S_OK;
  HRESULT add_reading_changed_return_code_ = S_OK;
  HRESULT remove_reading_changed_return_code_ = S_OK;
};

class FakeAccelerometerSensorWinrt
    : public FakeSensorWinrt<
          ABI::Windows::Devices::Sensors::IAccelerometer,
          ABI::Windows::Devices::Sensors::Accelerometer,
          ABI::Windows::Devices::Sensors::IAccelerometerReading,
          ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs,
          ABI::Windows::Devices::Sensors::
              AccelerometerReadingChangedEventArgs> {
 public:
  FakeAccelerometerSensorWinrt() = default;
  ~FakeAccelerometerSensorWinrt() override = default;

  IFACEMETHODIMP add_Shaken(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Sensors::Accelerometer*,
          ABI::Windows::Devices::Sensors::AccelerometerShakenEventArgs*>*,
      EventRegistrationToken*) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP remove_Shaken(EventRegistrationToken) override {
    return E_NOTIMPL;
  }
};

template <class ISensorStatics,
          class ISensor,
          class Sensor,
          class ISensorReading,
          class ISensorReadingChangedEventArgs,
          class SensorReadingChangedEventArgs>
class FakeSensorFactoryWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensorStatics> {
 public:
  FakeSensorFactoryWinrt() {
    fake_sensor_ =
        Microsoft::WRL::Make<FakeSensorWinrt<ISensor, Sensor, ISensorReading,
                                             ISensorReadingChangedEventArgs,
                                             SensorReadingChangedEventArgs>>();
  }
  FakeSensorFactoryWinrt(
      Microsoft::WRL::ComPtr<FakeSensorWinrt<ISensor,
                                             Sensor,
                                             ISensorReading,
                                             ISensorReadingChangedEventArgs,
                                             SensorReadingChangedEventArgs>>
          fake_sensor)
      : fake_sensor_(fake_sensor) {}
  ~FakeSensorFactoryWinrt() override = default;

  IFACEMETHODIMP GetDefault(ISensor** ppResult) override {
    if (fake_sensor_ && SUCCEEDED(get_default_return_code_)) {
      return fake_sensor_.CopyTo(ppResult);
    }
    return get_default_return_code_;
  }

  Microsoft::WRL::ComPtr<FakeSensorWinrt<ISensor,
                                         Sensor,
                                         ISensorReading,
                                         ISensorReadingChangedEventArgs,
                                         SensorReadingChangedEventArgs>>
      fake_sensor_;

  void SetGetDefaultReturnCode(HRESULT return_code) {
    get_default_return_code_ = return_code;
  }

 private:
  HRESULT get_default_return_code_ = S_OK;
};

class PlatformSensorReaderTestWinrt : public testing::Test {
 public:
  void SetUp() override {
    // Ensure each test starts with a fresh task runner.
    com_sta_task_runner_ =
        base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()});
  }

  // Synchronously creates a new PlatformSensorReaderWinrtBase-derived class on
  // |com_sta_task_runner_| and returns it.
  //
  // This better simulates real behavior, as PlatformSensorProviderWinrt
  // creates readers on a COM STA task runner.
  template <typename SensorReader,
            typename ISensorStatics,
            typename... OtherFactoryTypes>
  auto CreateAndInitializeSensor(
      const Microsoft::WRL::ComPtr<
          FakeSensorFactoryWinrt<ISensorStatics, OtherFactoryTypes...>>&
          fake_sensor_factory) {
    std::unique_ptr<SensorReader, base::OnTaskRunnerDeleter> reader(
        nullptr, base::OnTaskRunnerDeleter(com_sta_task_runner_));

    base::RunLoop run_loop;
    com_sta_task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          reader.reset(new SensorReader);
          reader->InitForTesting(base::BindLambdaForTesting(
              [&](ISensorStatics** sensor_factory) -> HRESULT {
                return fake_sensor_factory.CopyTo(sensor_factory);
              }));
          ASSERT_TRUE(reader->Initialize());
        }),
        run_loop.QuitClosure());
    run_loop.Run();

    return reader;
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;

  base::test::TaskEnvironment task_environment_;
  base::win::ScopedCOMInitializer scoped_com_initializer_;
};

// Tests that PlatformSensorReaderWinrtBase returns the expected error
// if it could not create the underlying sensor.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorCreate) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();

  // Case: sensor was created successfully
  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTesting(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_TRUE(sensor->Initialize());
  EXPECT_TRUE(sensor->IsUnderlyingWinrtObjectValidForTesting());

  // Case: failed to query sensor
  fake_sensor_factory->SetGetDefaultReturnCode(E_FAIL);
  EXPECT_FALSE(sensor->Initialize());
  EXPECT_FALSE(sensor->IsUnderlyingWinrtObjectValidForTesting());
  fake_sensor_factory->SetGetDefaultReturnCode(S_OK);

  // Case: Sensor does not exist on system
  fake_sensor_factory->fake_sensor_ = nullptr;
  EXPECT_FALSE(sensor->Initialize());
  EXPECT_FALSE(sensor->IsUnderlyingWinrtObjectValidForTesting());

  // Case:: failed to activate sensor factory
  sensor->InitForTesting(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return E_FAIL; }));
  EXPECT_FALSE(sensor->Initialize());
  EXPECT_FALSE(sensor->IsUnderlyingWinrtObjectValidForTesting());
}

// Tests that PlatformSensorReaderWinrtBase returns the right
// minimum report interval.
TEST_F(PlatformSensorReaderTestWinrt, SensorMinimumReportInterval) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  EXPECT_EQ(sensor->GetMinimalReportingInterval().InMilliseconds(),
            kExpectedMinimumReportInterval);
}

// Tests that PlatformSensorReaderWinrtBase returns a 0 report interval
// when it fails to query the sensor.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorMinimumReportInterval) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;
  fake_sensor->SetGetMinimumReportIntervalReturnCode(E_FAIL);

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  EXPECT_EQ(sensor->GetMinimalReportingInterval().InMilliseconds(), 0);
}

TEST_F(PlatformSensorReaderTestWinrt, ReadingChangedCallbackAndPostTask) {
  // Instead of using PlatformSensorReaderWinrtLightSensor, declare a custom
  // implementation that does less and whose sole purpose is to assert that its
  // OnReadingChangedCallback() implementation is never called.
  struct CustomLightSensor
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
            ABI::Windows::Devices::Sensors::
                ILightSensorReadingChangedEventArgs> {
    ~CustomLightSensor() override = default;

    void OnReadingChangedCallback(
        ABI::Windows::Devices::Sensors::ILightSensor*,
        ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs*)
        override {
      NOTREACHED() << "This function should not have been reached";
    }
  };

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  // Instead of using CreateAndInitializeSensor(), for simplicity and for
  // better control over |light_sensor|'s lifetime we invert things and create
  // the object in the main task runner and invoke StartSensor() from another
  // one. The effect on the Reader is the same -- the calls are still made from
  // different task runners.
  auto light_sensor = std::make_unique<CustomLightSensor>();
  light_sensor->InitForTesting(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  ASSERT_TRUE(light_sensor->Initialize());

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(light_sensor->StartSensor(
            PlatformSensorConfiguration(kExpectedReportFrequencySet)));
      }),
      run_loop.QuitClosure());
  run_loop.Run();

  // The idea here is to rely on the fact that TriggerFakeSensorReading() is
  // asynchronous: we call it while it has a valid handler, it schedules a
  // task, we destroy the handler object synchronoustly and then it Invoke()s
  // the callback, which should never reach
  // CustomLightSensor::OnReadingChangedCallback().
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Sensors::ILightSensorReading>
      reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
          ABI::Windows::Foundation::DateTime{}, 0.0f);
  fake_sensor->TriggerFakeSensorReading(reading);
  light_sensor.reset();
  task_environment_.RunUntilIdle();
}

// Tests that PlatformSensorReaderWinrtBase converts the timestamp correctly
TEST_F(PlatformSensorReaderTestWinrt, SensorTimestampConversion) {
  static constexpr double expectedTimestampDeltaSecs = 19.0;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  // Trigger a sensor reading at time 0 (epoch), converted timestamp should
  // also be 0.
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Sensors::ILightSensorReading>
      reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
          ABI::Windows::Foundation::DateTime{}, 0.0f);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(reading.als.timestamp, 0.0);
          EXPECT_EQ(reading.als.value, 0.0f);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }

  // Verify the reported time stamp has ticked forward
  // expectedTimestampDeltaSecs
  auto second_timestamp =
      base::Seconds(expectedTimestampDeltaSecs).ToWinrtDateTime();
  reading =
      Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(second_timestamp, 0.0f);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(reading.als.timestamp, expectedTimestampDeltaSecs);
          EXPECT_EQ(reading.als.value, 0.0f);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }
}

// Tests that PlatformSensorReaderWinrtBase starts and stops the
// underlying sensor when Start() and Stop() are called.
TEST_F(PlatformSensorReaderTestWinrt, StartStopSensorCallbacks) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));
  EXPECT_TRUE(fake_sensor->IsSensorStarted());

  // Calling Start() contiguously should not cause any errors/exceptions
  EXPECT_TRUE(sensor->StartSensor(sensor_config));
  EXPECT_TRUE(fake_sensor->IsSensorStarted());

  sensor->StopSensor();
  EXPECT_FALSE(fake_sensor->IsSensorStarted());

  // Calling Stop() contiguously should not cause any errors/exceptions
  sensor->StopSensor();
  EXPECT_FALSE(fake_sensor->IsSensorStarted());
}

// Tests that PlatformSensorReaderWinrtBase stops the underlying sensor
// even if Start() is called without a following Stop() upon destruction.
TEST_F(PlatformSensorReaderTestWinrt, StartWithoutStopSensorCallbacks) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));
  EXPECT_TRUE(fake_sensor->IsSensorStarted());

  // *sensor is deleted in |com_sta_task_runner_|, so we need to wait for it to
  // happen asynchronously.
  sensor.reset();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_sensor->IsSensorStarted());
}

// Tests that PlatformSensorReaderWinrtBase::StartSensor() returns false
// when setting the report interval or registering for sensor events fails.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorStart) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  fake_sensor->SetPutReportIntervalReturnCode(E_FAIL);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_FALSE(sensor->StartSensor(sensor_config));

  fake_sensor->SetPutReportIntervalReturnCode(S_OK);
  fake_sensor->SetAddReadingChangedReturnCode(E_FAIL);

  EXPECT_FALSE(sensor->StartSensor(sensor_config));
}

// Tests that PlatformSensorReaderWinrtBase::StopSensor() swallows
// unregister sensor change errors.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorStop) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  fake_sensor->SetRemoveReadingChangedReturnCode(E_FAIL);

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtLightSensor does not notify the
// client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedLightSensorSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();

  // This test relies on the NiceMock to work, if the sensor notifies
  // the client despite not being able to parse the sensor sample then
  // the NiceMock will throw an error as no EXPECT_CALL has been set.
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0.0f);

  // We cannot use a base::RunLoop in the checks below because we are expecting
  // that MockClient::OnReadingUpdate() does _not_ get called.

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetIlluminanceInLuxReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  task_environment_.RunUntilIdle();
}

// Tests that PlatformSensorReaderWinrtLightSensor notifies the client
// and gives it the correct sensor data when a new sensor sample arrives.
TEST_F(PlatformSensorReaderTestWinrt, SensorClientNotification) {
  static constexpr float expected_lux = 123.0f;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_lux);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(expected_lux, reading.als.value);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }
  sensor->StopSensor();
}

// Tests if PlatformSensorReaderWinrtAccelerometer correctly converts sensor
// readings from Windows.Devices.Sensors.Accelerometer.
TEST_F(PlatformSensorReaderTestWinrt, CheckAccelerometerReadingConversion) {
  constexpr double expected_x = 0.25;
  constexpr double expected_y = -0.25;
  constexpr double expected_z = -0.5;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IAccelerometerStatics,
      ABI::Windows::Devices::Sensors::IAccelerometer,
      ABI::Windows::Devices::Sensors::Accelerometer,
      ABI::Windows::Devices::Sensors::IAccelerometerReading,
      ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::AccelerometerReadingChangedEventArgs>>(
      Microsoft::WRL::Make<FakeAccelerometerSensorWinrt>());
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor =
      CreateAndInitializeSensor<PlatformSensorReaderWinrtAccelerometer>(
          fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeAccelerometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_x, expected_y, expected_z);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(-expected_x * base::kMeanGravityDouble, reading.accel.x);
          EXPECT_EQ(-expected_y * base::kMeanGravityDouble, reading.accel.y);
          EXPECT_EQ(-expected_z * base::kMeanGravityDouble, reading.accel.z);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtAccelerometer does not notify the
// client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedAccelerometerSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IAccelerometerStatics,
      ABI::Windows::Devices::Sensors::IAccelerometer,
      ABI::Windows::Devices::Sensors::Accelerometer,
      ABI::Windows::Devices::Sensors::IAccelerometerReading,
      ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::AccelerometerReadingChangedEventArgs>>(
      Microsoft::WRL::Make<FakeAccelerometerSensorWinrt>());
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor =
      CreateAndInitializeSensor<PlatformSensorReaderWinrtAccelerometer>(
          fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeAccelerometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0, 0, 0);

  // We cannot use a base::RunLoop in the checks below because we are expecting
  // that MockClient::OnReadingUpdate() does _not_ get called.

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetXReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetXReturnCode(S_OK);
  reading->SetGetYReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetYReturnCode(S_OK);
  reading->SetGetZReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  task_environment_.RunUntilIdle();
}

// Tests if PlatformSensorReaderWinrtGyrometer correctly converts sensor
// readings from Windows.Devices.Sensors.Gyrometer.
TEST_F(PlatformSensorReaderTestWinrt, CheckGyrometerReadingConversion) {
  constexpr double expected_x = 0.0;
  constexpr double expected_y = -1.8;
  constexpr double expected_z = -98.7;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IGyrometerStatics,
      ABI::Windows::Devices::Sensors::IGyrometer,
      ABI::Windows::Devices::Sensors::Gyrometer,
      ABI::Windows::Devices::Sensors::IGyrometerReading,
      ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::GyrometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtGyrometer>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeGyrometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_x, expected_y, expected_z);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(base::DegToRad(expected_x), reading.gyro.x);
          EXPECT_EQ(base::DegToRad(expected_y), reading.gyro.y);
          EXPECT_EQ(base::DegToRad(expected_z), reading.gyro.z);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtGyrometer does not notify the
// client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedGyrometerSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IGyrometerStatics,
      ABI::Windows::Devices::Sensors::IGyrometer,
      ABI::Windows::Devices::Sensors::Gyrometer,
      ABI::Windows::Devices::Sensors::IGyrometerReading,
      ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::GyrometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtGyrometer>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeGyrometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0, 0, 0);

  // We cannot use a base::RunLoop in the checks below because we are expecting
  // that MockClient::OnReadingUpdate() does _not_ get called.

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetXReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetXReturnCode(S_OK);
  reading->SetGetYReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetYReturnCode(S_OK);
  reading->SetGetZReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  task_environment_.RunUntilIdle();
}

// Tests if PlatformSensorReaderWinrtMagnetometer correctly converts sensor
// readings from Windows.Sensors.Devices.Magnetometer.
TEST_F(PlatformSensorReaderTestWinrt, CheckMagnetometerReadingConversion) {
  constexpr double expected_x = 112.0;
  constexpr double expected_y = 112.0;
  constexpr double expected_z = 457.0;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IMagnetometerStatics,
      ABI::Windows::Devices::Sensors::IMagnetometer,
      ABI::Windows::Devices::Sensors::Magnetometer,
      ABI::Windows::Devices::Sensors::IMagnetometerReading,
      ABI::Windows::Devices::Sensors::IMagnetometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::MagnetometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor =
      CreateAndInitializeSensor<PlatformSensorReaderWinrtMagnetometer>(
          fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeMagnetometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_x, expected_y, expected_z);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(expected_x, reading.magn.x);
          EXPECT_EQ(expected_y, reading.magn.y);
          EXPECT_EQ(expected_z, reading.magn.z);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtMagnetometer does not notify the
// client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedMagnetometerSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IMagnetometerStatics,
      ABI::Windows::Devices::Sensors::IMagnetometer,
      ABI::Windows::Devices::Sensors::Magnetometer,
      ABI::Windows::Devices::Sensors::IMagnetometerReading,
      ABI::Windows::Devices::Sensors::IMagnetometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::MagnetometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor =
      CreateAndInitializeSensor<PlatformSensorReaderWinrtMagnetometer>(
          fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeMagnetometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0, 0, 0);

  // We cannot use a base::RunLoop in the checks below because we are expecting
  // that MockClient::OnReadingUpdate() does _not_ get called.

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetXReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetXReturnCode(S_OK);
  reading->SetGetYReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetYReturnCode(S_OK);
  reading->SetGetZReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  task_environment_.RunUntilIdle();
}

// Tests if PlatformSensorReaderWinrtAbsOrientationEulerAngles correctly
// converts sensor readings from Windows.Devices.Sensors.Inclinometer.
TEST_F(PlatformSensorReaderTestWinrt, CheckInclinometerReadingConversion) {
  constexpr double expected_x = 10.0;
  constexpr double expected_y = 20.0;
  constexpr double expected_z = 30.0;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IInclinometerStatics,
      ABI::Windows::Devices::Sensors::IInclinometer,
      ABI::Windows::Devices::Sensors::Inclinometer,
      ABI::Windows::Devices::Sensors::IInclinometerReading,
      ABI::Windows::Devices::Sensors::IInclinometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::InclinometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<
      PlatformSensorReaderWinrtAbsOrientationEulerAngles>(fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeInclinometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_x, expected_y, expected_z);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(expected_x, reading.orientation_euler.x);
          EXPECT_EQ(expected_y, reading.orientation_euler.y);
          EXPECT_EQ(expected_z, reading.orientation_euler.z);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtAbsOrientationEulerAngles does not notify
// the client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedInclinometerSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IInclinometerStatics,
      ABI::Windows::Devices::Sensors::IInclinometer,
      ABI::Windows::Devices::Sensors::Inclinometer,
      ABI::Windows::Devices::Sensors::IInclinometerReading,
      ABI::Windows::Devices::Sensors::IInclinometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::InclinometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<
      PlatformSensorReaderWinrtAbsOrientationEulerAngles>(fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeInclinometerReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0, 0, 0);

  // We cannot use a base::RunLoop in the checks below because we are expecting
  // that MockClient::OnReadingUpdate() does _not_ get called.

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetXReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetXReturnCode(S_OK);
  reading->SetGetYReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetYReturnCode(S_OK);
  reading->SetGetZReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  task_environment_.RunUntilIdle();
}

// Tests if PlatformSensorReaderWinrtAbsOrientationQuaternion correctly
// converts sensor readings from Windows.Devices.Sensors.OrientationSensor.
TEST_F(PlatformSensorReaderTestWinrt, CheckOrientationSensorReadingConversion) {
  constexpr double expected_w = 11.0;
  constexpr double expected_x = 22.0;
  constexpr double expected_y = 33.0;
  constexpr double expected_z = 44.0;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IOrientationSensorStatics,
      ABI::Windows::Devices::Sensors::IOrientationSensor,
      ABI::Windows::Devices::Sensors::OrientationSensor,
      ABI::Windows::Devices::Sensors::IOrientationSensorReading,
      ABI::Windows::Devices::Sensors::IOrientationSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::
          OrientationSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<
      PlatformSensorReaderWinrtAbsOrientationQuaternion>(fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeOrientationSensorReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_w, expected_x, expected_y,
      expected_z);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
        .WillOnce(testing::Invoke([&](const SensorReading& reading) {
          EXPECT_EQ(expected_w, reading.orientation_quat.w);
          EXPECT_EQ(expected_x, reading.orientation_quat.x);
          EXPECT_EQ(expected_y, reading.orientation_quat.y);
          EXPECT_EQ(expected_z, reading.orientation_quat.z);
          run_loop.Quit();
        }));
    fake_sensor->TriggerFakeSensorReading(reading);
    run_loop.Run();
  }

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtAbsOrientationQuaternion does not notify
// the client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedOrientationSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IOrientationSensorStatics,
      ABI::Windows::Devices::Sensors::IOrientationSensor,
      ABI::Windows::Devices::Sensors::OrientationSensor,
      ABI::Windows::Devices::Sensors::IOrientationSensorReading,
      ABI::Windows::Devices::Sensors::IOrientationSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::
          OrientationSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<
      PlatformSensorReaderWinrtAbsOrientationQuaternion>(fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeOrientationSensorReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0, 0, 0, 0);

  // We cannot use a base::RunLoop in the checks below because we are expecting
  // that MockClient::OnReadingUpdate() does _not_ get called.

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetQuaternionReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  task_environment_.RunUntilIdle();
}

TEST_F(PlatformSensorReaderTestWinrt, LightSensorThresholding) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtLightSensor>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  float last_sent_lux = 1.0f;
  auto threshold_helper = [&](bool expect_callback) {
    auto reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
        ABI::Windows::Foundation::DateTime{}, last_sent_lux);
    if (expect_callback) {
      base::RunLoop run_loop;
      EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      fake_sensor->TriggerFakeSensorReading(reading);
      run_loop.Run();
    } else {
      fake_sensor->TriggerFakeSensorReading(reading);
      task_environment_.RunUntilIdle();
    }
  };

  // Expect callback, first sample
  threshold_helper(true);

  // No callback, threshold has not been met
  last_sent_lux +=
      PlatformSensorReaderWinrtLightSensor::kLuxPercentThreshold * 0.5f;
  threshold_helper(false);

  // Expect callback, threshold has been met since last reported sample
  last_sent_lux +=
      PlatformSensorReaderWinrtLightSensor::kLuxPercentThreshold * 0.6f;
  threshold_helper(true);

  sensor->StopSensor();
}

TEST_F(PlatformSensorReaderTestWinrt, AccelerometerThresholding) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IAccelerometerStatics,
      ABI::Windows::Devices::Sensors::IAccelerometer,
      ABI::Windows::Devices::Sensors::Accelerometer,
      ABI::Windows::Devices::Sensors::IAccelerometerReading,
      ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::AccelerometerReadingChangedEventArgs>>(
      Microsoft::WRL::Make<FakeAccelerometerSensorWinrt>());
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor =
      CreateAndInitializeSensor<PlatformSensorReaderWinrtAccelerometer>(
          fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  double last_sent_x = 1.0f;
  double last_sent_y = 2.0f;
  double last_sent_z = 3.0f;
  auto threshold_helper = [&](bool expect_callback) {
    auto reading = Microsoft::WRL::Make<FakeAccelerometerReadingWinrt>(
        ABI::Windows::Foundation::DateTime{}, last_sent_x, last_sent_y,
        last_sent_z);
    if (expect_callback) {
      base::RunLoop run_loop;
      EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      fake_sensor->TriggerFakeSensorReading(reading);
      run_loop.Run();
    } else {
      fake_sensor->TriggerFakeSensorReading(reading);
      task_environment_.RunUntilIdle();
    }
  };

  // Expect callback, first sample
  threshold_helper(true);

  // For each axis, increase its value by an amount lower than the threshold so
  // that no callback is invoked, then meet the threshold and do expect a
  // callback.
  last_sent_x += PlatformSensorReaderWinrtAccelerometer::kAxisThreshold * 0.5f;
  threshold_helper(false);
  last_sent_x += PlatformSensorReaderWinrtAccelerometer::kAxisThreshold * 0.6f;
  threshold_helper(true);
  last_sent_y += PlatformSensorReaderWinrtAccelerometer::kAxisThreshold * 0.5f;
  threshold_helper(false);
  last_sent_y += PlatformSensorReaderWinrtAccelerometer::kAxisThreshold * 0.6f;
  threshold_helper(true);
  last_sent_z += PlatformSensorReaderWinrtAccelerometer::kAxisThreshold * 0.5f;
  threshold_helper(false);
  last_sent_z += PlatformSensorReaderWinrtAccelerometer::kAxisThreshold * 0.6f;
  threshold_helper(true);

  sensor->StopSensor();
}

TEST_F(PlatformSensorReaderTestWinrt, GyrometerThresholding) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IGyrometerStatics,
      ABI::Windows::Devices::Sensors::IGyrometer,
      ABI::Windows::Devices::Sensors::Gyrometer,
      ABI::Windows::Devices::Sensors::IGyrometerReading,
      ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::GyrometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<PlatformSensorReaderWinrtGyrometer>(
      fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  double last_sent_x = 3.0f;
  double last_sent_y = 4.0f;
  double last_sent_z = 5.0f;
  auto threshold_helper = [&](bool expect_callback) {
    auto reading = Microsoft::WRL::Make<FakeGyrometerReadingWinrt>(
        ABI::Windows::Foundation::DateTime{}, last_sent_x, last_sent_y,
        last_sent_z);
    if (expect_callback) {
      base::RunLoop run_loop;
      EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      fake_sensor->TriggerFakeSensorReading(reading);
      run_loop.Run();
    } else {
      fake_sensor->TriggerFakeSensorReading(reading);
      task_environment_.RunUntilIdle();
    }
  };

  // Expect callback, first sample
  threshold_helper(true);

  // For each axis, increase its value by an amount lower than the threshold so
  // that no callback is invoked, then meet the threshold and do expect a
  // callback.
  last_sent_x += PlatformSensorReaderWinrtGyrometer::kDegreeThreshold * 0.5f;
  threshold_helper(false);
  last_sent_x += PlatformSensorReaderWinrtGyrometer::kDegreeThreshold * 0.6f;
  threshold_helper(true);
  last_sent_y += PlatformSensorReaderWinrtGyrometer::kDegreeThreshold * 0.5f;
  threshold_helper(false);
  last_sent_y += PlatformSensorReaderWinrtGyrometer::kDegreeThreshold * 0.6f;
  threshold_helper(true);
  last_sent_z += PlatformSensorReaderWinrtGyrometer::kDegreeThreshold * 0.5f;
  threshold_helper(false);
  last_sent_z += PlatformSensorReaderWinrtGyrometer::kDegreeThreshold * 0.6f;
  threshold_helper(true);

  sensor->StopSensor();
}

TEST_F(PlatformSensorReaderTestWinrt, MagnetometerThresholding) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IMagnetometerStatics,
      ABI::Windows::Devices::Sensors::IMagnetometer,
      ABI::Windows::Devices::Sensors::Magnetometer,
      ABI::Windows::Devices::Sensors::IMagnetometerReading,
      ABI::Windows::Devices::Sensors::IMagnetometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::MagnetometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor =
      CreateAndInitializeSensor<PlatformSensorReaderWinrtMagnetometer>(
          fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  double last_sent_x = 3.0f;
  double last_sent_y = 4.0f;
  double last_sent_z = 5.0f;
  auto threshold_helper = [&](bool expect_callback) {
    auto reading = Microsoft::WRL::Make<FakeMagnetometerReadingWinrt>(
        ABI::Windows::Foundation::DateTime{}, last_sent_x, last_sent_y,
        last_sent_z);
    if (expect_callback) {
      base::RunLoop run_loop;
      EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      fake_sensor->TriggerFakeSensorReading(reading);
      run_loop.Run();
    } else {
      fake_sensor->TriggerFakeSensorReading(reading);
      task_environment_.RunUntilIdle();
    }
  };

  // Expect callback, first sample
  threshold_helper(true);

  // For each axis, increase its value by an amount lower than the threshold so
  // that no callback is invoked, then meet the threshold and do expect a
  // callback.
  last_sent_x +=
      PlatformSensorReaderWinrtMagnetometer::kMicroteslaThreshold * 0.5f;
  threshold_helper(false);
  last_sent_x +=
      PlatformSensorReaderWinrtMagnetometer::kMicroteslaThreshold * 0.6f;
  threshold_helper(true);
  last_sent_y +=
      PlatformSensorReaderWinrtMagnetometer::kMicroteslaThreshold * 0.5f;
  threshold_helper(false);
  last_sent_y +=
      PlatformSensorReaderWinrtMagnetometer::kMicroteslaThreshold * 0.6f;
  threshold_helper(true);
  last_sent_z +=
      PlatformSensorReaderWinrtMagnetometer::kMicroteslaThreshold * 0.5f;
  threshold_helper(false);
  last_sent_z +=
      PlatformSensorReaderWinrtMagnetometer::kMicroteslaThreshold * 0.6f;
  threshold_helper(true);

  sensor->StopSensor();
}

TEST_F(PlatformSensorReaderTestWinrt, AbsOrientationEulerThresholding) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IInclinometerStatics,
      ABI::Windows::Devices::Sensors::IInclinometer,
      ABI::Windows::Devices::Sensors::Inclinometer,
      ABI::Windows::Devices::Sensors::IInclinometerReading,
      ABI::Windows::Devices::Sensors::IInclinometerReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::InclinometerReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<
      PlatformSensorReaderWinrtAbsOrientationEulerAngles>(fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  double last_sent_x = 3.0f;
  double last_sent_y = 4.0f;
  double last_sent_z = 5.0f;
  auto threshold_helper = [&](bool expect_callback) {
    auto reading = Microsoft::WRL::Make<FakeInclinometerReadingWinrt>(
        ABI::Windows::Foundation::DateTime{}, last_sent_x, last_sent_y,
        last_sent_z);
    if (expect_callback) {
      base::RunLoop run_loop;
      EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      fake_sensor->TriggerFakeSensorReading(reading);
      run_loop.Run();
    } else {
      fake_sensor->TriggerFakeSensorReading(reading);
      task_environment_.RunUntilIdle();
    }
  };

  // Expect callback, first sample
  threshold_helper(true);

  // For each axis, increase its value by an amount lower than the threshold so
  // that no callback is invoked, then meet the threshold and do expect a
  // callback.
  last_sent_x +=
      PlatformSensorReaderWinrtAbsOrientationEulerAngles::kDegreeThreshold *
      0.5f;
  threshold_helper(false);
  last_sent_x +=
      PlatformSensorReaderWinrtAbsOrientationEulerAngles::kDegreeThreshold *
      0.6f;
  threshold_helper(true);
  last_sent_y +=
      PlatformSensorReaderWinrtAbsOrientationEulerAngles::kDegreeThreshold *
      0.5f;
  threshold_helper(false);
  last_sent_y +=
      PlatformSensorReaderWinrtAbsOrientationEulerAngles::kDegreeThreshold *
      0.6f;
  threshold_helper(true);
  last_sent_z +=
      PlatformSensorReaderWinrtAbsOrientationEulerAngles::kDegreeThreshold *
      0.5f;
  threshold_helper(false);
  last_sent_z +=
      PlatformSensorReaderWinrtAbsOrientationEulerAngles::kDegreeThreshold *
      0.6f;
  threshold_helper(true);

  sensor->StopSensor();
}

TEST_F(PlatformSensorReaderTestWinrt, AbsOrientationQuatThresholding) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::IOrientationSensorStatics,
      ABI::Windows::Devices::Sensors::IOrientationSensor,
      ABI::Windows::Devices::Sensors::OrientationSensor,
      ABI::Windows::Devices::Sensors::IOrientationSensorReading,
      ABI::Windows::Devices::Sensors::IOrientationSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::
          OrientationSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = CreateAndInitializeSensor<
      PlatformSensorReaderWinrtAbsOrientationQuaternion>(fake_sensor_factory);
  ASSERT_TRUE(sensor);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  double last_sent_rad = 1.0;
  auto threshold_helper = [&](bool expect_callback) {
    auto quat = gfx::Quaternion(gfx::Vector3dF(1.0, 0, 0), last_sent_rad);
    auto reading = Microsoft::WRL::Make<FakeOrientationSensorReadingWinrt>(
        ABI::Windows::Foundation::DateTime{}, quat.w(), quat.x(), quat.y(),
        quat.z());
    if (expect_callback) {
      base::RunLoop run_loop;
      EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      fake_sensor->TriggerFakeSensorReading(reading);
      run_loop.Run();
    } else {
      fake_sensor->TriggerFakeSensorReading(reading);
      task_environment_.RunUntilIdle();
    }
  };

  // Expect callback, first sample
  threshold_helper(true);

  // No callback, threshold has not been met
  last_sent_rad +=
      PlatformSensorReaderWinrtAbsOrientationQuaternion::kRadianThreshold *
      0.5f;
  threshold_helper(false);

  // Expect callback, threshold has been met since last reported sample
  last_sent_rad +=
      PlatformSensorReaderWinrtAbsOrientationQuaternion::kRadianThreshold *
      0.6f;
  threshold_helper(true);

  sensor->StopSensor();
}

}  // namespace device
