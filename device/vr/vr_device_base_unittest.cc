// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
  ~VRDeviceBaseForTesting() override = default;

  void SetVRDisplayInfoForTest(mojom::VRDisplayInfoPtr display_info) {
    SetVRDisplayInfo(std::move(display_info));
  }

  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override {}

 private:

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBaseForTesting);
};

class StubVRDeviceEventListener : public mojom::XRRuntimeEventListener {
 public:
  StubVRDeviceEventListener() = default;
  ~StubVRDeviceEventListener() override = default;

  MOCK_METHOD1(DoOnChanged, void(mojom::VRDisplayInfo* vr_device_info));
  void OnDisplayInfoChanged(mojom::VRDisplayInfoPtr vr_device_info) override {
    DoOnChanged(vr_device_info.get());
  }

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
  ~VRDeviceTest() override {}

 protected:
  std::unique_ptr<VRDeviceBaseForTesting> MakeVRDevice() {
    std::unique_ptr<VRDeviceBaseForTesting> device =
        std::make_unique<VRDeviceBaseForTesting>();
    device->SetVRDisplayInfoForTest(MakeVRDisplayInfo(device->GetId()));
    return device;
  }

  mojom::VRDisplayInfoPtr MakeVRDisplayInfo(mojom::XRDeviceId device_id) {
    mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
    display_info->id = device_id;
    return display_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceTest);
};

// Tests VRDevice class default behaviour when it dispatches "vrdevicechanged"
// event. The expected behaviour is all of the services related with this device
// will receive the "vrdevicechanged" event.
TEST_F(VRDeviceTest, DeviceChangedDispatched) {
  auto device = MakeVRDevice();
  mojo::Remote<mojom::XRRuntime> device_remote(device->BindXRRuntime());
  StubVRDeviceEventListener listener;
  device_remote->ListenToDeviceChanges(
      listener.BindPendingRemote(),
      base::DoNothing());  // TODO: consider getting initial info
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(listener, DoOnChanged(testing::_)).Times(1);
  device->SetVRDisplayInfoForTest(MakeVRDisplayInfo(device->GetId()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace device
