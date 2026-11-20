// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_loader_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "content/browser/isolation_context.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/process_lock.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_info.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"

namespace content {

class WorkerScriptLoaderFactoryTest : public testing::Test {
 public:
  WorkerScriptLoaderFactoryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~WorkerScriptLoaderFactoryTest() override = default;

  void SetUp() override {
    // Set up the service worker system.
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());

    // Lock the worker process to the test site and record the origin as
    // committed in it, as for a real renderer that created the worker, so
    // that ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin() lets the
    // worker match its origin's service worker registrations.
    const GURL kOriginUrl("https://www.example.com");
    IsolationContext isolation_context(
        BrowsingInstanceId(1), helper_->browser_context(),
        /*is_guest=*/false, /*is_fenced=*/false,
        OriginAgentClusterIsolationState::CreateNonIsolatedByDefault());
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    policy->LockProcessForTesting(
        isolation_context, helper_->mock_render_process_id(), kOriginUrl);
    policy->AddCommittedOrigin(
        helper_->mock_render_process_id().GetUnsafeValue(),
        url::Origin::Create(kOriginUrl));

    browser_context_getter_ =
        base::BindRepeating(&ServiceWorkerContextWrapper::browser_context,
                            helper_->context_wrapper());

    // Set up the network factory.
    network_loader_factory_instance_ =
        std::make_unique<FakeNetworkURLLoaderFactory>();
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory;
    network_loader_factory_instance_->Clone(
        factory.InitWithNewPipeAndPassReceiver());
    auto info = std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
        std::move(factory));
    network_loader_factory_ =
        network::SharedURLLoaderFactory::Create(std::move(info));

    // Set up a service worker host for the shared worker.
    service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
        helper_->context_wrapper(), base::DoNothing(),
        /*fetch_event_client_id=*/"");
  }

 protected:
  struct RegisteredDedicatedWorkerHost {
    blink::DedicatedWorkerToken token;
    std::unique_ptr<DedicatedWorkerHost> host;
    mojo::Remote<blink::mojom::DedicatedWorkerHost> remote;
  };

  RegisteredDedicatedWorkerHost CreateRegisteredDedicatedWorkerHost(
      const url::Origin& origin,
      const blink::StorageKey& storage_key) {
    EXPECT_TRUE(helper_->mock_render_process_host()->Init());
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        helper_->mock_render_process_host()->GetStoragePartition());
    RegisteredDedicatedWorkerHost registered;
    registered.host = std::make_unique<DedicatedWorkerHost>(
        static_cast<DedicatedWorkerServiceImpl*>(
            storage_partition->GetDedicatedWorkerService()),
        registered.token, helper_->mock_render_process_host(),
        DedicatedWorkerCreator(GlobalRenderFrameHostId()), WeakDocumentPtr(),
        origin, storage_key, origin,
        net::IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        network::mojom::ClientSecurityState::New(), PolicyContainerPolicies(),
        /*creator_coep_reporter=*/nullptr, base::UnguessableToken::Create(),
        registered.remote.BindNewPipeAndPassReceiver(),
        net::StorageAccessApiStatus::kNone);
    return registered;
  }

  mojo::PendingRemote<network::mojom::URLLoader> CreateTestLoaderAndStart(
      const GURL& url,
      WorkerScriptLoaderFactory* factory,
      network::TestURLLoaderClient* client) {
    mojo::PendingRemote<network::mojom::URLLoader> loader;
    network::ResourceRequest resource_request;
    resource_request.url = url;
    resource_request.trusted_params = network::ResourceRequest::TrustedParams();
    resource_request.trusted_params->isolation_info =
        net::IsolationInfo::Create(
            net::IsolationInfo::RequestType::kOther, url::Origin::Create(url),
            url::Origin::Create(url), net::SiteForCookies());
    resource_request.resource_type =
        static_cast<int>(blink::mojom::ResourceType::kSharedWorker);
    resource_request.destination =
        network::mojom::RequestDestination::kSharedWorker;
    factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request,
        client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return loader;
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::unique_ptr<FakeNetworkURLLoaderFactory> network_loader_factory_instance_;
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;
  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  WorkerScriptLoaderFactory::BrowserContextGetter browser_context_getter_;
};

