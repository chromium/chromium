// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/compute_pressure/pressure_client_impl.h"
#include "content/browser/compute_pressure/pressure_service_for_frame.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "url/gurl.h"

namespace content {

using device::mojom::PressureSource;
using device::mojom::PressureState;
using device::mojom::PressureStatus;
using device::mojom::PressureUpdate;

namespace {

// Test double for PressureClient that records all updates.
class FakePressureClient : public device::mojom::PressureClient {
 public:
  FakePressureClient() : receiver_(this) {}
  ~FakePressureClient() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  FakePressureClient(const FakePressureClient&) = delete;
  FakePressureClient& operator=(const FakePressureClient&) = delete;

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(device::mojom::PressureUpdatePtr state) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    updates_.push_back(*state);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  std::vector<PressureUpdate>& updates() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return updates_;
  }

  void SetNextUpdateCallback(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!update_callback_) << " already called before update received";

    update_callback_ = std::move(callback);
  }

  void WaitForUpdate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::RunLoop run_loop;
    SetNextUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  static void WaitForUpdates(
      std::initializer_list<FakePressureClient*> clients) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(clients.size(), run_loop.QuitClosure());
    for (FakePressureClient* client : clients) {
      client->SetNextUpdateCallback(update_barrier);
    }
    run_loop.Run();
  }

  mojo::PendingRemote<device::mojom::PressureClient>
  BindNewPipeAndPassRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<PressureUpdate> updates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<device::mojom::PressureClient> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

class PressureServiceForFrameTest : public RenderViewHostImplTestHarness {
 public:
  PressureServiceForFrameTest() = default;
  ~PressureServiceForFrameTest() override = default;

  PressureServiceForFrameTest(const PressureServiceForFrameTest&) = delete;
  PressureServiceForFrameTest& operator=(const PressureServiceForFrameTest&) =
      delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    SetPressureServiceForFrame();
  }

  void TearDown() override {
    pressure_manager_overrider_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetPressureServiceForFrame() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedPressureManagerOverrider>();
    pressure_manager_.reset();
    auto* rfh = contents()->GetPrimaryMainFrame();
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfh->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
    broker->GetInterface(pressure_manager_.BindNewPipeAndPassReceiver());
    // Focus on the page and frame to make HasImplicitFocus() return true
    // by default.
    rfh->GetRenderWidgetHost()->Focus();
    FocusWebContentsOnMainFrame();
    task_environment()->RunUntilIdle();
  }

  PressureStatus AddPressureClient(
      mojo::PendingRemote<device::mojom::PressureClient> client,
      PressureSource source) {
    base::test::TestFuture<PressureStatus> future;
    pressure_manager_->AddClient(std::move(client), source,
                                 future.GetCallback());
    return future.Get();
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const GURL kInsecureUrl{"http://example.com/compute_pressure.html"};

  mojo::Remote<device::mojom::PressureManager> pressure_manager_;
  std::unique_ptr<device::ScopedPressureManagerOverrider>
      pressure_manager_overrider_;
};

TEST_F(PressureServiceForFrameTest, AddClient) {
  FakePressureClient client;
  ASSERT_EQ(AddPressureClient(client.BindNewPipeAndPassRemote(),
                              PressureSource::kCpu),
            PressureStatus::kOk);

  const base::TimeTicks time = base::TimeTicks::Now();
  PressureUpdate update(PressureSource::kCpu, PressureState::kNominal, time);
  pressure_manager_overrider_->UpdateClients(update);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update);
}

TEST_F(PressureServiceForFrameTest, AddClientNotSupported) {
  pressure_manager_overrider_->set_is_supported(false);

  FakePressureClient client;
  ASSERT_EQ(AddPressureClient(client.BindNewPipeAndPassRemote(),
                              PressureSource::kCpu),
            PressureStatus::kNotSupported);

  const auto& pressure_client =
      PressureServiceForFrame::GetOrCreateForCurrentDocument(
          contents()->GetPrimaryMainFrame())
          ->GetPressureClientForTesting(PressureSource::kCpu);
  EXPECT_FALSE(pressure_client.IsClientReceiverBoundForTesting());
}

