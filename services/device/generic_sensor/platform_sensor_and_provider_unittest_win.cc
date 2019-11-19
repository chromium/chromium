// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <SensorsApi.h>
#include <sensors.h>
#include <wrl/implements.h>

#include "base/bind.h"
#include "base/metrics/statistics_recorder.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/win/propvarutil.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_propvariant.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/platform_sensor_provider_win.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/angle_conversions.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::WithArgs;

namespace device {

using mojom::SensorType;

// Mock class for ISensorManager COM interface.
class MockISensorManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISensorManager> {
 public:
  // ISensorManager interface
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorsByCategory,
                             HRESULT(REFSENSOR_CATEGORY_ID category,
                                     ISensorCollection** sensors_found));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorsByType,
                             HRESULT(REFSENSOR_TYPE_ID sensor_id,
                                     ISensorCollection** sensors_found));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorByID,
                             HRESULT(REFSENSOR_ID sensor_id, ISensor** sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetEventSink,
                             HRESULT(ISensorManagerEvents* event_sink));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             RequestPermissions,
                             HRESULT(HWND parent,
                                     ISensorCollection* sensors,
                                     BOOL is_modal));

 protected:
  ~MockISensorManager() override = default;
};

// Mock class for ISensorCollection COM interface.
class MockISensorCollection
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISensorCollection> {
 public:
  // ISensorCollection interface
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetAt,
                             HRESULT(ULONG index, ISensor** sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetCount,
                             HRESULT(ULONG* count));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, Add, HRESULT(ISensor* sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Remove,
                             HRESULT(ISensor* sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             RemoveByID,
                             HRESULT(REFSENSOR_ID sensor_id));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Clear, HRESULT());

 protected:
  ~MockISensorCollection() override = default;
};

// Mock class for ISensor COM interface.
class MockISensor
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISensor> {
 public:
  // ISensor interface
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, GetID, HRESULT(SENSOR_ID* id));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetCategory,
                             HRESULT(SENSOR_CATEGORY_ID* category));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetType,
                             HRESULT(SENSOR_TYPE_ID* type));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetFriendlyName,
                             HRESULT(BSTR* name));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetProperty,
                             HRESULT(REFPROPERTYKEY key,
                                     PROPVARIANT* property));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetProperties,
                             HRESULT(IPortableDeviceKeyCollection* keys,
                                     IPortableDeviceValues** properties));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSupportedDataFields,
                             HRESULT(IPortableDeviceKeyCollection** data));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetProperties,
                             HRESULT(IPortableDeviceValues* properties,
                                     IPortableDeviceValues** results));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SupportsDataField,
                             HRESULT(REFPROPERTYKEY key,
                                     VARIANT_BOOL* is_supported));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetState,
                             HRESULT(SensorState* state));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetData,
                             HRESULT(ISensorDataReport** data_report));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SupportsEvent,
                             HRESULT(REFGUID event_guid,
                                     VARIANT_BOOL* is_supported));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetEventInterest,
                             HRESULT(GUID** values, ULONG* count));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetEventInterest,
                             HRESULT(GUID* values, ULONG count));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetEventSink,
                             HRESULT(ISensorEvents* pEvents));

 protected:
  ~MockISensor() override = default;
};

// Mock class for ISensorDataReport COM interface.
class MockISensorDataReport
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISensorDataReport> {
 public:
  // ISensorDataReport interface
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTimestamp,
                             HRESULT(SYSTEMTIME* timestamp));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorValue,
                             HRESULT(REFPROPERTYKEY key, PROPVARIANT* value));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorValues,
                             HRESULT(IPortableDeviceKeyCollection* keys,
                                     IPortableDeviceValues** values));

 protected:
  ~MockISensorDataReport() override = default;
};

