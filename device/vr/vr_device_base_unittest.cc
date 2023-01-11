// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class VRDeviceBaseForTesting : public VRDeviceBase {
 public:
  VRDeviceBaseForTesting() : VRDeviceBase(mojom::XRDeviceId::FAKE_DEVICE_ID) {}

  VRDeviceBaseForTesting(const VRDeviceBaseForTesting&) = delete;
  VRDeviceBaseForTesting& operator=(const VRDeviceBaseForTesting&) = delete;

  ~VRDeviceBaseForTesting() override = default;

  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override {}
  void ShutdownSession(
      mojom::XRRuntime::ShutdownSessionCallback callback) override {
    std::move(callback).Run();
  }
};

class StubVRDeviceEventListener : public mojom::XRRuntimeEventListener {
 public:
  StubVRDeviceEventListener() = default;
  ~StubVRDeviceEventListener() override = default;

  MOCK_METHOD0(OnExitPresent, void());
  MOCK_METHOD1(OnVisibilityStateChanged, void(mojom::XRVisibilityState));

  mojo::PendingAssociatedRemote<mojom::XRRuntimeEventListener>
  BindPendingRemote() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  mojo::AssociatedReceiver<mojom::XRRuntimeEventListener> receiver_{this};
};

}  // namespace

class VRDeviceTest : public testing::Test {
 public:
  VRDeviceTest() {}

  VRDeviceTest(const VRDeviceTest&) = delete;
  VRDeviceTest& operator=(const VRDeviceTest&) = delete;

  ~VRDeviceTest() override {}

 protected:
  std::unique_ptr<VRDeviceBaseForTesting> MakeVRDevice() {
    std::unique_ptr<VRDeviceBaseForTesting> device =
        std::make_unique<VRDeviceBaseForTesting>();
    return device;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests VRDevice class default behaviour when it dispatches "vrdevicechanged"
// event. The expected behaviour is all of the services related with this device
// will receive the "vrdevicechanged" event.
TEST_F(VRDeviceTest, DeviceChangedDispatched) {
  auto device = MakeVRDevice();
  mojo::Remote<mojom::XRRuntime> device_remote(device->BindXRRuntime());
  StubVRDeviceEventListener listener;
  device_remote->ListenToDeviceChanges(listener.BindPendingRemote());
  base::RunLoop().RunUntilIdle();
  base::RunLoop().RunUntilIdle();
}

}  // namespace device
