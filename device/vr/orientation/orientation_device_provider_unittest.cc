// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/base/features.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/test/fake_orientation_provider.h"
#include "device/vr/test/fake_sensor_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/quaternion.h"

namespace device {

class VROrientationDeviceProviderTest : public testing::Test {
 protected:
  VROrientationDeviceProviderTest() = default;
  ~VROrientationDeviceProviderTest() override = default;
  void SetUp() override {
    fake_sensor_provider_ = std::make_unique<FakeSensorProvider>();

    fake_sensor_ = std::make_unique<FakeOrientationSensor>(
        sensor_.InitWithNewPipeAndPassReceiver());
    shared_buffer_handle_ = mojo::SharedBufferHandle::Create(
        sizeof(SensorReadingSharedBuffer) *
        (static_cast<uint64_t>(mojom::SensorType::kMaxValue) + 1));

    mojo::PendingReceiver<service_manager::mojom::Connector> receiver;
    connector_ = service_manager::Connector::Create(&receiver);
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(mojom::kServiceName),
        mojom::SensorProvider::Name_,
        base::BindRepeating(&FakeSensorProvider::Bind,
                            base::Unretained(fake_sensor_provider_.get())));

    provider_ = std::make_unique<VROrientationDeviceProvider>(connector_.get());

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

    init_params->memory = shared_buffer_handle_->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY);

    init_params->buffer_offset =
        SensorReadingSharedBuffer::GetOffset(kOrientationSensorType);

    return init_params;
  }

  base::RepeatingCallback<void(device::mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojo::PendingRemote<mojom::XRRuntime> device)>
  DeviceAndIdCallbackFailIfCalled() {
    return base::BindRepeating(
        [](device::mojom::XRDeviceId id, mojom::VRDisplayInfoPtr,
           mojo::PendingRemote<mojom::XRRuntime> device) { FAIL(); });
  }

  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
  DeviceIdCallbackFailIfCalled() {
    return base::BindRepeating([](device::mojom::XRDeviceId id) { FAIL(); });
  }

  base::RepeatingCallback<void(device::mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojo::PendingRemote<mojom::XRRuntime> device)>
  DeviceAndIdCallbackMustBeCalled(base::RunLoop* loop) {
    return base::BindRepeating(
        [](base::OnceClosure quit_closure, device::mojom::XRDeviceId id,
           mojom::VRDisplayInfoPtr info,
           mojo::PendingRemote<mojom::XRRuntime> device) {
          ASSERT_TRUE(device);
          ASSERT_TRUE(info);
          std::move(quit_closure).Run();
        },
        loop->QuitClosure());
  }

  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
  DeviceIdCallbackMustBeCalled(base::RunLoop* loop) {
    return base::BindRepeating(
        [](base::OnceClosure quit_closure, device::mojom::XRDeviceId id) {
          std::move(quit_closure).Run();
        },
        loop->QuitClosure());
  }

  base::OnceClosure ClosureFailIfCalled() {
    return base::BindOnce([]() { FAIL(); });
  }

  base::OnceClosure ClosureMustBeCalled(base::RunLoop* loop) {
    return base::BindOnce(
        [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
        loop->QuitClosure());
  }

  // Needed for MakeRequest to work.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<VROrientationDeviceProvider> provider_;

  std::unique_ptr<FakeSensorProvider> fake_sensor_provider_;
  mojo::Remote<mojom::SensorProvider> sensor_provider_;

  // Fake Sensor Init params objects
  std::unique_ptr<FakeOrientationSensor> fake_sensor_;
  mojo::PendingRemote<mojom::Sensor> sensor_;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojo::Remote<mojom::SensorClient> sensor_client_;

  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(VROrientationDeviceProviderTest);
};

TEST_F(VROrientationDeviceProviderTest, InitializationTest) {
  // Check that without running anything, the provider will not be initialized.
  EXPECT_FALSE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackSuccessTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      device::kWebXrOrientationSensorDevice);

  base::RunLoop wait_for_device;
  base::RunLoop wait_for_init;

  provider_->Initialize(DeviceAndIdCallbackMustBeCalled(&wait_for_device),
                        DeviceIdCallbackFailIfCalled(),
                        ClosureMustBeCalled(&wait_for_init));

  InitializeDevice(FakeInitParams());

  wait_for_init.Run();
  wait_for_device.Run();

  EXPECT_TRUE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackFailureTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      device::kWebXrOrientationSensorDevice);

  base::RunLoop wait_for_init;

  provider_->Initialize(DeviceAndIdCallbackFailIfCalled(),
                        DeviceIdCallbackFailIfCalled(),
                        ClosureMustBeCalled(&wait_for_init));

  InitializeDevice(nullptr);

  // Wait for the initialization to finish.
  wait_for_init.Run();
  EXPECT_TRUE(provider_->Initialized());
}

TEST_F(VROrientationDeviceProviderTest, InitializationCallbackUnsupportedTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      device::kWebXrOrientationSensorDevice);

  base::RunLoop wait_for_init;

  provider_->Initialize(DeviceAndIdCallbackFailIfCalled(),
                        DeviceIdCallbackFailIfCalled(),
                        ClosureMustBeCalled(&wait_for_init));

  // With the feature disabled, the device should still be initialized to match
  // the failure case above, but we shouldn't need any callbacks triggered via
  // InitializeDevice.
  wait_for_init.Run();
  EXPECT_TRUE(provider_->Initialized());
}

}  // namespace device