TEST_F(WorkerScriptLoaderFactoryTest, ServiceWorkerContainerHost) {
  GURL url("https://www.example.com/worker.js");

  // Make the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  // Load the script.
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  base::RunLoop().RunUntilIdle();

  // `SetExecutionReady()` should wait for `OnFetcherCallbackCalled()`.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      service_worker_handle_->service_worker_client();
  EXPECT_FALSE(service_worker_client->is_response_committed());
  EXPECT_FALSE(service_worker_client->is_execution_ready());

  // Emulate CommitResponse() and SetContainerReady() calls that would happen
  // inside `WorkerScriptFetcher::callback_`.
  auto container_info =
      service_worker_handle_->scoped_service_worker_client()
          ->CommitResponseAndRelease(
              /*rfh_id=*/std::nullopt, PolicyContainerPolicies(),
              /*coep_reporter=*/{}, /*dip_reporter=*/{}, ukm::kInvalidSourceId);
  (*service_worker_handle_->scoped_service_worker_client())
      ->SetContainerReady();
  factory->GetScriptLoader()->OnFetcherCallbackCalled();
  client.RunUntilComplete();

  EXPECT_EQ(net::OK, client.completion_status().error_code);

  // The container host should be set up.
  EXPECT_TRUE(service_worker_client->is_response_committed());
  EXPECT_TRUE(service_worker_client->is_execution_ready());
  EXPECT_EQ(url, service_worker_client->url());
}

// An opaque worker StorageKey must not create a ServiceWorkerClient (and so
// cannot match the precursor origin's registrations).
TEST_F(WorkerScriptLoaderFactoryTest,
       NoServiceWorkerClientForOpaqueStorageKey) {
  GURL url("https://www.example.com/worker.js");
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(url).DeriveNewOpaqueOrigin()),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  base::RunLoop().RunUntilIdle();

  // No ServiceWorkerClient was created, so the script is fetched from the
  // network without service worker interception.
  EXPECT_FALSE(service_worker_handle_->service_worker_client());
  factory->GetScriptLoader()->OnFetcherCallbackCalled();
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

// A process that cannot access the worker origin's data (here, a
// sandboxed-frame process) must not create a ServiceWorkerClient either.
TEST_F(WorkerScriptLoaderFactoryTest,
       NoServiceWorkerClientForProcessDeniedDataAccess) {
  GURL url("https://www.example.com/worker.js");
  MockRenderProcessHost sandboxed_process(helper_->browser_context());
  sandboxed_process.Init();
  IsolationContext isolation_context(
      BrowsingInstanceId(1), helper_->browser_context(),
      /*is_guest=*/false, /*is_fenced=*/false,
      OriginAgentClusterIsolationState::CreateNonIsolatedByDefault());
  sandboxed_process.SetProcessLock(ProcessLock::Create(
      isolation_context, UrlInfo(UrlInfoInit(GURL("https://www.example.com"))
                                     .WithSandbox(true)
                                     .WithStoragePartitionConfig(
                                         StoragePartitionConfig::CreateDefault(
                                             helper_->browser_context())))));
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      sandboxed_process.GetID(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  base::RunLoop().RunUntilIdle();

  // No ServiceWorkerClient was created, so the script is fetched from the
  // network without service worker interception.
  EXPECT_FALSE(service_worker_handle_->service_worker_client());
  factory->GetScriptLoader()->OnFetcherCallbackCalled();
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

// Test a null service worker handle. This typically only happens during
// shutdown or after a fatal error occurred in the service worker system.
TEST_F(WorkerScriptLoaderFactoryTest, NullServiceWorkerHandle) {
  GURL url("https://www.example.com/worker.js");

  // Make the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  // Destroy the handle.
  service_worker_handle_.reset();
  // Let the IO thread task run to destroy the handle core.
  base::RunLoop().RunUntilIdle();

  // Load the script.
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// Test a null browser context when the request starts. This happens when
// shutdown starts between the constructor and when CreateLoaderAndStart is
// invoked.
TEST_F(WorkerScriptLoaderFactoryTest, NullBrowserContext) {
  GURL url("https://www.example.com/worker.js");

  // Make the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  // Set a null browser context.
  helper_->context_wrapper()->Shutdown();

  // Load the script.
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// Tests that a redirect received while loading a blob: URL is rejected. Loading
// a blob URL never produces a redirect, so any redirect must be rejected before
// it is forwarded or followed.
TEST_F(WorkerScriptLoaderFactoryTest, RedirectFromBlobUrl) {
  GURL url("blob:https://www.example.com/49146318-7a89-4041-9bcc-36e6b6eeef86");

  // Defer the mock network load so we can inject a redirect on the in-flight
  // load.
  network_loader_factory_instance_->DeferHandleRequest();

  // Create the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  // Start loading the script.
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory->GetScriptLoader() != nullptr; }));

  // Simulate receiving a redirect from the blob load.
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = GURL("https://other.example.com/worker.js");
  factory->GetScriptLoader()->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());
  client.RunUntilComplete();

  // Verify that the redirect was blocked and the load was aborted with
  // ERR_UNSAFE_REDIRECT.
  EXPECT_FALSE(client.has_received_redirect());
  ASSERT_TRUE(client.has_received_completion());
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, client.completion_status().error_code);
}

