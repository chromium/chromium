// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service.h"
#include "services/device/device_service_test_base.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/sensor_provider_impl.h"
#include "services/device/generic_sensor/virtual_platform_sensor.h"
#include "services/device/generic_sensor/virtual_platform_sensor_provider.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

using ::testing::_;
using ::testing::Invoke;

namespace device {

using mojom::SensorType;

namespace {

// These values match what FakePlatformSensor reports and are somewhat
// arbitrary.
constexpr double kMinimumPlatformFrequency = 1.0;
constexpr double kMaximumPlatformFrequency = 50.0;

class TestSensorClient : public mojom::SensorClient {
 public:
  TestSensorClient(SensorType type) : type_(type) {}

  TestSensorClient(const TestSensorClient&) = delete;
  TestSensorClient& operator=(const TestSensorClient&) = delete;

  bool FetchSensorReading(SensorReading* out_reading) {
    return shared_buffer_reader_->GetReading(out_reading);
  }

  // Implements mojom::SensorClient:
  void SensorReadingChanged() override {
    if (!shared_buffer_reader_->GetReading(&reading_data_)) {
      ADD_FAILURE() << "Failed to get readings from shared buffer";
      return;
    }
    if (on_reading_changed_callback_) {
      switch (type_) {
        case SensorType::AMBIENT_LIGHT:
          std::move(on_reading_changed_callback_).Run(reading_data_.als.value);
          break;
        case SensorType::ACCELEROMETER:
          std::move(on_reading_changed_callback_).Run(reading_data_.accel.x);
          break;
        default:
          NOTREACHED_NORETURN() << "Unsupported sensor type in test " << type_;
      }
    }
  }
  void RaiseError() override {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
  }

  double WaitForReading() {
    base::test::TestFuture<double> future;
    SetOnReadingChangedCallback(future.GetCallback());
    return future.Get();
  }

  bool AddConfigurationSync(const PlatformSensorConfiguration& configuration) {
    base::test::TestFuture<bool> future;
    sensor()->AddConfiguration(configuration, future.GetCallback());
    return future.Get();
  }

  // Sensor mojo interfaces callbacks:
  void OnSensorCreated(base::OnceClosure quit_closure,
                       mojom::SensorCreationResult result,
                       mojom::SensorInitParamsPtr params) {
    ASSERT_TRUE(params);
    EXPECT_EQ(mojom::SensorCreationResult::SUCCESS, result);
    EXPECT_TRUE(params->memory.IsValid());

    const double expected_maximum_frequency = std::min(
        kMaximumPlatformFrequency, GetSensorMaxAllowedFrequency(type_));
    EXPECT_DOUBLE_EQ(expected_maximum_frequency, params->maximum_frequency);
    EXPECT_DOUBLE_EQ(kMinimumPlatformFrequency, params->minimum_frequency);

    const double expected_default_frequency =
        std::clamp(params->default_configuration.frequency(),
                   kMinimumPlatformFrequency, expected_maximum_frequency);
    EXPECT_DOUBLE_EQ(expected_default_frequency,
                     params->default_configuration.frequency());

    shared_buffer_reader_ = device::SensorReadingSharedBufferReader::Create(
        std::move(params->memory), params->buffer_offset);
    ASSERT_TRUE(shared_buffer_reader_);

    sensor_.Bind(std::move(params->sensor));
    client_receiver_.Bind(std::move(params->client_receiver));
    std::move(quit_closure).Run();
  }

  // For SensorReadingChanged().
  void SetOnReadingChangedCallback(base::OnceCallback<void(double)> callback) {
    on_reading_changed_callback_ = std::move(callback);
  }

  // For RaiseError().
  void SetOnErrorCallback(base::OnceClosure callback) {
    on_error_callback_ = std::move(callback);
  }

  mojom::Sensor* sensor() { return sensor_.get(); }
  void ResetSensor() { sensor_.reset(); }

 private:
  mojo::Remote<mojom::Sensor> sensor_;
  mojo::Receiver<mojom::SensorClient> client_receiver_{this};
  std::unique_ptr<device::SensorReadingSharedBufferReader>
      shared_buffer_reader_;
  SensorReading reading_data_;

