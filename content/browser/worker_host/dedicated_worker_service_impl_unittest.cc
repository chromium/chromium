// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_service_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/isolation_info.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

// Mocks a dedicated worker living in a renderer process.
class MockDedicatedWorker
    : public blink::mojom::DedicatedWorkerHostFactoryClient {
 public:
  MockDedicatedWorker(ChildProcessId worker_process_id,
                      GlobalRenderFrameHostId render_frame_host_id,
                      const url::Origin& origin) {
    // The COEP reporter is replaced by a placeholder connection. Reports are
    // ignored.
    auto coep_reporter = std::make_unique<CrossOriginEmbedderPolicyReporter>(
        RenderFrameHostImpl::FromID(render_frame_host_id)
            ->GetStoragePartition()
            ->GetWeakPtr(),
        GURL(), std::nullopt, std::nullopt, base::UnguessableToken::Create(),
        net::NetworkAnonymizationKey());

    mojo::MakeSelfOwnedReceiver(
        std::make_unique<DedicatedWorkerHostFactoryImpl>(
            worker_process_id, /*creator=*/render_frame_host_id,
            render_frame_host_id, blink::StorageKey::CreateFirstParty(origin),
            net::IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
            network::mojom::ClientSecurityState::New(),
            PolicyContainerPolicies(), coep_reporter->GetWeakPtr(),
            /*network_restrictions_id=*/std::nullopt),
        factory_.BindNewPipeAndPassReceiver());

    auto fetch_client_settings_object =
        blink::mojom::FetchClientSettingsObject::New();
    fetch_client_settings_object->policy_container_policies =
        blink::mojom::PolicyContainerPolicies::New();

    factory_->CreateWorkerHostAndStartScriptLoad(
        blink::DedicatedWorkerToken(),
        /*script_url=*/origin.GetURL().Resolve("worker.js"),
        network::mojom::CredentialsMode::kSameOrigin,
        std::move(fetch_client_settings_object),
        mojo::PendingRemote<blink::mojom::BlobURLToken>(),
        receiver_.BindNewPipeAndPassRemote(),
        net::StorageAccessApiStatus::kNone);
  }

  ~MockDedicatedWorker() override = default;

  // Non-copyable.
  MockDedicatedWorker(const MockDedicatedWorker& other) = delete;

  // blink::mojom::DedicatedWorkerHostFactoryClient:
  void OnWorkerHostCreated(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      mojo::PendingRemote<blink::mojom::DedicatedWorkerHost>,
      const url::Origin&) override {
    browser_interface_broker_.Bind(std::move(browser_interface_broker));
  }

  void OnScriptLoadStarted(
      blink::mojom::ServiceWorkerContainerInfoForClientPtr
          service_worker_container_info,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          pending_subresource_loader_factory_bundle,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          subresource_loader_updater,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojo::PendingRemote<blink::mojom::BackForwardCacheControllerHost>
          back_forward_cache_controller_host,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          coep_reporting_observer,
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          dip_reporting_observer) override {}
  void OnScriptLoadStartFailed() override {}

 private:
  mojo::Receiver<blink::mojom::DedicatedWorkerHostFactoryClient> receiver_{
      this};

  // Allows creating the dedicated worker host.
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory_;

  mojo::Remote<blink::mojom::BrowserInterfaceBroker> browser_interface_broker_;
  mojo::Remote<blink::mojom::DedicatedWorkerHost> remote_host_;
};

class DedicatedWorkerServiceImplTest
    : public RenderViewHostImplTestHarness {
 public:
  DedicatedWorkerServiceImplTest() = default;
  ~DedicatedWorkerServiceImplTest() override = default;

  // Non-copyable.
  DedicatedWorkerServiceImplTest(const DedicatedWorkerServiceImplTest& other) =
      delete;
  DedicatedWorkerServiceImplTest& operator=(
      const DedicatedWorkerServiceImplTest& other) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    browser_context_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

  DedicatedWorkerService* GetDedicatedWorkerService() const {
    return browser_context_->GetDefaultStoragePartition()
        ->GetDedicatedWorkerService();
  }

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
};