// Tests that response headers received while loading a blob: URL are sanitized.
// Loading a blob URL never produces service worker interception or network
// address space metadata, so any unexpected fields must be reset before they
// are forwarded to the client.
TEST_F(WorkerScriptLoaderFactoryTest, ResponseFromBlobUrl) {
  GURL url("blob:https://www.example.com/49146318-7a89-4041-9bcc-36e6b6eeef86");

  // Defer the mock network load so we can inject a response on the in-flight
  // load.
  network_loader_factory_instance_->DeferHandleRequest();

  // Create the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  // Start loading the script.
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory->GetScriptLoader() != nullptr; }));

  // Simulate receiving a response from the blob load with unexpected fields.
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->was_fetched_via_service_worker = true;
  response_head->url_list_via_service_worker = {
      GURL("https://other.example.com/worker.js")};
  response_head->client_address_space =
      network::mojom::IPAddressSpace::kLoopback;
  response_head->response_address_space =
      network::mojom::IPAddressSpace::kLoopback;
  response_head->remote_endpoint =
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 8080);
  response_head->was_fetched_via_cache = true;
  response_head->is_validated = true;

  factory->GetScriptLoader()->OnReceiveResponse(
      std::move(response_head), mojo::ScopedDataPipeConsumerHandle(),
      std::nullopt);
  client.RunUntilResponseReceived();

  // Verify that the unexpected fields were sanitized.
  ASSERT_TRUE(client.has_received_response());
  EXPECT_FALSE(client.response_head()->was_fetched_via_service_worker);
  EXPECT_TRUE(client.response_head()->url_list_via_service_worker.empty());
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            client.response_head()->client_address_space);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            client.response_head()->response_address_space);
  EXPECT_EQ(net::IPEndPoint(), client.response_head()->remote_endpoint);
  factory->GetScriptLoader()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  factory->GetScriptLoader()->OnFetcherCallbackCalled();
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

// Tests that Early Hints received while loading a blob: URL are ignored.
TEST_F(WorkerScriptLoaderFactoryTest, EarlyHintsFromBlobUrl) {
  GURL url("blob:https://www.example.com/49146318-7a89-4041-9bcc-36e6b6eeef86");

  // Defer the mock network load so we can inject an early hint on the in-flight
  // load.
  network_loader_factory_instance_->DeferHandleRequest();

  // Create the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  // Start loading the script.
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory->GetScriptLoader() != nullptr; }));

  auto hints = network::mojom::EarlyHints::New();
  hints->headers = network::mojom::ParsedHeaders::New();
  hints->ip_address_space = network::mojom::IPAddressSpace::kLoopback;
  factory->GetScriptLoader()->OnReceiveEarlyHints(std::move(hints));
  factory->GetScriptLoader()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  factory->GetScriptLoader()->OnFetcherCallbackCalled();
  client.RunUntilComplete();

  EXPECT_FALSE(client.has_received_early_hints());
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

