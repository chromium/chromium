// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_device_provider.h"
#include "device/vr/test/fake_vr_service_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class XRRuntimeManagerTest : public testing::Test {
 public:
  XRRuntimeManagerTest(const XRRuntimeManagerTest&) = delete;
  XRRuntimeManagerTest& operator=(const XRRuntimeManagerTest&) = delete;

 protected:
  XRRuntimeManagerTest() = default;
  ~XRRuntimeManagerTest() override = default;

  void SetUp() override {
    std::vector<std::unique_ptr<device::VRDeviceProvider>> providers;
    provider_ = new device::FakeVRDeviceProvider();
    providers.emplace_back(base::WrapUnique(provider_.get()));
    xr_runtime_manager_ =
        XRRuntimeManagerImpl::CreateInstance(std::move(providers), nullptr);
  }

  void TearDown() override {
    ClearProvider();
    DropRuntimeManagerRef();
    EXPECT_EQ(XRRuntimeManager::GetInstanceIfCreated(), nullptr);
  }

  std::unique_ptr<VRServiceImpl> BindService() {
    // The mojom bindings that get run as part of adding a device need to run on
    // a single thread.
    base::test::SingleThreadTaskEnvironment task_environment;
    mojo::PendingRemote<device::mojom::VRServiceClient> proxy;
    device::FakeVRServiceClient client(proxy.InitWithNewPipeAndPassReceiver());
    auto service =
        std::make_unique<VRServiceImpl>(base::PassKey<XRRuntimeManagerTest>());
    service->SetClient(std::move(proxy));
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    return service;
  }

  scoped_refptr<XRRuntimeManagerImpl> GetRuntimeManager() {
    EXPECT_NE(XRRuntimeManager::GetInstanceIfCreated(), nullptr);
    return XRRuntimeManagerImpl::GetOrCreateInstanceForTesting();
  }

  device::mojom::XRRuntime* GetRuntimeForTest(
      device::mojom::XRDeviceId device_id) {
    return GetRuntimeManager()->GetRuntimeForTest(device_id);
  }

  size_t ServiceCount() {
    return GetRuntimeManager()->NumberOfConnectedServices();
  }

  device::FakeVRDeviceProvider* Provider() {
    EXPECT_NE(XRRuntimeManager::GetInstanceIfCreated(), nullptr);
    return provider_;
  }

  // Drops the internal XRRuntimeManagerImplRef. This is useful for testing the
  // reference counting behavior of the XRRuntimeManagerImpl singleton.
  void DropRuntimeManagerRef() { xr_runtime_manager_ = nullptr; }

  void ClearProvider() { provider_ = nullptr; }

 private:
  raw_ptr<device::FakeVRDeviceProvider> provider_ = nullptr;
  scoped_refptr<XRRuntimeManagerImpl> xr_runtime_manager_;
};

TEST_F(XRRuntimeManagerTest, InitializationTest) {
  // Returns true because XRRuntimeManagerImpl is created at the constructor.
  EXPECT_TRUE(Provider()->Initialized());
}

TEST_F(XRRuntimeManagerTest, GetNoDevicesTest) {
  auto service = BindService();
  // Calling GetVRDevices should initialize the providers.
  EXPECT_TRUE(Provider()->Initialized());

  // GetDeviceByIndex should return nullptr if an invalid index in queried.
  device::mojom::XRRuntime* queried_device =
      GetRuntimeForTest(device::mojom::XRDeviceId::FAKE_DEVICE_ID);
  EXPECT_EQ(nullptr, queried_device);
}

// Ensure that services are registered with the device manager as they are
// created and removed from the device manager as their connections are closed.
TEST_F(XRRuntimeManagerTest, DeviceManagerRegistration) {
  EXPECT_EQ(0u, ServiceCount());
  auto service_1 = BindService();
  EXPECT_EQ(1u, ServiceCount());
  auto service_2 = BindService();
  EXPECT_EQ(2u, ServiceCount());
  service_1.reset();
  EXPECT_EQ(1u, ServiceCount());
  service_2.reset();

  ClearProvider();
  DropRuntimeManagerRef();
  EXPECT_EQ(XRRuntimeManager::GetInstanceIfCreated(), nullptr);
}

// Ensure that devices added and removed are reflected in calls to request
// sessions.
TEST_F(XRRuntimeManagerTest, AddRemoveDevices) {
  auto service = BindService();
  EXPECT_EQ(1u, ServiceCount());
  EXPECT_TRUE(Provider()->Initialized());

  // The mojom bindings that get run as part of adding a device need to run on
  // a single thread.
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  device::FakeVRDevice* device = new device::FakeVRDevice(
      device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID);
  Provider()->AddDevice(base::WrapUnique(device));
  run_loop.RunUntilIdle();

  device::mojom::XRSessionOptions options = {};
  options.mode = device::mojom::XRSessionMode::kInline;
  EXPECT_TRUE(GetRuntimeManager()->GetRuntimeForOptions(&options));
  Provider()->RemoveDevice(device->GetId());
  EXPECT_TRUE(!GetRuntimeManager()->GetRuntimeForOptions(&options));
}

}  // namespace content