  // |on_reading_changed_callback_| is called to verify the data is same as we
  // expected in SensorReadingChanged().
  base::OnceCallback<void(double)> on_reading_changed_callback_;

  base::OnceClosure on_error_callback_;

  SensorType type_;
};

}  //  namespace

class GenericSensorServiceTest : public DeviceServiceTestBase {
 public:
  GenericSensorServiceTest() = default;

  GenericSensorServiceTest(const GenericSensorServiceTest&) = delete;
  GenericSensorServiceTest& operator=(const GenericSensorServiceTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kGenericSensorExtraClasses}, {});
    DeviceServiceTestBase::SetUp();

    fake_platform_sensor_provider_ = new FakePlatformSensorProvider();
    sensor_provider_impl_ = new SensorProviderImpl(
        base::WrapUnique(fake_platform_sensor_provider_.get()));
    device_service_impl()->SetSensorProviderImplForTesting(
        base::WrapUnique(sensor_provider_impl_.get()));
    device_service()->BindSensorProvider(
        sensor_provider_.BindNewPipeAndPassReceiver());
  }

 protected:
  mojom::CreateVirtualSensorResult CreateVirtualSensorSync(SensorType type) {
    auto metadata = mojom::VirtualSensorMetadata::New();
    metadata->maximum_frequency = kMaximumPlatformFrequency;
    metadata->minimum_frequency = kMinimumPlatformFrequency;

    // Make all sensor types have the same reporting mode for the tests to work
    // as expected here (i.e. TestSensorClient::SensorReadingChanged() works
    // reliably).
    metadata->reporting_mode = mojom::ReportingMode::ON_CHANGE;

    base::test::TestFuture<mojom::CreateVirtualSensorResult> future;
    sensor_provider_->CreateVirtualSensor(type, std::move(metadata),
                                          future.GetCallback());
    return future.Get();
  }

  mojom::VirtualSensorInformationPtr GetVirtualSensorInformationSync(
      SensorType type) {
    base::test::TestFuture<mojom::GetVirtualSensorInformationResultPtr>
        info_future;
    sensor_provider_->GetVirtualSensorInformation(type,
                                                  info_future.GetCallback());
    EXPECT_EQ(info_future.Get()->which(),
              mojom::GetVirtualSensorInformationResult::Tag::kInfo);
    return std::move(info_future.Take()->get_info());
  }

  mojom::UpdateVirtualSensorResult UpdateVirtualSensorSync(
      SensorType type,
      double single_value) {
    // SensorReading is a union, so we can always use reading.als here
    // regardless of |type|.
    SensorReading reading;
    switch (type) {
      case SensorType::AMBIENT_LIGHT:
        reading.als.value = single_value;
        break;
      case SensorType::ACCELEROMETER:
        reading.accel.x = single_value;
        break;
      default:
        NOTREACHED_NORETURN() << "Unsupported sensor type in test " << type;
    }

    base::test::TestFuture<mojom::UpdateVirtualSensorResult> future;
    sensor_provider_->UpdateVirtualSensor(type, reading, future.GetCallback());
    return future.Get();
  }

  void AddReadingWithFrequency(SensorType type) {
    const auto sensor_info = GetVirtualSensorInformationSync(type);
    ASSERT_TRUE(sensor_info);
    EXPECT_EQ(UpdateVirtualSensorSync(type, sensor_info->sampling_frequency),
              mojom::UpdateVirtualSensorResult::kSuccess);
  }

  mojo::Remote<mojom::SensorProvider> sensor_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<FakePlatformSensorProvider>
      fake_platform_sensor_provider_;  // Owned by |sensor_provider_impl_|.
  raw_ptr<SensorProviderImpl>
      sensor_provider_impl_;  // Owned by the DeviceService instance.
};