// Tests that a redirect to an unsafe target scheme is rejected.
TEST_F(WorkerScriptLoaderFactoryTest, RejectUnsafeRedirectTarget) {
  GURL url("https://www.example.com/worker.js");

  network_loader_factory_instance_->DeferHandleRequest();

  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url)),
      service_worker_handle_.get(), browser_context_getter_,
      network_loader_factory_);

  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory->GetScriptLoader() != nullptr; }));

  // Simulate receiving an unsafe redirect (e.g. to a file scheme).
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = GURL("file:///path/to/worker.js");
  factory->GetScriptLoader()->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());
  client.RunUntilComplete();

  // Verify that the redirect was blocked and the load was aborted with
  // ERR_UNSAFE_REDIRECT.
  EXPECT_FALSE(client.has_received_redirect());
  ASSERT_TRUE(client.has_received_completion());
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, client.completion_status().error_code);
}

TEST_F(WorkerScriptLoaderFactoryTest, BlobUrlWorkerParentOriginMismatch) {
  const GURL parent_url("https://other.example.com/index.html");
  const url::Origin parent_origin = url::Origin::Create(parent_url);
  ChildProcessSecurityPolicyImpl::GetInstance()->AddCommittedOrigin(
      helper_->mock_render_process_id().GetUnsafeValue(), parent_origin);
  ScopedServiceWorkerClient parent_client =
      helper_->context()
          ->service_worker_client_owner()
          .CreateServiceWorkerClientForWindow(/*are_ancestors_secure=*/true,
                                              FrameTreeNodeId());
  parent_client->UpdateUrls(parent_url, parent_origin,
                            blink::StorageKey::CreateFirstParty(parent_origin));

  // A blob URL from a different origin than `parent_client` must not create a
  // service worker client or inherit `parent_client`'s storage key/controller.
  const GURL different_origin_blob_url(
      "blob:https://www.example.com/49146318-7a89-4041-9bcc-36e6b6eeef86");
  const url::Origin different_blob_origin =
      url::Origin::Create(different_origin_blob_url);
  auto mismatched_handle = std::make_unique<ServiceWorkerMainResourceHandle>(
      helper_->context_wrapper(), base::DoNothing(),
      parent_client->client_uuid(), parent_client.AsWeakPtr());
  auto mismatched_factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(different_blob_origin),
      net::IsolationInfo::CreateForInternalRequest(different_blob_origin),
      mismatched_handle.get(), browser_context_getter_,
      network_loader_factory_);

  network::TestURLLoaderClient mismatched_client;
  mojo::PendingRemote<network::mojom::URLLoader> mismatched_loader =
      CreateTestLoaderAndStart(different_origin_blob_url,
                               mismatched_factory.get(), &mismatched_client);
  mismatched_client.RunUntilResponseReceived();
  EXPECT_FALSE(mismatched_handle->service_worker_client());

  // Positive control: a blob URL with the same origin as `parent_client`
  // creates a service worker client and inherits `parent_client`'s key.
  const GURL same_origin_blob_url(
      "blob:https://other.example.com/49146318-7a89-4041-9bcc-36e6b6eeef86");
  auto matching_handle = std::make_unique<ServiceWorkerMainResourceHandle>(
      helper_->context_wrapper(), base::DoNothing(),
      parent_client->client_uuid(), parent_client.AsWeakPtr());
  auto matching_factory = std::make_unique<WorkerScriptLoaderFactory>(
      helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
      blink::StorageKey::CreateFirstParty(parent_origin),
      net::IsolationInfo::CreateForInternalRequest(parent_origin),
      matching_handle.get(), browser_context_getter_, network_loader_factory_);

  network::TestURLLoaderClient matching_client;
  mojo::PendingRemote<network::mojom::URLLoader> matching_loader =
      CreateTestLoaderAndStart(same_origin_blob_url, matching_factory.get(),
                               &matching_client);
  matching_client.RunUntilResponseReceived();
  ASSERT_TRUE(matching_handle->service_worker_client());
  EXPECT_EQ(parent_client->key(),
            matching_handle->service_worker_client()->key());
}