class TestDedicatedWorkerServiceObserver
    : public DedicatedWorkerService::Observer {
 public:
  struct DedicatedWorkerInfo {
    ChildProcessId worker_process_id;
    url::Origin origin;
    DedicatedWorkerCreator creator;

    bool operator==(const DedicatedWorkerInfo& other) const {
      return std::tie(worker_process_id, origin, creator) ==
             std::tie(other.worker_process_id, other.origin, other.creator);
    }
  };

  TestDedicatedWorkerServiceObserver() = default;
  ~TestDedicatedWorkerServiceObserver() override = default;

  // Non-copyable.
  TestDedicatedWorkerServiceObserver(
      const TestDedicatedWorkerServiceObserver& other) = delete;

  // DedicatedWorkerService::Observer:
  void OnWorkerCreated(const blink::DedicatedWorkerToken& token,
                       ChildProcessId worker_process_id,
                       const url::Origin& security_origin,
                       DedicatedWorkerCreator creator) override {
    bool inserted =
        dedicated_worker_infos_
            .emplace(token, DedicatedWorkerInfo{worker_process_id,
                                                security_origin, creator})
            .second;
    DCHECK(inserted);

    if (on_worker_event_callback_)
      std::move(on_worker_event_callback_).Run();
  }
  void OnBeforeWorkerDestroyed(const blink::DedicatedWorkerToken& token,
                               DedicatedWorkerCreator creator) override {
    size_t removed = dedicated_worker_infos_.erase(token);
    DCHECK_EQ(removed, 1u);

    if (on_worker_event_callback_)
      std::move(on_worker_event_callback_).Run();
  }
  void OnFinalResponseURLDetermined(const blink::DedicatedWorkerToken& token,
                                    const GURL& url) override {}

  void RunUntilWorkerEvent() {
    base::RunLoop run_loop;
    on_worker_event_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const base::flat_map<blink::DedicatedWorkerToken, DedicatedWorkerInfo>&
  dedicated_worker_infos() const {
    return dedicated_worker_infos_;
  }

 private:
  // Used to wait until one of OnWorkerStarted() or OnBeforeWorkerTerminated()
  // is called.
  base::OnceClosure on_worker_event_callback_;

  base::flat_map<blink::DedicatedWorkerToken, DedicatedWorkerInfo>
      dedicated_worker_infos_;
};

TEST_F(DedicatedWorkerServiceImplTest, DedicatedWorkerServiceObserver) {
  // Set up the observer.
  TestDedicatedWorkerServiceObserver observer;
  base::ScopedObservation<DedicatedWorkerService,
                          DedicatedWorkerService::Observer>
      scoped_dedicated_worker_service_observation_(&observer);
  scoped_dedicated_worker_service_observation_.Observe(
      GetDedicatedWorkerService());

  const GURL kUrl("http://example.com/");
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents(kUrl);
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();

  // At first, there is no live dedicated worker.
  EXPECT_TRUE(observer.dedicated_worker_infos().empty());

  // Create the dedicated worker.
  const DedicatedWorkerCreator creator(render_frame_host->GetGlobalId());
  const ChildProcessId render_process_host_id =
      render_frame_host->GetProcess()->GetID();
  const auto origin = url::Origin::Create(kUrl);
  auto mock_dedicated_worker = std::make_unique<MockDedicatedWorker>(
      render_process_host_id, render_frame_host->GetGlobalId(), origin);
  observer.RunUntilWorkerEvent();

  // The service sent a OnWorkerStarted() notification.
  {
    ASSERT_EQ(observer.dedicated_worker_infos().size(), 1u);
    const auto& dedicated_worker_info =
        observer.dedicated_worker_infos().begin()->second;
    EXPECT_EQ(dedicated_worker_info.worker_process_id, render_process_host_id);
    EXPECT_EQ(dedicated_worker_info.creator, creator);
    EXPECT_EQ(dedicated_worker_info.origin, origin);
  }

  // Test EnumerateDedicatedWorkers().
  {
    TestDedicatedWorkerServiceObserver enumeration_observer;
    EXPECT_TRUE(enumeration_observer.dedicated_worker_infos().empty());

    GetDedicatedWorkerService()->EnumerateDedicatedWorkers(
        &enumeration_observer);

    ASSERT_EQ(enumeration_observer.dedicated_worker_infos().size(), 1u);
    const auto& dedicated_worker_info =
        enumeration_observer.dedicated_worker_infos().begin()->second;
    EXPECT_EQ(dedicated_worker_info.worker_process_id, render_process_host_id);
    EXPECT_EQ(dedicated_worker_info.creator, creator);
    EXPECT_EQ(dedicated_worker_info.origin, origin);
  }

  // Delete the dedicated worker.
  mock_dedicated_worker = nullptr;
  observer.RunUntilWorkerEvent();

  // The service sent a OnBeforeWorkerTerminated() notification.
  EXPECT_TRUE(observer.dedicated_worker_infos().empty());
}

class DedicatedWorkerHostFactoryImplTest
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }
};