// Requests the SensorProvider to create a sensor.
TEST_F(GenericSensorServiceTest, GetSensorTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  base::RunLoop run_loop;
  sensor_provider_->GetSensor(
      SensorType::ACCELEROMETER,
      base::BindOnce(&TestSensorClient::OnSensorCreated,
                     base::Unretained(client.get()), run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests GetDefaultConfiguration.
TEST_F(GenericSensorServiceTest, GetDefaultConfigurationTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // TODO(crbug.com/40274069): this test is not very meaningful. It could be
  // better to check if the default configuration is always clamped between the
  // minimum and maximum allowed frequencies (it currently is not), for example.
  base::test::TestFuture<const PlatformSensorConfiguration&> future;
  client->sensor()->GetDefaultConfiguration(future.GetCallback());
  EXPECT_DOUBLE_EQ(GetSensorDefaultFrequency(SensorType::ACCELEROMETER),
                   future.Get().frequency());
}

// Tests adding a valid configuration. Client should be notified by
// SensorClient::SensorReadingChanged().
TEST_F(GenericSensorServiceTest, ValidAddConfigurationTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::AMBIENT_LIGHT),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::AMBIENT_LIGHT);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::AMBIENT_LIGHT,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(50.0)));
  AddReadingWithFrequency(SensorType::AMBIENT_LIGHT);
  EXPECT_DOUBLE_EQ(client->WaitForReading(), 50.0);
}

// Tests adding an invalid configuation, the max allowed frequency is 50.0 in
// the mocked SensorImpl, while we add one with 60.0.
TEST_F(GenericSensorServiceTest, InvalidAddConfigurationTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::LINEAR_ACCELERATION),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client =
      std::make_unique<TestSensorClient>(SensorType::LINEAR_ACCELERATION);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::LINEAR_ACCELERATION,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Invalid configuration that exceeds the max allowed frequency.
  EXPECT_FALSE(client->AddConfigurationSync(PlatformSensorConfiguration(60.0)));
}

// Tests adding more than one clients. Sensor should send notification to all
// its clients.
TEST_F(GenericSensorServiceTest, MultipleClientsTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client_1 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  auto client_2 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_1.get()), barrier_closure));
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_2.get()), barrier_closure));
    run_loop.Run();
  }

  EXPECT_TRUE(
      client_1->AddConfigurationSync(PlatformSensorConfiguration(48.0)));

  // Expect the SensorReadingChanged() will be called for both clients.
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client_1->WaitForReading(), 48.0);
  EXPECT_DOUBLE_EQ(client_2->WaitForReading(), 48.0);
}

// Tests adding more than one clients. If mojo connection is broken on one
// client, other clients should not be affected.
TEST_F(GenericSensorServiceTest, ClientMojoConnectionBrokenTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client_1 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  auto client_2 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_1.get()), barrier_closure));
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_2.get()), barrier_closure));
    run_loop.Run();
  }

  // Breaks mojo connection of client_1.
  client_1->ResetSensor();

  // Expect the SensorReadingChanged() will be called on client_2.
  EXPECT_TRUE(
      client_2->AddConfigurationSync(PlatformSensorConfiguration(48.0)));
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client_2->WaitForReading(), 48.0);
}

// Test add and remove configuration operations.
TEST_F(GenericSensorServiceTest, AddAndRemoveConfigurationTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called. The frequency value
  // should be 10.0.
  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(10.0)));
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client->WaitForReading(), 10.0);

  // Expect the SensorReadingChanged() will be called. The frequency value
  // should be 40.0.
  PlatformSensorConfiguration configuration_40(40.0);
  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(40.0)));
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client->WaitForReading(), 40.0);

  // After |configuration_40| is removed, expect the SensorReadingChanged() will
  // be called. The frequency value should be 10.0.
  client->sensor()->RemoveConfiguration(configuration_40);
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client->WaitForReading(), 10.0);
}