// Tests that a cross-origin worker script URL does not create a
// ServiceWorkerClient for either dedicated or shared workers.
TEST_F(WorkerScriptLoaderFactoryTest, CrossOriginScriptSkipsServiceWorker) {
  GURL worker_origin_url("https://www.example.com/");
  GURL cross_origin_script_url("https://other.example.com/worker.js");

  for (const DedicatedOrSharedWorkerToken& worker_token :
       {DedicatedOrSharedWorkerToken(blink::DedicatedWorkerToken()),
        DedicatedOrSharedWorkerToken(blink::SharedWorkerToken())}) {
    auto handle = std::make_unique<ServiceWorkerMainResourceHandle>(
        helper_->context_wrapper(), base::DoNothing(),
        /*fetch_event_client_id=*/"");
    auto factory = std::make_unique<WorkerScriptLoaderFactory>(
        helper_->mock_render_process_id(), worker_token,
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(worker_origin_url)),
        net::IsolationInfo::CreateForInternalRequest(
            url::Origin::Create(worker_origin_url)),
        handle.get(), browser_context_getter_, network_loader_factory_);

    network::TestURLLoaderClient client;
    mojo::PendingRemote<network::mojom::URLLoader> loader =
        CreateTestLoaderAndStart(cross_origin_script_url, factory.get(),
                                 &client);
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(handle->service_worker_client());
    factory->GetScriptLoader()->OnFetcherCallbackCalled();
    client.RunUntilComplete();
    EXPECT_EQ(net::OK, client.completion_status().error_code);
  }
}

// Tests that a cross-origin redirect on a worker script load discards the
// reserved ServiceWorkerClient while keeping the handle alive for fallback,
// and remains cleared across subsequent redirect hops.
TEST_F(WorkerScriptLoaderFactoryTest,
       CrossOriginRedirectResetsServiceWorkerClient) {
  GURL url("https://www.example.com/worker.js");
  const url::Origin origin = url::Origin::Create(url);
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(origin);

  RegisteredDedicatedWorkerHost registered_host =
      CreateRegisteredDedicatedWorkerHost(origin, storage_key);

  network_loader_factory_instance_->DeferHandleRequest();

  for (const DedicatedOrSharedWorkerToken& worker_token :
       {DedicatedOrSharedWorkerToken(registered_host.token),
        DedicatedOrSharedWorkerToken(blink::SharedWorkerToken())}) {
    auto handle = std::make_unique<ServiceWorkerMainResourceHandle>(
        helper_->context_wrapper(), base::DoNothing(),
        /*fetch_event_client_id=*/"");
    auto factory = std::make_unique<WorkerScriptLoaderFactory>(
        helper_->mock_render_process_id(), worker_token, storage_key,
        net::IsolationInfo::CreateForInternalRequest(origin), handle.get(),
        browser_context_getter_, network_loader_factory_);

    network::TestURLLoaderClient client;
    mojo::PendingRemote<network::mojom::URLLoader> remote_loader =
        CreateTestLoaderAndStart(url, factory.get(), &client);
    mojo::Remote<network::mojom::URLLoader> loader(std::move(remote_loader));
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return factory->GetScriptLoader() != nullptr; }));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(handle->service_worker_client());

    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL("https://other.example.com/worker.js");
    factory->GetScriptLoader()->OnReceiveRedirect(
        redirect_info, network::mojom::URLResponseHead::New());
    client.RunUntilRedirectReceived();
    ASSERT_TRUE(client.has_received_redirect());

    loader->FollowRedirect(/*headers_update_params=*/{},
                           /*new_url=*/std::nullopt);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(handle);
    EXPECT_FALSE(handle->service_worker_client());

    // Even if a subsequent redirect hop returns to the original origin, the
    // discarded client must not be resurrected or crash InitializeForRequest.
    client.ClearHasReceivedRedirect();
    net::RedirectInfo redirect_back;
    redirect_back.status_code = 302;
    redirect_back.new_method = "GET";
    redirect_back.new_url = GURL("https://www.example.com/final.js");
    factory->GetScriptLoader()->OnReceiveRedirect(
        redirect_back, network::mojom::URLResponseHead::New());
    client.RunUntilRedirectReceived();
    loader->FollowRedirect(/*headers_update_params=*/{},
                           /*new_url=*/std::nullopt);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(handle);
    EXPECT_FALSE(handle->service_worker_client());
  }
}