TEST_F(DedicatedWorkerHostFactoryImplTest, CrossOriginScriptOriginCheck) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("isolated-app", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  const GURL kIwaAppA(
      "isolated-app://"
      "aerugqztij5biu7uk3mi3no76snn7762675v7vsyof772itbv7id2yyd");
  const url::Origin kIwaOriginA = url::Origin::Create(kIwaAppA);
  const GURL kIwaAppB(
      "isolated-app://"
      "berugqztij5biu7uk3mi3no76snn7762675v7vsyof772itbv7id2yyd");

  const GURL kExtAppA(
      "chrome-extension://"
      "aerugqztij5biu7uk3mi3no76snn7762675v7vsyof772itbv7id2yyd");
  const url::Origin kExtOriginA = url::Origin::Create(kExtAppA);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), kIwaAppA);
  RenderFrameHost* creator_rfh = web_contents()->GetPrimaryMainFrame();

  auto create_factory =
      [&](mojo::Remote<blink::mojom::DedicatedWorkerHostFactory>& factory,
          const url::Origin& creator_origin) {
        auto coep_reporter =
            std::make_unique<CrossOriginEmbedderPolicyReporter>(
                static_cast<StoragePartitionImpl*>(
                    creator_rfh->GetStoragePartition())
                    ->GetWeakPtr(),
                GURL(), std::nullopt, std::nullopt,
                base::UnguessableToken::Create(),
                net::NetworkAnonymizationKey());

        return std::make_unique<DedicatedWorkerHostFactoryImpl>(
            creator_rfh->GetProcess()->GetID(),
            static_cast<RenderFrameHostImpl*>(creator_rfh)->GetGlobalId(),
            static_cast<RenderFrameHostImpl*>(creator_rfh)->GetGlobalId(),
            blink::StorageKey::CreateFirstParty(creator_origin),
            net::IsolationInfo::CreateTransient(std::nullopt),
            network::mojom::ClientSecurityState::New(),
            PolicyContainerPolicies(), coep_reporter->GetWeakPtr(),
            /*network_restrictions_id=*/std::nullopt);
      };

  auto start_script_load =
      [](mojo::Remote<blink::mojom::DedicatedWorkerHostFactory>& factory,
         const GURL& script_url) {
        auto fetch_client_settings_object =
            blink::mojom::FetchClientSettingsObject::New();
        fetch_client_settings_object->policy_container_policies =
            blink::mojom::PolicyContainerPolicies::New();

        mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient>
            client_remote;
        std::ignore = client_remote.InitWithNewPipeAndPassReceiver();

        factory->CreateWorkerHostAndStartScriptLoad(
            blink::DedicatedWorkerToken(), script_url,
            network::mojom::CredentialsMode::kSameOrigin,
            std::move(fetch_client_settings_object),
            mojo::PendingRemote<blink::mojom::BlobURLToken>(),
            std::move(client_remote), net::StorageAccessApiStatus::kNone);
      };

  // Flag OFF: Cross-origin script URL should NOT cause a bad message.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kEnforceDedicatedWorkerSameOriginCheck);

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    auto factory_impl = create_factory(factory, kIwaOriginA);
    mojo::Receiver<blink::mojom::DedicatedWorkerHostFactory> receiver(
        factory_impl.get(), factory.BindNewPipeAndPassReceiver());

    mojo::test::BadMessageObserver bad_message_observer;
    start_script_load(factory, kIwaAppB);
    factory.FlushForTesting();

    EXPECT_FALSE(bad_message_observer.got_bad_message());
  }

  // Flag ON: Cross-origin script URL for IWA should cause a bad message.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kEnforceDedicatedWorkerSameOriginCheck);

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    auto factory_impl = create_factory(factory, kIwaOriginA);
    mojo::Receiver<blink::mojom::DedicatedWorkerHostFactory> receiver(
        factory_impl.get(), factory.BindNewPipeAndPassReceiver());

    mojo::test::BadMessageObserver bad_message_observer;
    start_script_load(factory, kIwaAppB);
    factory.FlushForTesting();

    EXPECT_EQ("DWH_INVALID_SCRIPT_URL_ORIGIN",
              bad_message_observer.WaitForBadMessage());
  }

  // Flag ON: Cross-origin script URL for Extensions should cause a bad message.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kEnforceDedicatedWorkerSameOriginCheck);

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    auto factory_impl = create_factory(factory, kExtOriginA);
    mojo::Receiver<blink::mojom::DedicatedWorkerHostFactory> receiver(
        factory_impl.get(), factory.BindNewPipeAndPassReceiver());

    mojo::test::BadMessageObserver bad_message_observer;
    // Cross-origin load (even to https) should be blocked for extensions now.
    start_script_load(factory, GURL("https://example.com/worker.js"));
    factory.FlushForTesting();

    EXPECT_EQ("DWH_INVALID_SCRIPT_URL_ORIGIN",
              bad_message_observer.WaitForBadMessage());
  }

  // Flag ON: Same-origin script URL should NOT cause a bad message.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kEnforceDedicatedWorkerSameOriginCheck);

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    auto factory_impl = create_factory(factory, kIwaOriginA);
    mojo::Receiver<blink::mojom::DedicatedWorkerHostFactory> receiver(
        factory_impl.get(), factory.BindNewPipeAndPassReceiver());

    mojo::test::BadMessageObserver bad_message_observer;
    start_script_load(factory, kIwaAppA);
    factory.FlushForTesting();

    EXPECT_FALSE(bad_message_observer.got_bad_message());
  }

  // Flag ON: Data URL script should NOT cause a bad message (exceptions are
  // allowed).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kEnforceDedicatedWorkerSameOriginCheck);

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    auto factory_impl = create_factory(factory, kIwaOriginA);
    mojo::Receiver<blink::mojom::DedicatedWorkerHostFactory> receiver(
        factory_impl.get(), factory.BindNewPipeAndPassReceiver());

    mojo::test::BadMessageObserver bad_message_observer;
    start_script_load(factory, GURL("data:text/javascript,console.log('hi')"));
    factory.FlushForTesting();

    EXPECT_FALSE(bad_message_observer.got_bad_message());
  }

  // Flag ON: Opaque origin creator should NOT be blocked for same-site-ish
  // URLs, to avoid breaking sandboxed iframes etc. (unless it's an IWA/Ext).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kEnforceDedicatedWorkerSameOriginCheck);

    const GURL kOpaqueUrl("data:text/html,<html></html>");
    const url::Origin kOpaqueOrigin = url::Origin::Create(kOpaqueUrl);
    EXPECT_TRUE(kOpaqueOrigin.opaque());

    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                      kOpaqueUrl);
    RenderFrameHost* opaque_rfh = web_contents()->GetPrimaryMainFrame();

    auto create_opaque_factory =
        [&](mojo::Remote<blink::mojom::DedicatedWorkerHostFactory>& factory) {
          auto coep_reporter =
              std::make_unique<CrossOriginEmbedderPolicyReporter>(
                  static_cast<StoragePartitionImpl*>(
                      opaque_rfh->GetStoragePartition())
                      ->GetWeakPtr(),
                  GURL(), std::nullopt, std::nullopt,
                  base::UnguessableToken::Create(),
                  net::NetworkAnonymizationKey());

          return std::make_unique<DedicatedWorkerHostFactoryImpl>(
              opaque_rfh->GetProcess()->GetID(),
              static_cast<RenderFrameHostImpl*>(opaque_rfh)->GetGlobalId(),
              static_cast<RenderFrameHostImpl*>(opaque_rfh)->GetGlobalId(),
              blink::StorageKey::CreateFirstParty(kOpaqueOrigin),
              net::IsolationInfo::CreateTransient(std::nullopt),
              network::mojom::ClientSecurityState::New(),
              PolicyContainerPolicies(), coep_reporter->GetWeakPtr(),
              /*network_restrictions_id=*/std::nullopt);
        };

    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    auto factory_impl = create_opaque_factory(factory);
    mojo::Receiver<blink::mojom::DedicatedWorkerHostFactory> receiver(
        factory_impl.get(), factory.BindNewPipeAndPassReceiver());

    mojo::test::BadMessageObserver bad_message_observer;
    // Attempt to load a script that would normally be same-origin to the
    // precursor (https://example.com). This should be allowed because
    // neither side is an IWA or Extension.
    start_script_load(factory, GURL("https://example.com/worker.js"));
    factory.FlushForTesting();

    EXPECT_FALSE(bad_message_observer.got_bad_message());
  }
}

}  // namespace content