// Class that provides test harness support for generic sensor adaptation for
// Windows platform. Testing is mainly done by mocking main COM interfaces that
// are used to communicate with Sensors API.
// MockISensorManager    - mocks ISensorManager and responsible for fetching
//                         list of supported sensors.
// MockISensorCollection - mocks collection of ISensor objects.
// MockISensor           - mocks ISensor intrface.
// MockISensorDataReport - mocks IDataReport interface that is used to deliver
//                         data in OnDataUpdated event.
class PlatformSensorAndProviderTestWin : public ::testing::Test {
 public:
  PlatformSensorAndProviderTestWin()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    sensor_ = Microsoft::WRL::Make<NiceMock<MockISensor>>();
    sensor_collection_ =
        Microsoft::WRL::Make<NiceMock<MockISensorCollection>>();
    sensor_manager_ = Microsoft::WRL::Make<NiceMock<MockISensorManager>>();
    Microsoft::WRL::ComPtr<ISensorManager> manager;
    sensor_manager_->QueryInterface(IID_PPV_ARGS(&manager));

    // Overrides default ISensorManager with mocked interface.
    provider_ = std::make_unique<PlatformSensorProviderWin>();
    provider_->SetSensorManagerForTesting(std::move(manager));
  }

 protected:
  void SensorCreated(scoped_refptr<PlatformSensor> sensor) {
    platform_sensor_ = sensor;
    run_loop_->Quit();
  }

  // Sensor creation is asynchronous, therefore inner loop is used to wait for
  // PlatformSensorProvider::CreateSensorCallback completion.
  scoped_refptr<PlatformSensor> CreateSensor(mojom::SensorType type) {
    run_loop_ = std::make_unique<base::RunLoop>();
    provider_->CreateSensor(
        type, base::Bind(&PlatformSensorAndProviderTestWin::SensorCreated,
                         base::Unretained(this)));
    run_loop_->Run();
    scoped_refptr<PlatformSensor> sensor;
    sensor.swap(platform_sensor_);
    run_loop_ = nullptr;
    return sensor;
  }

  // Listening the sensor is asynchronous, therefore inner loop is used to wait
  // for SetEventSink to be called.
  bool StartListening(scoped_refptr<PlatformSensor> sensor,
                      PlatformSensor::Client* client,
                      const PlatformSensorConfiguration& config) {
    run_loop_ = std::make_unique<base::RunLoop>();
    bool ret = sensor->StartListening(client, config);
    if (ret)
      run_loop_->Run();
    run_loop_ = nullptr;
    return ret;
  }

  void QuitInnerLoop() { run_loop_->Quit(); }

  void SetUnsupportedSensor(REFSENSOR_TYPE_ID sensor) {
    EXPECT_CALL(*(sensor_manager_.Get()), GetSensorsByType(sensor, _))
        .WillRepeatedly(
            Invoke([](REFSENSOR_TYPE_ID type, ISensorCollection** collection) {
              return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            }));
  }

  // Sets sensor with REFSENSOR_TYPE_ID |sensor| to be supported by mocked
  // ISensorMager and it will be present in ISensorCollection.
  void SetSupportedSensor(REFSENSOR_TYPE_ID sensor) {
    // Returns mock ISensorCollection.
    EXPECT_CALL(*(sensor_manager_.Get()), GetSensorsByType(sensor, _))
        .WillOnce(Invoke(
            [this](REFSENSOR_TYPE_ID type, ISensorCollection** collection) {
              sensor_collection_->QueryInterface(
                  __uuidof(ISensorCollection),
                  reinterpret_cast<void**>(collection));
              return S_OK;
            }));

    // Returns number of ISensor objects in ISensorCollection, at the moment
    // only one ISensor interface instance is suported.
    EXPECT_CALL(*(sensor_collection_.Get()), GetCount(_))
        .WillOnce(Invoke([](ULONG* count) {
          *count = 1;
          return S_OK;
        }));

    // Returns ISensor interface instance at index 0.
    EXPECT_CALL(*(sensor_collection_.Get()), GetAt(0, _))
        .WillOnce(Invoke([this](ULONG index, ISensor** sensor) {
          sensor_->QueryInterface(__uuidof(ISensor),
                                  reinterpret_cast<void**>(sensor));
          return S_OK;
        }));

    // Handles |SetEventSink| call that is used to subscribe to sensor events
    // through ISensorEvents interface. ISensorEvents is stored and attached to
    // |sensor_events_| that is used later to generate fake error, state and
    // data change events.
    ON_CALL(*(sensor_.Get()), SetEventSink(NotNull()))
        .WillByDefault(Invoke([this](ISensorEvents* events) {
          events->AddRef();
          sensor_events_.Attach(events);
          if (this->run_loop_) {
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE,
                base::BindOnce(&PlatformSensorAndProviderTestWin::QuitInnerLoop,
                               base::Unretained(this)));
          }
          return S_OK;
        }));

    // When |SetEventSink| is called with nullptr, it means that client is no
    // longer interested in sensor events and ISensorEvents can be released.
    ON_CALL(*(sensor_.Get()), SetEventSink(IsNull()))
        .WillByDefault(Invoke([this](ISensorEvents* events) {
          sensor_events_.Reset();
          if (this->run_loop_) {
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE,
                base::BindOnce(&PlatformSensorAndProviderTestWin::QuitInnerLoop,
                               base::Unretained(this)));
          }
          return S_OK;
        }));
  }

  // Sets minimal reporting frequency for the mock sensor.
  void SetSupportedReportingFrequency(int frequency) {
    ON_CALL(*(sensor_.Get()),
            GetProperty(SENSOR_PROPERTY_MIN_REPORT_INTERVAL, _))
        .WillByDefault(
            Invoke([frequency](REFPROPERTYKEY key, PROPVARIANT* pProperty) {
              pProperty->vt = VT_UI4;
              pProperty->ulVal = 0;
              if (frequency != 0) {
                pProperty->ulVal =
                    (1.0 / frequency) * base::Time::kMillisecondsPerSecond;
              }
              return S_OK;
            }));
  }

  // Generates OnLeave event, e.g. when sensor is disconnected.
  void GenerateLeaveEvent() {
    if (!sensor_events_)
      return;
    sensor_events_->OnLeave(SENSOR_ID());
  }

  // Generates OnStateChangedLeave event.
  void GenerateStateChangeEvent(SensorState state) {
    if (!sensor_events_)
      return;
    sensor_events_->OnStateChanged(sensor_.Get(), state);
  }

  struct PropertyKeyCompare {
    bool operator()(REFPROPERTYKEY a, REFPROPERTYKEY b) const {
      if (a.fmtid == b.fmtid)
        return a.pid < b.pid;
      return false;
    }
  };

  using SensorData =
      std::map<PROPERTYKEY, const PROPVARIANT*, PropertyKeyCompare>;

  // Generates OnDataUpdated event and creates ISensorDataReport with fake
  // |value| for property with |key|.
  void GenerateDataUpdatedEvent(const SensorData& values) {
    if (!sensor_events_)
      return;

    auto mock_report = Microsoft::WRL::Make<NiceMock<MockISensorDataReport>>();
    Microsoft::WRL::ComPtr<ISensorDataReport> data_report;
    mock_report.As(&data_report);

    EXPECT_CALL(*(mock_report.Get()), GetTimestamp(_))
        .WillOnce(Invoke([](SYSTEMTIME* timestamp) {
          GetSystemTime(timestamp);
          return S_OK;
        }));

    EXPECT_CALL(*(mock_report.Get()), GetSensorValue(_, _))
        .WillRepeatedly(WithArgs<0, 1>(
            Invoke([&values](REFPROPERTYKEY key, PROPVARIANT* variant) {
              auto it = values.find(key);
              if (it == values.end())
                return E_FAIL;

              PropVariantCopy(variant, it->second);
              return S_OK;
            })));

    sensor_events_->OnDataUpdated(sensor_.Get(), data_report.Get());
  }

  base::win::ScopedCOMInitializer com_initializer_;
  base::test::TaskEnvironment task_environment_;
  Microsoft::WRL::ComPtr<MockISensorManager> sensor_manager_;
  Microsoft::WRL::ComPtr<MockISensorCollection> sensor_collection_;
  Microsoft::WRL::ComPtr<MockISensor> sensor_;
  std::unique_ptr<PlatformSensorProviderWin> provider_;
  Microsoft::WRL::ComPtr<ISensorEvents> sensor_events_;
  scoped_refptr<PlatformSensor> platform_sensor_;
  // Inner run loop used to wait for async sensor creation callback.
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Tests that PlatformSensorManager returns null sensor when sensor
// is not implemented.
TEST_F(PlatformSensorAndProviderTestWin, SensorIsNotImplemented) {
  EXPECT_CALL(*(sensor_manager_.Get()),
              GetSensorsByType(SENSOR_TYPE_PRESSURE, _))
      .Times(0);
  EXPECT_FALSE(CreateSensor(SensorType::PRESSURE));
}

