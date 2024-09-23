// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/test/fake_orientation_provider.h"
#include "device/vr/test/fake_sensor_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/quaternion.h"

namespace device {

namespace {
std::unique_ptr<XrFrameSinkClient> FrameSinkClientFactory(int32_t, int32_t) {
  return nullptr;
}
}  // namespace

class VROrientationDeviceProviderTest : public testing::Test {
 public:
  VROrientationDeviceProviderTest(const VROrientationDeviceProviderTest&) =
      delete;
  VROrientationDeviceProviderTest& operator=(
      const VROrientationDeviceProviderTest&) = delete;

 protected:
  VROrientationDeviceProviderTest() = default;
  ~VROrientationDeviceProviderTest() override = default;
  void SetUp() override {
    fake_sensor_provider_ = std::make_unique<FakeXRSensorProvider>();

    fake_sensor_ = std::make_unique<FakeOrientationSensor>(
        sensor_.InitWithNewPipeAndPassReceiver());
    mapped_region_ = base::ReadOnlySharedMemoryRegion::Create(
        sizeof(SensorReadingSharedBuffer) *
        (static_cast<uint64_t>(mojom::SensorType::kMaxValue) + 1));
    ASSERT_TRUE(mapped_region_.IsValid());

    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider;
    fake_sensor_provider_->Bind(
        sensor_provider.InitWithNewPipeAndPassReceiver());
    provider_ = std::make_unique<VROrientationDeviceProvider>(
        std::move(sensor_provider));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {}

  void InitializeDevice(mojom::SensorInitParamsPtr params) {
    // Be sure GetSensor goes through so the callback is set.
    task_environment_.RunUntilIdle();

    fake_sensor_provider_->CallCallback(std::move(params));

    // Allow the callback call to go through.
    task_environment_.RunUntilIdle();
  }

  mojom::SensorInitParamsPtr FakeInitParams() {
    auto init_params = mojom::SensorInitParams::New();
    init_params->sensor = std::move(sensor_);
    init_params->default_configuration = PlatformSensorConfiguration(
        SensorTraits<kOrientationSensorType>::kDefaultFrequency);

    init_params->client_receiver = sensor_client_.BindNewPipeAndPassReceiver();

    init_params->memory = mapped_region_.region.Duplicate();

    init_params->buffer_offset =
        GetSensorReadingSharedBufferOffset(kOrientationSensorType);

    return init_params;
  }

  // Needed for MakeRequest to work.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<VROrientationDeviceProvider> provider_;

  std::unique_ptr<FakeXRSensorProvider> fake_sensor_provider_;
  mojo::Remote<mojom::SensorProvider> sensor_provider_;

  // Fake Sensor Init params objects
  std::unique_ptr<FakeOrientationSensor> fake_sensor_;
  mojo::PendingRemote<mojom::Sensor> sensor_;
  base::MappedReadOnlyRegion mapped_region_;
  mojo::Remote<mojom::SensorClient> sensor_client_;
};

class MockOrientationDeviceProviderClient : public VRDeviceProviderClient {
 public:
  MockOrientationDeviceProviderClient(base::RunLoop* wait_for_device,
                                      base::RunLoop* wait_for_init)
      : wait_for_device_(wait_for_device), wait_for_init_(wait_for_init) {}
  ~MockOrientationDeviceProviderClient() override = default;
  void AddRuntime(
      device::mojom::XRDeviceId id,
      device::mojom::XRDeviceDataPtr device_data,
      mojo::PendingRemote<device::mojom::XRRuntime> runtime) override {
    if (wait_for_device_) {
      ASSERT_TRUE(device_data);
      ASSERT_TRUE(runtime);
      wait_for_device_->Quit();
      return;
    }

    // If we were created without a wait_for_device runloop, then the test
    // is not expecting us to be called.
    FAIL();
  }

  void RemoveRuntime(device::mojom::XRDeviceId id) override {
    // The only devices that actually create an Orientation device cannot
    // physically remove the orientation sensors, so this should never be
    // called.
    FAIL();
  }

  void OnProviderInitialized() override {
    if (!wait_for_init_) {
      FAIL();
    }

    wait_for_init_->Quit();
  }

  device::XrFrameSinkClientFactory GetXrFrameSinkClientFactory() override {
    ADD_FAILURE();

    return base::BindRepeating(&FrameSinkClientFactory);
  }

 private:
  raw_ptr<base::RunLoop> wait_for_device_ = nullptr;
  raw_ptr<base::RunLoop> wait_for_init_ = nullptr;
};

TEST_F(VROrientationDeviceProviderTest, InitializationTest) {
  // Check that without running anything, the provider will not be initialized.
  EXPECT_FALSE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackSuccessTest) {
  base::RunLoop wait_for_device;
  base::RunLoop wait_for_init;

  MockOrientationDeviceProviderClient client(&wait_for_device, &wait_for_init);

  // The orientation device provider does not make use of the WebContents.
  provider_->Initialize(&client, nullptr);

  InitializeDevice(FakeInitParams());

  wait_for_init.Run();
  wait_for_device.Run();

  EXPECT_TRUE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackFailureTest) {
  base::RunLoop wait_for_init;

  MockOrientationDeviceProviderClient client(nullptr, &wait_for_init);

  // The orientation device provider does not make use of the WebContents.
  provider_->Initialize(&client, nullptr);

  InitializeDevice(nullptr);

  // Wait for the initialization to finish.
  wait_for_init.Run();
  EXPECT_TRUE(provider_->Initialized());
}

}  // namespace device