// Test suspend. After suspending, the client won't be notified by
// SensorReadingChanged() after calling AddConfiguration.
// Call the AddConfiguration() twice, if the SensorReadingChanged() was
// called, it should happen before the callback triggerred by the second
// AddConfiguration(). In this way we make sure it won't be missed by the
// early quit of main thread (when there is an unexpected notification by
// SensorReadingChanged()).
TEST_F(GenericSensorServiceTest, SuspendTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::AMBIENT_LIGHT),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::AMBIENT_LIGHT);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::AMBIENT_LIGHT,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  client->sensor()->Suspend();

  // Expect the SensorReadingChanged() won't be called. Pass a bad value(123.0)
  // to |check_value_| to guarantee SensorReadingChanged() really doesn't be
  // called. Otherwise the CheckValue() will complain on the bad value.
  client->SetOnReadingChangedCallback(
      base::BindOnce([](double) { ADD_FAILURE() << "Unexpected reading."; }));

  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(30.0)));
  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(31.0)));

  const auto sensor_info =
      GetVirtualSensorInformationSync(SensorType::AMBIENT_LIGHT);
  ASSERT_TRUE(sensor_info);
  EXPECT_DOUBLE_EQ(sensor_info->sampling_frequency, 0.0);
}

// Tests that error notifications are delivered even if a sensor is suspended.
TEST_F(GenericSensorServiceTest, ErrorWhileSuspendedTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::AMBIENT_LIGHT),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::AMBIENT_LIGHT);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::AMBIENT_LIGHT,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  client->sensor()->Suspend();

  // Expect that SensorReadingChanged() will not be called.
  client->SetOnReadingChangedCallback(
      base::BindOnce([](double) { ADD_FAILURE() << "Unexpected reading."; }));
  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(30.0)));

  // Expect that RaiseError() will be called.
  base::test::TestFuture<void> error_future;
  client->SetOnErrorCallback(error_future.GetCallback());

  base::test::TestFuture<void> future;
  sensor_provider_->RemoveVirtualSensor(SensorType::AMBIENT_LIGHT,
                                        future.GetCallback());
  EXPECT_TRUE(error_future.Wait());
}

// Test suspend and resume. After resuming, client can add configuration and
// be notified by SensorReadingChanged() as usual.
TEST_F(GenericSensorServiceTest, SuspendThenResumeTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called. The frequency should
  // be 10.0 after AddConfiguration.
  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(10.0)));
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client->WaitForReading(), 10.0);

  client->sensor()->Suspend();
  {
    const auto sensor_info =
        GetVirtualSensorInformationSync(SensorType::ACCELEROMETER);
    ASSERT_TRUE(sensor_info);
    EXPECT_DOUBLE_EQ(sensor_info->sampling_frequency, 0.0);
  }
  client->sensor()->Resume();
  {
    const auto sensor_info =
        GetVirtualSensorInformationSync(SensorType::ACCELEROMETER);
    ASSERT_TRUE(sensor_info);
    EXPECT_DOUBLE_EQ(sensor_info->sampling_frequency, 10.0);
  }

  // Expect the SensorReadingChanged() will be called. The frequency should
  // be 50.0 after new configuration is added.
  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(50.0)));
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client->WaitForReading(), 50.0);
}

// Test suspend when there are more than one client. The suspended client won't
// receive SensorReadingChanged() notification.
TEST_F(GenericSensorServiceTest, MultipleClientsSuspendAndResumeTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);

  auto client_1 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  auto client_2 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_1.get()), barrier_closure));
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_2.get()), barrier_closure));
    run_loop.Run();
  }

  client_1->sensor()->Suspend();

  // Expect the sensor_2 will receive SensorReadingChanged() notification while
  // sensor_1 won't.
  EXPECT_TRUE(
      client_2->AddConfigurationSync(PlatformSensorConfiguration(46.0)));
  AddReadingWithFrequency(SensorType::ACCELEROMETER);
  EXPECT_DOUBLE_EQ(client_2->WaitForReading(), 46.0);
}