// Tests that PlatformSensorManager returns null sensor when sensor
// is implemented, but not supported by the hardware.
TEST_F(PlatformSensorAndProviderTestWin, SensorIsNotSupported) {
  EXPECT_CALL(*(sensor_manager_.Get()),
              GetSensorsByType(SENSOR_TYPE_AMBIENT_LIGHT, _))
      .WillOnce(Invoke([](REFSENSOR_TYPE_ID, ISensorCollection** result) {
        *result = nullptr;
        return E_FAIL;
      }));

  EXPECT_FALSE(CreateSensor(SensorType::AMBIENT_LIGHT));
}

// Tests that PlatformSensorManager returns correct sensor when sensor
// is supported by the hardware.
TEST_F(PlatformSensorAndProviderTestWin, SensorIsSupported) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor->GetType());
}

// Tests that PlatformSensor::StartListening fails when provided reporting
// frequency is above hardware capabilities.
TEST_F(PlatformSensorAndProviderTestWin, StartFails) {
  SetSupportedReportingFrequency(1);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_FALSE(sensor->StartListening(client.get(), configuration));
}

// Tests that PlatformSensor::StartListening succeeds and notification about
// modified sensor reading is sent to the PlatformSensor::Client interface.
TEST_F(PlatformSensorAndProviderTestWin, SensorStarted) {
  SetSupportedReportingFrequency(10);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);

  EXPECT_CALL(*(sensor_.Get()), SetEventSink(NotNull())).Times(1);
  EXPECT_CALL(*(sensor_.Get()), SetEventSink(IsNull())).Times(1);
  EXPECT_CALL(*(sensor_.Get()), SetProperties(NotNull(), _))
      .WillRepeatedly(Invoke(
          [](IPortableDeviceValues* props, IPortableDeviceValues** result) {
            ULONG value = 0;
            HRESULT hr = props->GetUnsignedIntegerValue(
                SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL, &value);
            EXPECT_TRUE(SUCCEEDED(hr));
            // 10Hz is 100msec
            EXPECT_THAT(value, 100);
            return hr;
          }));

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));

  EXPECT_CALL(*client, OnSensorReadingChanged(sensor->GetType())).Times(1);
  base::win::ScopedPropVariant pvLux;
  InitPropVariantFromDouble(3.14, pvLux.Receive());
  GenerateDataUpdatedEvent({{SENSOR_DATA_TYPE_LIGHT_LEVEL_LUX, pvLux.ptr()}});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that OnSensorError is called when sensor is disconnected.