// Tests that `--disable-web-security` allows cross-origin HTTP(S) worker
// scripts to create a ServiceWorkerClient, while still rejecting cross-origin
// `blob:` worker scripts.
TEST_F(WorkerScriptLoaderFactoryTest,
       DisableWebSecurityAllowsCrossOriginHttpButSkipsCrossOriginBlob) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableWebSecurity);

  GURL worker_origin_url("https://www.example.com/");
  GURL cross_origin_http_url("https://other.example.com/worker.js");
  GURL cross_origin_blob_url(
      "blob:https://other.example.com/49146318-7a89-4041-9bcc-36e6b6eeef86");

  // Cross-origin HTTP(S) script is allowed under `--disable-web-security`.
  {
    const url::Origin worker_origin = url::Origin::Create(worker_origin_url);
    const blink::StorageKey worker_storage_key =
        blink::StorageKey::CreateFirstParty(worker_origin);
    RegisteredDedicatedWorkerHost registered_host =
        CreateRegisteredDedicatedWorkerHost(worker_origin, worker_storage_key);

    auto factory = std::make_unique<WorkerScriptLoaderFactory>(
        helper_->mock_render_process_id(),
        DedicatedOrSharedWorkerToken(registered_host.token), worker_storage_key,
        net::IsolationInfo::CreateForInternalRequest(worker_origin),
        service_worker_handle_.get(), browser_context_getter_,
        network_loader_factory_);

    network::TestURLLoaderClient client;
    mojo::PendingRemote<network::mojom::URLLoader> loader =
        CreateTestLoaderAndStart(cross_origin_http_url, factory.get(), &client);
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(service_worker_handle_->service_worker_client());
    EXPECT_EQ(blink::StorageKey::CreateFirstParty(
                  url::Origin::Create(cross_origin_http_url)),
              service_worker_handle_->service_worker_client()->key());
    factory->GetScriptLoader()->OnFetcherCallbackCalled();
    client.RunUntilComplete();
    EXPECT_EQ(net::OK, client.completion_status().error_code);
  }

  // Cross-origin `blob:` script is still rejected even with
  // `--disable-web-security` and a parent ServiceWorkerClient.
  {
    const url::Origin cross_origin = url::Origin::Create(cross_origin_http_url);
    ScopedServiceWorkerClient parent_client =
        helper_->context()
            ->service_worker_client_owner()
            .CreateServiceWorkerClientForWindow(
                /*are_ancestors_secure=*/true, FrameTreeNodeId());
    parent_client->UpdateUrls(
        cross_origin_http_url, cross_origin,
        blink::StorageKey::CreateFirstParty(cross_origin));
    auto blob_handle = std::make_unique<ServiceWorkerMainResourceHandle>(
        helper_->context_wrapper(), base::DoNothing(),
        /*fetch_event_client_id=*/"", parent_client.AsWeakPtr());

    auto factory = std::make_unique<WorkerScriptLoaderFactory>(
        helper_->mock_render_process_id(), DedicatedOrSharedWorkerToken(),
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(worker_origin_url)),
        net::IsolationInfo::CreateForInternalRequest(
            url::Origin::Create(worker_origin_url)),
        blob_handle.get(), browser_context_getter_, network_loader_factory_);

    network::TestURLLoaderClient client;
    mojo::PendingRemote<network::mojom::URLLoader> loader =
        CreateTestLoaderAndStart(cross_origin_blob_url, factory.get(), &client);
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(blob_handle->service_worker_client());
    factory->GetScriptLoader()->OnFetcherCallbackCalled();
    client.RunUntilComplete();
    EXPECT_EQ(net::OK, client.completion_status().error_code);
  }
}

// TODO(falken): Add a test for a shared worker that's controlled by a service
// worker.

}  // namespace content