TEST_F(GenericSensorServiceTest, MojoReceiverDisconnectionTest) {
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 0U);
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);

  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_TRUE(client->AddConfigurationSync(PlatformSensorConfiguration(50.0)));
  EXPECT_EQ(UpdateVirtualSensorSync(SensorType::ACCELEROMETER, 42.0),
            mojom::UpdateVirtualSensorResult::kSuccess);

  // Break the Mojo connection to SensorProviderImpl. The corresponding
  // VirtualPlatformSensorProvider still has a connected accelerometer, so it
  // is not deleted.
  sensor_provider_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);
  {
    SensorReading reading;
    client->FetchSensorReading(&reading);
    EXPECT_DOUBLE_EQ(reading.als.value, 42.0);
  }

  // Now simulate a new connection to SensorProviderImpl (a new page, for
  // example).
  device_service()->BindSensorProvider(
      sensor_provider_.BindNewPipeAndPassReceiver());

  // Repeat the whole setup process.
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 2U);

  auto new_client =
      std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(new_client.get()),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_TRUE(
      new_client->AddConfigurationSync(PlatformSensorConfiguration(1.0)));
  EXPECT_EQ(UpdateVirtualSensorSync(SensorType::ACCELEROMETER, 2.0),
            mojom::UpdateVirtualSensorResult::kSuccess);

  // Check that the existing VirtualPlatformSensorProvider and
  // VirtualPlatformSensor instances do not interfere with one another.
  // |client| and |new_client| are connected to different
  // VirtualPlatformSensorProviders with different shared memory buffers.
  {
    SensorReading reading;
    client->FetchSensorReading(&reading);
    EXPECT_DOUBLE_EQ(reading.als.value, 42.0);
    new_client->FetchSensorReading(&reading);
    EXPECT_DOUBLE_EQ(reading.als.value, 2.0);
  }

  // Now disconnect from the new pressure sensor and the new
  // VirtualPlatformSensorProvider. This one should be deleted.
  new_client->ResetSensor();
  sensor_provider_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);

  // The newer Mojo connection to SensorProviderImpl was broken and the
  // corresponding VirtualPlatformSensorProvider has been deleted. All readings
  // have been zeroed.
  {
    SensorReading reading;
    client->FetchSensorReading(&reading);
    EXPECT_DOUBLE_EQ(reading.als.value, 42.0);
    new_client->FetchSensorReading(&reading);
    EXPECT_DOUBLE_EQ(reading.als.value, 0.0);
  }
}

TEST_F(GenericSensorServiceTest,
       DifferentVirtualAndNonVirtualPlatformSensorsTest) {
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 0U);
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);

  // Create a non-virtual sensor, make sure creation works as expected.
  auto client = std::make_unique<TestSensorClient>(SensorType::AMBIENT_LIGHT);
  EXPECT_CALL(*fake_platform_sensor_provider_, CreateSensorInternal).Times(1);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::AMBIENT_LIGHT,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  ASSERT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);
  // This only works and makes sense because of the assertion above.
  auto* provider =
      sensor_provider_impl_->GetLastVirtualSensorProviderForTesting();
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->IsOverridingSensor(SensorType::ACCELEROMETER));
  EXPECT_FALSE(provider->IsOverridingSensor(SensorType::AMBIENT_LIGHT));
}

TEST_F(GenericSensorServiceTest, SameVirtualAndNonVirtualPlatformSensorsTest) {
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 0U);
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);

  // Create a sensor. Its type is the one we created a virtual sensor for
  // above, so the non-virtual code path (i.e. the FakePlatformSensorProvider
  // in this case) should not be called.
  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  EXPECT_CALL(*fake_platform_sensor_provider_, CreateSensorInternal).Times(0);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  ASSERT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);
  // This only works and makes sense because of the assertion above.
  auto* provider =
      sensor_provider_impl_->GetLastVirtualSensorProviderForTesting();
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->IsOverridingSensor(SensorType::ACCELEROMETER));
}