TEST_F(PlatformSensorAndProviderTestWin, SensorRemoved) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorError()).Times(1);

  GenerateLeaveEvent();
  base::RunLoop().RunUntilIdle();
}

// Tests that OnSensorError is called when sensor is in an error state.
TEST_F(PlatformSensorAndProviderTestWin, SensorStateChangedToError) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorError()).Times(1);

  GenerateStateChangeEvent(SENSOR_STATE_ERROR);
  base::RunLoop().RunUntilIdle();
}

// Tests that OnSensorError is not called when sensor is in a ready state.
TEST_F(PlatformSensorAndProviderTestWin, SensorStateChangedToReady) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorError()).Times(0);

  GenerateStateChangeEvent(SENSOR_STATE_READY);
  base::RunLoop().RunUntilIdle();
}

// Tests that GetMaximumSupportedFrequency provides correct value.
TEST_F(PlatformSensorAndProviderTestWin, GetMaximumSupportedFrequency) {
  SetSupportedReportingFrequency(20);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(), 20);
}

// Tests that GetMaximumSupportedFrequency returns fallback value.
TEST_F(PlatformSensorAndProviderTestWin, GetMaximumSupportedFrequencyFallback) {
  SetSupportedReportingFrequency(0);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(), 5);
}

// Tests that Accelerometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderTestWin, CheckAccelerometerReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::ACCELEROMETER));

  SetSupportedSensor(SENSOR_TYPE_ACCELEROMETER_3D);
  auto sensor = CreateSensor(SensorType::ACCELEROMETER);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged(sensor->GetType())).Times(1);

  double x_accel = 0.25;
  double y_accel = -0.25;
  double z_accel = -0.5;

  base::win::ScopedPropVariant pvX, pvY, pvZ;
  InitPropVariantFromDouble(x_accel, pvX.Receive());
  InitPropVariantFromDouble(y_accel, pvY.Receive());
  InitPropVariantFromDouble(z_accel, pvZ.Receive());

  GenerateDataUpdatedEvent({{SENSOR_DATA_TYPE_ACCELERATION_X_G, pvX.ptr()},
                            {SENSOR_DATA_TYPE_ACCELERATION_Y_G, pvY.ptr()},
                            {SENSOR_DATA_TYPE_ACCELERATION_Z_G, pvZ.ptr()}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.accel.x, -x_accel * base::kMeanGravityDouble);
  EXPECT_THAT(buffer->reading.accel.y, -y_accel * base::kMeanGravityDouble);
  EXPECT_THAT(buffer->reading.accel.z, -z_accel * base::kMeanGravityDouble);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Gyroscope readings are correctly converted.
TEST_F(PlatformSensorAndProviderTestWin, CheckGyroscopeReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::GYROSCOPE));

  SetSupportedSensor(SENSOR_TYPE_GYROMETER_3D);
  auto sensor = CreateSensor(SensorType::GYROSCOPE);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged(sensor->GetType())).Times(1);

  double x_ang_accel = 0.0;
  double y_ang_accel = -1.8;
  double z_ang_accel = -98.7;

  base::win::ScopedPropVariant pvX, pvY, pvZ;
  InitPropVariantFromDouble(x_ang_accel, pvX.Receive());
  InitPropVariantFromDouble(y_ang_accel, pvY.Receive());
  InitPropVariantFromDouble(z_ang_accel, pvZ.Receive());

  GenerateDataUpdatedEvent(
      {{SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND, pvX.ptr()},
       {SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND, pvY.ptr()},
       {SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND, pvZ.ptr()}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.gyro.x, gfx::DegToRad(x_ang_accel));
  EXPECT_THAT(buffer->reading.gyro.y, gfx::DegToRad(y_ang_accel));
  EXPECT_THAT(buffer->reading.gyro.z, gfx::DegToRad(z_ang_accel));
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Magnetometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderTestWin, CheckMagnetometerReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::MAGNETOMETER));

  SetSupportedSensor(SENSOR_TYPE_COMPASS_3D);
  auto sensor = CreateSensor(SensorType::MAGNETOMETER);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged(sensor->GetType())).Times(1);

  double x_magn_field = 112.0;
  double y_magn_field = -162.0;
  double z_magn_field = 457.0;

  base::win::ScopedPropVariant pvX, pvY, pvZ;
  InitPropVariantFromDouble(x_magn_field, pvX.Receive());
  InitPropVariantFromDouble(y_magn_field, pvY.Receive());
  InitPropVariantFromDouble(z_magn_field, pvZ.Receive());

  GenerateDataUpdatedEvent(
      {{SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_X_MILLIGAUSS, pvX.ptr()},
       {SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Y_MILLIGAUSS, pvY.ptr()},
       {SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Z_MILLIGAUSS, pvZ.ptr()}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.magn.x, x_magn_field * kMicroteslaInMilligauss);
  EXPECT_THAT(buffer->reading.magn.y, y_magn_field * kMicroteslaInMilligauss);
  EXPECT_THAT(buffer->reading.magn.z, z_magn_field * kMicroteslaInMilligauss);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that AbsoluteOrientationEulerAngles sensor readings are correctly
// provided.
TEST_F(PlatformSensorAndProviderTestWin,
       CheckDeviceOrientationEulerAnglesReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping =
      handle->MapAtOffset(sizeof(SensorReadingSharedBuffer),
                          SensorReadingSharedBuffer::GetOffset(
                              SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));

  SetSupportedSensor(SENSOR_TYPE_INCLINOMETER_3D);
  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged(sensor->GetType())).Times(1);

  double x = 10;
  double y = 20;
  double z = 30;

  base::win::ScopedPropVariant pvX, pvY, pvZ;
  InitPropVariantFromDouble(x, pvX.Receive());
  InitPropVariantFromDouble(y, pvY.Receive());
  InitPropVariantFromDouble(z, pvZ.Receive());

  GenerateDataUpdatedEvent({{SENSOR_DATA_TYPE_TILT_X_DEGREES, pvX.ptr()},
                            {SENSOR_DATA_TYPE_TILT_Y_DEGREES, pvY.ptr()},
                            {SENSOR_DATA_TYPE_TILT_Z_DEGREES, pvZ.ptr()}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());

  EXPECT_THAT(buffer->reading.orientation_euler.x, x);
  EXPECT_THAT(buffer->reading.orientation_euler.y, y);
  EXPECT_THAT(buffer->reading.orientation_euler.z, z);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that AbsoluteOrientationQuaternion sensor readings are correctly
// provided.
TEST_F(PlatformSensorAndProviderTestWin,
       CheckDeviceOrientationQuaternionReadingConversion) {
  mojo::ScopedSharedBufferHandle handle = provider_->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping =
      handle->MapAtOffset(sizeof(SensorReadingSharedBuffer),
                          SensorReadingSharedBuffer::GetOffset(
                              SensorType::ABSOLUTE_ORIENTATION_QUATERNION));

  SetSupportedSensor(SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION);
  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
  EXPECT_TRUE(sensor);

  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(StartListening(sensor, client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged(sensor->GetType())).Times(1);

  double x = -0.5;
  double y = -0.5;
  double z = 0.5;
  double w = 0.5;
  float quat_elements[4] = {x, y, z, w};

  base::win::ScopedPropVariant pvQuat;

  // The SENSOR_DATA_TYPE_QUATERNION property has [VT_VECTOR | VT_UI1] type.
  // https://msdn.microsoft.com/en-us/library/windows/hardware/dn265187(v=vs.85).aspx
  // Helper functions e.g., InitVariantFromDoubleArray cannot be used for its
  // intialization and the only way to initialize it, is to use
  // InitPropVariantFromGUIDAsBuffer with quaternion format GUID.
  InitPropVariantFromGUIDAsBuffer(SENSOR_DATA_TYPE_QUATERNION.fmtid,
                                  pvQuat.Receive());
  memcpy(pvQuat.get().caub.pElems, &quat_elements, sizeof(quat_elements));
  GenerateDataUpdatedEvent({{SENSOR_DATA_TYPE_QUATERNION, pvQuat.ptr()}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());

  EXPECT_THAT(buffer->reading.orientation_quat.x, x);
  EXPECT_THAT(buffer->reading.orientation_quat.y, y);
  EXPECT_THAT(buffer->reading.orientation_quat.z, z);
  EXPECT_THAT(buffer->reading.orientation_quat.w, w);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that when only the quaternion version of the absolute orientation
// sensor is available the provider falls back to using a fusion algorithm
// to provide the euler angles version.
TEST_F(PlatformSensorAndProviderTestWin,
       CheckDeviceOrientationEulerAnglesFallback) {
  SetUnsupportedSensor(SENSOR_TYPE_INCLINOMETER_3D);
  SetSupportedSensor(SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION);

  auto sensor = CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
  EXPECT_TRUE(sensor);
}

// Tests that with neither absolute orientation sensor type available
// the fallback logic does not generate an infinite loop.
TEST_F(PlatformSensorAndProviderTestWin,
       CheckDeviceOrientationFallbackFailure) {
  SetUnsupportedSensor(SENSOR_TYPE_INCLINOMETER_3D);
  SetUnsupportedSensor(SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION);

  auto euler_angles_sensor =
      CreateSensor(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
  EXPECT_FALSE(euler_angles_sensor);
  auto quaternion_sensor =
      CreateSensor(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
  EXPECT_FALSE(quaternion_sensor);
}

// Tests the sensor activation histogram tracks sensor activation return
// codes correctly.
TEST_F(PlatformSensorAndProviderTestWin, CheckSensorActivationHistogram) {
  base::HistogramTester histogram_tester;

  // Trigger ERROR_NOT_FOUND
  SetUnsupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_FALSE(sensor);
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Sensors.Windows.ISensor.Activation.Result",
                HRESULT_FROM_WIN32(ERROR_NOT_FOUND)),
            1);

  // Trigger S_OK
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Sensors.Windows.ISensor.Activation.Result", S_OK),
            1);

  histogram_tester.ExpectTotalCount("Sensors.Windows.ISensor.Activation.Result",
                                    2);
}

// Tests the sensor start histogram tracks sensor start return codes
// correctly.
TEST_F(PlatformSensorAndProviderTestWin, CheckSensorStartHistogram) {
  base::HistogramTester histogram_tester;

  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);

  // Trigger S_OK
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
  base::Optional<base::RunLoop> run_loop;
  run_loop.emplace();
  provider_->GetComStaTaskRunnerForTesting()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), run_loop->QuitClosure());
  run_loop->Run();
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Sensors.Windows.ISensor.Start.Result", S_OK),
            1);

  // Trigger E_OUTOFMEMORY
  ON_CALL(*(sensor_.Get()), SetEventSink(NotNull()))
      .WillByDefault(Invoke([](ISensorEvents*) { return E_OUTOFMEMORY; }));

  // StartListening() swallows SetEventSink() errors so this will return
  // true even if the sensor failed to start.
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
  run_loop.emplace();
  provider_->GetComStaTaskRunnerForTesting()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), run_loop->QuitClosure());
  run_loop->Run();
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Sensors.Windows.ISensor.Start.Result", E_OUTOFMEMORY),
            1);

  histogram_tester.ExpectTotalCount("Sensors.Windows.ISensor.Start.Result", 2);
}

// Tests the sensor stop histogram tracks sensor stop return codes
// correctly.
TEST_F(PlatformSensorAndProviderTestWin, CheckSensorStopHistogram) {
  base::HistogramTester histogram_tester;

  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  auto client = std::make_unique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);

  // Trigger S_OK
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Sensors.Windows.ISensor.Stop.Result", S_OK),
            1);

  // Trigger E_POINTER
  ON_CALL(*(sensor_.Get()), SetEventSink(IsNull()))
      .WillByDefault(Invoke([&](ISensorEvents*) { return E_POINTER; }));

  // StopListening() swallows SetEventSink() errors so this will return
  // true even if the sensor failed to start.
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Sensors.Windows.ISensor.Stop.Result", E_POINTER),
            1);

  histogram_tester.ExpectTotalCount("Sensors.Windows.ISensor.Stop.Result", 2);
}

}  // namespace device