TEST_F(PressureServiceForFrameTest, AddClientTwice) {
  FakePressureClient client1;
  ASSERT_EQ(AddPressureClient(client1.BindNewPipeAndPassRemote(),
                              PressureSource::kCpu),
            PressureStatus::kOk);

  // Simulate the renderer calling AddClient twice for the same PressureSource
  // and wait for the PressureServiceBase to finish the call.
  FakePressureClient client2;
  mojo::test::BadMessageObserver bad_message_observer;
  pressure_manager_->AddClient(client2.BindNewPipeAndPassRemote(),
                               PressureSource::kCpu, base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "PressureClientImpl is already connected.");

  auto* pressure_service =
      PressureServiceForFrame::GetOrCreateForCurrentDocument(
          contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(pressure_service->IsManagerReceiverBoundForTesting());
}

TEST_F(PressureServiceForFrameTest, DisconnectFromBlink) {
  FakePressureClient client;
  ASSERT_EQ(AddPressureClient(client.BindNewPipeAndPassRemote(),
                              PressureSource::kCpu),
            PressureStatus::kOk);

  // Simulate the renderer disconnecting and wait for the PressureServiceBase
  // to observe the pipe close.
  pressure_manager_.reset();
  task_environment()->RunUntilIdle();

  auto* pressure_service =
      PressureServiceForFrame::GetOrCreateForCurrentDocument(
          contents()->GetPrimaryMainFrame());
  const auto& pressure_client =
      pressure_service->GetPressureClientForTesting(PressureSource::kCpu);
  EXPECT_FALSE(pressure_service->IsManagerReceiverBoundForTesting());
  EXPECT_TRUE(pressure_client.IsClientRemoteBoundForTesting());
  EXPECT_TRUE(pressure_client.IsClientReceiverBoundForTesting());
}

TEST_F(PressureServiceForFrameTest, InsecureOrigin) {
  NavigateAndCommit(kInsecureUrl);

  SetPressureServiceForFrame();
  EXPECT_EQ(1, process()->bad_msg_count());
}

TEST_F(PressureServiceForFrameTest, PermissionsPolicyBlock) {
  // Make compute pressure blocked by permissions policy and it can only be
  // made once on page load, so we refresh the page to simulate that.
  RenderFrameHost* rfh =
      static_cast<RenderFrameHost*>(contents()->GetPrimaryMainFrame());
  blink::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      blink::mojom::PermissionsPolicyFeature::kComputePressure;
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      rfh->GetLastCommittedURL(), rfh);
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  SetPressureServiceForFrame();
  EXPECT_EQ(1, process()->bad_msg_count());
}

// Allows callers to run a custom callback before running
// FakePressureManager::AddClient().
class InterceptingFakePressureManager : public device::FakePressureManager {
 public:
  explicit InterceptingFakePressureManager(
      base::OnceClosure interception_callback)
      : interception_callback_(std::move(interception_callback)) {}

  void AddClient(mojo::PendingRemote<device::mojom::PressureClient> client,
                 device::mojom::PressureSource source,
                 AddClientCallback callback) override {
    std::move(interception_callback_).Run();
    device::FakePressureManager::AddClient(std::move(client), source,
                                           std::move(callback));
  }

 private:
  base::OnceClosure interception_callback_;
};

// Test for https://crbug.com/1355662: destroying PressureServiceForFrameTest
// between calling PressureServiceForFrame::AddClient() and its
// |manager_remote_| invoking the callback it receives does not crash.
TEST_F(PressureServiceForFrameTest, DestructionOrderWithOngoingCallback) {
  auto intercepting_fake_pressure_manager =
      std::make_unique<InterceptingFakePressureManager>(
          base::BindLambdaForTesting([&]() {
            // Delete the current WebContents and consequently trigger
            // PressureServiceForFrame's destruction between calling
            // PressureServiceForFrame::AddClient() and its |manager_remote_|
            // invoking the callback it receives.
            DeleteContents();
          }));
  pressure_manager_overrider_->set_fake_pressure_manager(
      std::move(intercepting_fake_pressure_manager));

  base::RunLoop run_loop;
  pressure_manager_.set_disconnect_handler(run_loop.QuitClosure());
  FakePressureClient client;
  pressure_manager_->AddClient(
      client.BindNewPipeAndPassRemote(), PressureSource::kCpu,
      base::BindOnce([](device::mojom::PressureStatus) {
        ADD_FAILURE() << "Reached AddClient callback unexpectedly";
      }));
  run_loop.Run();
}

class PressureServiceForFrameFencedFrameTest
    : public PressureServiceForFrameTest {
 public:
  PressureServiceForFrameFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~PressureServiceForFrameFencedFrameTest() override = default;

 protected:
  RenderFrameHost* CreateFencedFrame(RenderFrameHost* parent) {
    RenderFrameHost* fenced_frame =
        RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PressureServiceForFrameFencedFrameTest, AccessFromFencedFrame) {
  auto* fenced_frame_rfh = CreateFencedFrame(contents()->GetPrimaryMainFrame());
  // Secure origin check will fail if the RenderFrameHost* passed to it has
  // not navigated to a secure origin, so we need to create a navigation here.
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://fencedframe.com"), fenced_frame_rfh);
  navigation_simulator->Commit();
  fenced_frame_rfh = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());

  mojo::Remote<device::mojom::PressureManager> fenced_frame_pressure_manager;
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      static_cast<RenderFrameHostImpl*>(fenced_frame_rfh)
          ->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
  broker->GetInterface(
      fenced_frame_pressure_manager.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(fenced_frame_rfh)
                   ->GetProcess()
                   ->bad_msg_count());
}

}  // namespace content