TEST_F(GenericSensorServiceTest,
       QuaternionSensorsOverrideEulerAngleSensorsTest) {
  EXPECT_EQ(
      CreateVirtualSensorSync(SensorType::RELATIVE_ORIENTATION_QUATERNION),
      mojom::CreateVirtualSensorResult::kSuccess);

  ASSERT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);
  // This only works and makes sense because of the assertion above.
  auto* provider =
      sensor_provider_impl_->GetLastVirtualSensorProviderForTesting();
  ASSERT_TRUE(provider);
  EXPECT_FALSE(provider->IsOverridingSensor(
      SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_FALSE(provider->IsOverridingSensor(
      SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
  EXPECT_TRUE(provider->IsOverridingSensor(
      SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(provider->IsOverridingSensor(
      SensorType::RELATIVE_ORIENTATION_QUATERNION));

  EXPECT_EQ(
      CreateVirtualSensorSync(SensorType::ABSOLUTE_ORIENTATION_QUATERNION),
      mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_TRUE(provider->IsOverridingSensor(
      SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES));
  EXPECT_TRUE(provider->IsOverridingSensor(
      SensorType::ABSOLUTE_ORIENTATION_QUATERNION));
}

TEST_F(GenericSensorServiceTest, VirtualEulerAngleSensorCreationTest) {
  EXPECT_EQ(
      CreateVirtualSensorSync(SensorType::RELATIVE_ORIENTATION_QUATERNION),
      mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);

  // Create a sensor. Its type is the one we created a virtual sensor for
  // above, so the non-virtual code path (i.e. the FakePlatformSensorProvider
  // in this case) should not be called.
  auto client = std::make_unique<TestSensorClient>(
      SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
  EXPECT_CALL(*fake_platform_sensor_provider_, CreateSensorInternal).Times(0);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::RELATIVE_ORIENTATION_EULER_ANGLES,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  ASSERT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);
  // This only works and makes sense because of the assertion above.
  auto* provider =
      sensor_provider_impl_->GetLastVirtualSensorProviderForTesting();
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->IsOverridingSensor(
      SensorType::RELATIVE_ORIENTATION_EULER_ANGLES));
}

TEST_F(GenericSensorServiceTest, VirtualPlatformOverridesNonVirtualTest) {
  // Create a non-virtual sensor first.
  EXPECT_CALL(*fake_platform_sensor_provider_, CreateSensorInternal).Times(1);
  auto client1 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client1.get()),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Now start overriding sensors of this type.
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(sensor_provider_impl_->GetVirtualProviderCountForTesting(), 1U);
  auto client2 = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client2.get()),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Existing non-virtual sensors must continue to work.
  EXPECT_TRUE(client1->AddConfigurationSync(PlatformSensorConfiguration(46.0)));
  EXPECT_DOUBLE_EQ(client1->WaitForReading(), 46.0);

  EXPECT_TRUE(client2->AddConfigurationSync(PlatformSensorConfiguration(23.0)));

  // Updating virtual sensor does not change the non-virtual one.
  EXPECT_EQ(UpdateVirtualSensorSync(SensorType::ACCELEROMETER, 1.0),
            mojom::UpdateVirtualSensorResult::kSuccess);
  EXPECT_DOUBLE_EQ(client2->WaitForReading(), 1.0);
  SensorReading reading;
  client1->FetchSensorReading(&reading);
  EXPECT_DOUBLE_EQ(reading.als.value, 46.0);
  client2->FetchSensorReading(&reading);
  EXPECT_DOUBLE_EQ(reading.als.value, 1.0);
}

TEST_F(GenericSensorServiceTest, DoubleVirtualPlatformSensorCreationTest) {
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSuccess);
  EXPECT_EQ(CreateVirtualSensorSync(SensorType::ACCELEROMETER),
            mojom::CreateVirtualSensorResult::kSensorTypeAlreadyOverridden);
}

TEST_F(GenericSensorServiceTest, GetNonOverriddenSensorTest) {
  base::test::TestFuture<mojom::GetVirtualSensorInformationResultPtr> future;
  sensor_provider_->GetVirtualSensorInformation(SensorType::ACCELEROMETER,
                                                future.GetCallback());
  EXPECT_EQ(future.Get()->which(),
            mojom::GetVirtualSensorInformationResult::Tag::kError);
  EXPECT_EQ(future.Get()->get_error(),
            mojom::GetVirtualSensorInformationError::kSensorTypeNotOverridden);
}

TEST_F(GenericSensorServiceTest, UpdateNonOverriddenSensorTest) {
  EXPECT_EQ(UpdateVirtualSensorSync(SensorType::ACCELEROMETER, 42.0),
            mojom::UpdateVirtualSensorResult::kSensorTypeNotOverridden);
}

}  //  namespace device
