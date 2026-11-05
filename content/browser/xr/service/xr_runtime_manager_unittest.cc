// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/xr_runtime_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/xr/service/browser_xr_runtime_impl.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_renderer_host.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_device_provider.h"
#include "device/vr/test/fake_vr_service_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace content {

namespace {

// An XRRuntime that grants every session request, and that finishes shutting
// down a session only when the test calls FinishShutdown().
class FakeXRRuntime : public device::mojom::XRRuntime {
 public:
  FakeXRRuntime() = default;
  FakeXRRuntime(const FakeXRRuntime&) = delete;
  FakeXRRuntime& operator=(const FakeXRRuntime&) = delete;
  ~FakeXRRuntime() override = default;

  mojo::PendingRemote<device::mojom::XRRuntime> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // device::mojom::XRRuntime:
  void RequestSession(device::mojom::XRRuntimeSessionOptionsPtr options,
                      RequestSessionCallback callback) override {
    ++session_requests_;
    if (has_session_) {
      ++session_requests_while_presenting_;
    }
    has_session_ = true;

    auto result = device::mojom::XRRuntimeSessionResult::New();
    // Keep the receivers so that the browser's ends stay connected.
    controller_receivers_.push_back(
        result->controller.InitWithNewPipeAndPassReceiver());
    result->session = device::mojom::XRSession::New();
    data_provider_receivers_.push_back(
        result->session->data_provider.InitWithNewPipeAndPassReceiver());
    result->session->device_config =
        device::mojom::XRSessionDeviceConfig::New();
    result->session->enviroment_blend_mode =
        device::mojom::XREnvironmentBlendMode::kOpaque;
    result->session->interaction_mode =
        device::mojom::XRInteractionMode::kWorldSpace;
    std::move(callback).Run(std::move(result));
  }
  void ShutdownSession(ShutdownSessionCallback callback) override {
    shutdown_callback_ = std::move(callback);
  }
  void ListenToDeviceChanges(
      mojo::PendingAssociatedRemote<device::mojom::XRRuntimeEventListener>
          listener) override {}

  bool IsShuttingDown() const { return !shutdown_callback_.is_null(); }
  void FinishShutdown() {
    has_session_ = false;
    std::move(shutdown_callback_).Run();
  }

  int session_requests() const { return session_requests_; }
  int session_requests_while_presenting() const {
    return session_requests_while_presenting_;
  }

 private:
  mojo::Receiver<device::mojom::XRRuntime> receiver_{this};
  std::vector<mojo::PendingReceiver<device::mojom::XRSessionController>>
      controller_receivers_;
  std::vector<mojo::PendingReceiver<device::mojom::XRFrameDataProvider>>
      data_provider_receivers_;
  ShutdownSessionCallback shutdown_callback_;
  bool has_session_ = false;
  int session_requests_ = 0;
  int session_requests_while_presenting_ = 0;
};

device::mojom::XRRuntimeSessionOptionsPtr ImmersiveVrOptions() {
  auto options = device::mojom::XRRuntimeSessionOptions::New();
  options->mode = device::mojom::XRSessionMode::kImmersiveVr;
  return options;
}

}  // namespace

class XRRuntimeManagerTest : public RenderViewHostTestHarness {
 public:
  XRRuntimeManagerTest(const XRRuntimeManagerTest&) = delete;
  XRRuntimeManagerTest& operator=(const XRRuntimeManagerTest&) = delete;

 protected:
  XRRuntimeManagerTest() = default;
  ~XRRuntimeManagerTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    std::vector<std::unique_ptr<device::VRDeviceProvider>> providers;
    provider_ = new device::FakeVRDeviceProvider();
    providers.emplace_back(base::WrapUnique(provider_.get()));
    xr_runtime_manager_ = XRRuntimeManagerImpl::CreateInstance(
        std::move(providers), web_contents());
  }

  void TearDown() override {
    ClearProvider();
    DropRuntimeManagerRef();
    EXPECT_EQ(XRRuntimeManager::GetInstanceIfCreated(), nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<VRServiceImpl> BindService() {
    mojo::PendingRemote<device::mojom::VRServiceClient> proxy;
    device::FakeVRServiceClient client(proxy.InitWithNewPipeAndPassReceiver());
    auto service = std::make_unique<VRServiceImpl>(main_rfh());
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

  bool QuerySupportsSession(VRServiceImpl* service,
                            device::mojom::XRSessionMode mode) {
    device::mojom::XRSessionOptionsPtr options =
        device::mojom::XRSessionOptions::New();
    options->mode = mode;
    base::test::TestFuture<bool> future;
    service->SupportsSession(std::move(options), future.GetCallback());
    return future.Get();
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

TEST_F(XRRuntimeManagerTest, SupportsSession_VisibilityGate) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebXrForceRuntime, "fake");

  auto service = BindService();
  EXPECT_EQ(1u, ServiceCount());
  EXPECT_TRUE(Provider()->Initialized());

  device::FakeVRDevice* device =
      new device::FakeVRDevice(device::mojom::XRDeviceId::FAKE_DEVICE_ID);
  Provider()->AddDevice(base::WrapUnique(device));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetRuntimeForTest(device::mojom::XRDeviceId::FAKE_DEVICE_ID) !=
           nullptr;
  }));

  // When the frame is visible, SupportsSession should reflect device support.
  EXPECT_TRUE(QuerySupportsSession(service.get(),
                                   device::mojom::XRSessionMode::kImmersiveVr));

  // Simulate hiding the frame.
  RenderViewHostTester::For(rvh())->SimulateWasHidden();

