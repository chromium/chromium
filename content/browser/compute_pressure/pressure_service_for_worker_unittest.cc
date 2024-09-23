// Copyright 2023 The Chromium Authors
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
#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager.mojom.h"
#include "url/gurl.h"

namespace content {

using device::mojom::PressureManagerAddClientError;
using device::mojom::PressureSource;
using device::mojom::PressureState;
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

  void Bind(
      mojo::PendingReceiver<device::mojom::PressureClient> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    receiver_.Bind(std::move(pending_receiver));
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

class PressureServiceForDedicatedWorkerTest
    : public RenderViewHostImplTestHarness {
 public:
  PressureServiceForDedicatedWorkerTest() = default;
  ~PressureServiceForDedicatedWorkerTest() override = default;

  PressureServiceForDedicatedWorkerTest(
      const PressureServiceForDedicatedWorkerTest&) = delete;
  PressureServiceForDedicatedWorkerTest& operator=(
      const PressureServiceForDedicatedWorkerTest&) = delete;

  void TearDown() override {
    pressure_manager_overrider_.reset();
    worker_host_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetPressureServiceForDedicatedWorker() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedPressureManagerOverrider>();
    pressure_manager_.reset();

    auto* rfh = contents()->GetPrimaryMainFrame();
    CHECK_EQ(rfh->GetLastCommittedOrigin(), rfh->GetStorageKey().origin());
    worker_host_ = std::make_unique<DedicatedWorkerHost>(
        &worker_service_, blink::DedicatedWorkerToken(), rfh->GetProcess(),
        rfh->GetGlobalId(), rfh->GetGlobalId(), rfh->GetStorageKey(),
        rfh->GetStorageKey().origin(), rfh->GetIsolationInfoForSubresources(),
        rfh->BuildClientSecurityState(), nullptr, nullptr,
        mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost>());
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        worker_host_->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
    broker->GetInterface(pressure_manager_.BindNewPipeAndPassReceiver());

    // Focus on the page and frame to make HasImplicitFocus() return true
    // by default.
    rfh->GetRenderWidgetHost()->Focus();
    FocusWebContentsOnMainFrame();
    task_environment()->RunUntilIdle();
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};

  mojo::Remote<blink::mojom::WebPressureManager> pressure_manager_;
  std::unique_ptr<device::ScopedPressureManagerOverrider>
      pressure_manager_overrider_;
  DedicatedWorkerServiceImpl worker_service_;
  std::unique_ptr<DedicatedWorkerHost> worker_host_;
};

TEST_F(PressureServiceForDedicatedWorkerTest, AddClient) {
  NavigateAndCommit(kTestUrl);
  SetPressureServiceForDedicatedWorker();

  FakePressureClient client;
  base::test::TestFuture<device::mojom::PressureManagerAddClientResultPtr>
      future;
  pressure_manager_->AddClient(PressureSource::kCpu, future.GetCallback());
  ASSERT_TRUE(future.Get()->is_pressure_client());
  auto result = future.Take();
  client.Bind(std::move(result->get_pressure_client()));

  const base::TimeTicks time = base::TimeTicks::Now();
  PressureUpdate update(PressureSource::kCpu, PressureState::kNominal, time);
  pressure_manager_overrider_->UpdateClients(update);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update);
}

TEST_F(PressureServiceForDedicatedWorkerTest,
       WebContentPressureManagerProxyTest) {
  NavigateAndCommit(kTestUrl);
  SetPressureServiceForDedicatedWorker();
  ASSERT_NE(worker_host_->pressure_service(), nullptr);

  auto* web_contents =
      WebContents::FromRenderFrameHost(RenderFrameHostImpl::FromID(
          worker_host_->GetAncestorRenderFrameHostId()));
  EXPECT_EQ(WebContentsPressureManagerProxy::FromWebContents(web_contents),
            nullptr);
  auto* pressure_manager_proxy =
      WebContentsPressureManagerProxy::GetOrCreate(web_contents);
  EXPECT_NE(pressure_manager_proxy, nullptr);
  EXPECT_EQ(pressure_manager_proxy,
            WebContentsPressureManagerProxy::FromWebContents(web_contents));

  EXPECT_EQ(worker_host_->pressure_service()->GetTokenFor(PressureSource::kCpu),
            std::nullopt);
  {
    auto pressure_source =
        pressure_manager_proxy->CreateVirtualPressureSourceForDevTools(
            PressureSource::kCpu,
            device::mojom::VirtualPressureSourceMetadata::New());
    EXPECT_NE(
        worker_host_->pressure_service()->GetTokenFor(PressureSource::kCpu),
        std::nullopt);
  }
  EXPECT_EQ(worker_host_->pressure_service()->GetTokenFor(PressureSource::kCpu),
            std::nullopt);
}

TEST_F(PressureServiceForDedicatedWorkerTest, PermissionsPolicyBlock) {
  // Make compute pressure blocked by permissions policy and it can only be
  // made once on page load, so we refresh the page to simulate that.
  blink::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      blink::mojom::PermissionsPolicyFeature::kComputePressure;
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(kTestUrl, main_rfh());
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  SetPressureServiceForDedicatedWorker();
  EXPECT_EQ(worker_host_->pressure_service(), nullptr);
}

class PressureServiceForSharedWorkerTest
    : public RenderViewHostImplTestHarness {
 public:
  PressureServiceForSharedWorkerTest() = default;
  ~PressureServiceForSharedWorkerTest() override = default;

  PressureServiceForSharedWorkerTest(
      const PressureServiceForSharedWorkerTest&) = delete;
  PressureServiceForSharedWorkerTest& operator=(
      const PressureServiceForSharedWorkerTest&) = delete;

  void TearDown() override {
    pressure_manager_overrider_.reset();
    worker_host_.reset();
    worker_service_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetPressureServiceForSharedWorker() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedPressureManagerOverrider>();
    pressure_manager_.reset();

    auto* rfh = contents()->GetPrimaryMainFrame();
    SharedWorkerInstance instance(
        kWorkerUrl, blink::mojom::ScriptType::kClassic,
        network::mojom::CredentialsMode::kSameOrigin, "name",
        rfh->GetStorageKey(),
        blink::mojom::SharedWorkerCreationContextType::kSecure,
        rfh->GetStorageKey().IsFirstPartyContext()
            ? blink::mojom::SharedWorkerSameSiteCookies::kAll
            : blink::mojom::SharedWorkerSameSiteCookies::kNone);
    worker_service_ = std::make_unique<SharedWorkerServiceImpl>(
        rfh->GetStoragePartition(), nullptr /* service_worker_context */);
    worker_host_ = std::make_unique<SharedWorkerHost>(
        worker_service_.get(), instance, rfh->GetSiteInstance(),
        std::vector<network::mojom::ContentSecurityPolicyPtr>(),
        base::MakeRefCounted<PolicyContainerHost>());
    AddClient(receiver_.InitWithNewPipeAndPassRemote(), rfh->GetGlobalId(),
              blink::MessagePortChannel(port_pair_.TakePort0()),
              kClientUkmSourceId);
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        worker_host_->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
    broker->GetInterface(pressure_manager_.BindNewPipeAndPassReceiver());

    // Focus on the page and frame to make HasImplicitFocus() return true
    // by default.
    rfh->GetRenderWidgetHost()->Focus();
    FocusWebContentsOnMainFrame();
    task_environment()->RunUntilIdle();
  }

  void AddClient(mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
                 GlobalRenderFrameHostId client_render_frame_host_id,
                 const blink::MessagePortChannel& port,
                 ukm::SourceId client_ukm_source_id) {
    worker_host_->AddClient(std::move(client), client_render_frame_host_id,
                            port, client_ukm_source_id);
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const GURL kWorkerUrl{"https://example.com/w.js"};
  const ukm::SourceId kClientUkmSourceId = 12345;

  mojo::Remote<blink::mojom::WebPressureManager> pressure_manager_;
  mojo::PendingReceiver<blink::mojom::SharedWorkerClient> receiver_;
  std::unique_ptr<device::ScopedPressureManagerOverrider>
      pressure_manager_overrider_;
  blink::MessagePortDescriptorPair port_pair_;
  std::unique_ptr<SharedWorkerServiceImpl> worker_service_;
  std::unique_ptr<SharedWorkerHost> worker_host_;
};

TEST_F(PressureServiceForSharedWorkerTest, AddClient) {
  NavigateAndCommit(kTestUrl);
  SetPressureServiceForSharedWorker();

  FakePressureClient client;
  base::test::TestFuture<device::mojom::PressureManagerAddClientResultPtr>
      future;
  pressure_manager_->AddClient(PressureSource::kCpu, future.GetCallback());
  ASSERT_TRUE(future.Get()->is_pressure_client());
  auto result = future.Take();
  client.Bind(std::move(result->get_pressure_client()));

  const base::TimeTicks time = base::TimeTicks::Now();
  PressureUpdate update(PressureSource::kCpu, PressureState::kNominal, time);
  pressure_manager_overrider_->UpdateClients(update);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update);
}

TEST_F(PressureServiceForSharedWorkerTest, WebContentPressureManagerProxyTest) {
  NavigateAndCommit(kTestUrl);
  SetPressureServiceForSharedWorker();
  ASSERT_NE(worker_host_->pressure_service(), nullptr);

  // Not much to test for shared workers: there is no way to retrieve a
  // WebContentsPressureManagerProxy instance since there isn't one single
  // WebContents we could use.
  EXPECT_EQ(worker_host_->pressure_service()->GetTokenFor(PressureSource::kCpu),
            std::nullopt);
}

TEST_F(PressureServiceForSharedWorkerTest, PermissionsPolicyBlock) {
  // Make compute pressure blocked by permissions policy and it can only be
  // made once on page load, so we refresh the page to simulate that.
  blink::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      blink::mojom::PermissionsPolicyFeature::kComputePressure;
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(kTestUrl, main_rfh());
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  SetPressureServiceForSharedWorker();
  EXPECT_EQ(worker_host_->pressure_service(), nullptr);
}

TEST_F(PressureServiceForSharedWorkerTest,
       ResetWhenClientWithoutPermissionsPolicy) {
  NavigateAndCommit(kTestUrl);
  SetPressureServiceForSharedWorker();
  EXPECT_NE(worker_host_->pressure_service(), nullptr);

  auto web_contents = TestWebContents::Create(browser_context(), nullptr);
  auto* rfh = web_contents->GetPrimaryMainFrame();
  blink::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      blink::mojom::PermissionsPolicyFeature::kComputePressure;
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(kTestUrl, rfh);
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  mojo::PendingReceiver<blink::mojom::SharedWorkerClient> receiver;
  blink::MessagePortDescriptorPair port_pair;
  AddClient(receiver.InitWithNewPipeAndPassRemote(), rfh->GetGlobalId(),
            blink::MessagePortChannel(port_pair.TakePort0()),
            kClientUkmSourceId);
  EXPECT_EQ(worker_host_->pressure_service(), nullptr);
}

}  // namespace content