#if BUILDFLAG(IS_ANDROID)
  // On Android, standard devices return true when hidden.
  EXPECT_TRUE(QuerySupportsSession(service.get(),
                                   device::mojom::XRSessionMode::kImmersiveVr));

  // On Desktop Android without XR device, hidden tabs should return false.
  base::android::device_info::set_is_desktop_for_testing(true);
  EXPECT_FALSE(QuerySupportsSession(
      service.get(), device::mojom::XRSessionMode::kImmersiveVr));

  // On Desktop Android with XR device, hidden tabs should return true.
  base::android::device_info::set_is_xr_for_testing();
  EXPECT_TRUE(QuerySupportsSession(service.get(),
                                   device::mojom::XRSessionMode::kImmersiveVr));

  base::android::device_info::reset_is_desktop_for_testing();
  base::android::device_info::reset_is_xr_for_testing();
#else
  // On desktop platforms (e.g. Windows/Linux), hidden tabs should return false.
  EXPECT_FALSE(QuerySupportsSession(
      service.get(), device::mojom::XRSessionMode::kImmersiveVr));
#endif

  // Restoring visibility should re-enable query results.
  RenderViewHostTester::For(rvh())->SimulateWasShown();
  EXPECT_TRUE(QuerySupportsSession(service.get(),
                                   device::mojom::XRSessionMode::kImmersiveVr));

  Provider()->RemoveDevice(device->GetId());
}

// An immersive session request that arrives while the runtime shuts down the
// previous session waits for the shutdown, instead of reaching the runtime
// while it still has the old session.
TEST_F(XRRuntimeManagerTest, ImmersiveRequestWaitsForSessionShutdown) {
  FakeXRRuntime fake_runtime;
  BrowserXRRuntimeImpl runtime(device::mojom::XRDeviceId::FAKE_DEVICE_ID,
                               device::mojom::XRDeviceData::New(),
                               fake_runtime.BindNewPipeAndPassRemote());
  auto presenting_service = BindService();
  auto next_service = BindService();
  runtime.OnServiceAdded(presenting_service.get());
  runtime.OnServiceAdded(next_service.get());

  base::test::TestFuture<device::mojom::XRRuntimeSessionResultPtr> first;
  runtime.RequestImmersiveSession(presenting_service.get(),
                                  ImmersiveVrOptions(), first.GetCallback());
  ASSERT_TRUE(first.Get());
  EXPECT_EQ(runtime.GetServiceWithActiveImmersiveSession(),
            presenting_service.get());

  // The presenting page goes away, e.g. because it navigated.
  runtime.OnServiceRemoved(presenting_service.get());
  presenting_service.reset();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return fake_runtime.IsShuttingDown(); }));

  base::test::TestFuture<device::mojom::XRRuntimeSessionResultPtr> second;
  runtime.RequestImmersiveSession(next_service.get(), ImmersiveVrOptions(),
                                  second.GetCallback());
  runtime.FlushForTesting();
  EXPECT_FALSE(second.IsReady());
  EXPECT_TRUE(runtime.HasPendingImmersiveSessionRequest());
  EXPECT_EQ(fake_runtime.session_requests(), 1);

  fake_runtime.FinishShutdown();
  ASSERT_TRUE(second.Get());
  EXPECT_FALSE(runtime.HasPendingImmersiveSessionRequest());
  EXPECT_EQ(runtime.GetServiceWithActiveImmersiveSession(), next_service.get());
  EXPECT_EQ(fake_runtime.session_requests(), 2);
  EXPECT_EQ(fake_runtime.session_requests_while_presenting(), 0);

  runtime.OnServiceRemoved(next_service.get());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return fake_runtime.IsShuttingDown(); }));
  fake_runtime.FinishShutdown();
  runtime.FlushForTesting();
}

// A request that waits for the shutdown fails if its page goes away first.
TEST_F(XRRuntimeManagerTest, WaitingImmersiveRequestFailsWhenServiceIsRemoved) {
  FakeXRRuntime fake_runtime;
  BrowserXRRuntimeImpl runtime(device::mojom::XRDeviceId::FAKE_DEVICE_ID,
                               device::mojom::XRDeviceData::New(),
                               fake_runtime.BindNewPipeAndPassRemote());
  auto presenting_service = BindService();
  auto next_service = BindService();
  runtime.OnServiceAdded(presenting_service.get());
  runtime.OnServiceAdded(next_service.get());

  base::test::TestFuture<device::mojom::XRRuntimeSessionResultPtr> first;
  runtime.RequestImmersiveSession(presenting_service.get(),
                                  ImmersiveVrOptions(), first.GetCallback());
  ASSERT_TRUE(first.Get());

  runtime.OnServiceRemoved(presenting_service.get());
  presenting_service.reset();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return fake_runtime.IsShuttingDown(); }));

  base::test::TestFuture<device::mojom::XRRuntimeSessionResultPtr> second;
  runtime.RequestImmersiveSession(next_service.get(), ImmersiveVrOptions(),
                                  second.GetCallback());
  runtime.OnServiceRemoved(next_service.get());
  next_service.reset();

  fake_runtime.FinishShutdown();
  EXPECT_FALSE(second.Get());
  EXPECT_FALSE(runtime.HasPendingImmersiveSessionRequest());
  EXPECT_EQ(fake_runtime.session_requests(), 1);
}

}  // namespace content
