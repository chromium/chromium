// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/worker_host/mock_shared_worker.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/browser/worker_host/worker_script_fetcher.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/not_implemented_url_loader_factory.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/test/client_security_state_builder.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using blink::MessagePortChannel;

namespace content {

namespace {
const ukm::SourceId kClientUkmSourceId = 12345;
}  // namespace

class SharedWorkerHostTest : public testing::Test {
 public:
  const GURL kSiteUrl{"http://www.example.com/"};
  const GURL kWorkerUrl{"http://www.example.com/w.js"};

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &mock_render_process_host_factory_);
    site_instance_ =
        SiteInstanceImpl::CreateForTesting(&browser_context_, kWorkerUrl);
    RenderProcessHost* rph = site_instance_->GetProcess();

    std::vector<std::unique_ptr<MockRenderProcessHost>>* processes =
        mock_render_process_host_factory_.GetProcesses();
    ASSERT_EQ(processes->size(), 1u);
    mock_render_process_host_ = processes->at(0).get();
    ASSERT_EQ(rph, mock_render_process_host_);
    ASSERT_TRUE(mock_render_process_host_->Init());
  }

  SharedWorkerHostTest()
      : service_(nullptr /* storage_partition */,
                 nullptr /* service_worker_context */) {}

  SharedWorkerHostTest(const SharedWorkerHostTest&) = delete;
  SharedWorkerHostTest& operator=(const SharedWorkerHostTest&) = delete;

  base::WeakPtr<SharedWorkerHost> CreateHost() {
    SharedWorkerInstance instance(
        kWorkerUrl, blink::mojom::ScriptType::kClassic,
        network::mojom::CredentialsMode::kSameOrigin, "name",
        blink::StorageKey::CreateFirstParty(url::Origin::Create(kWorkerUrl)),
        blink::mojom::SharedWorkerCreationContextType::kSecure,
        blink::mojom::SharedWorkerSameSiteCookies::kAll);
    auto host = std::make_unique<SharedWorkerHost>(
        &service_, instance, site_instance_,
        std::vector<network::mojom::ContentSecurityPolicyPtr>(),
        base::MakeRefCounted<PolicyContainerHost>());
    auto weak_host = host->AsWeakPtr();
    service_.worker_hosts_.insert(std::move(host));
    return weak_host;
  }

  void StartWorker(
      SharedWorkerHost* host,
      mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory,
      const GURL& final_response_url = GURL(),
      network::mojom::URLResponseHeadPtr response_head =
          network::mojom::URLResponseHead::New()) {
    auto main_script_load_params =
        blink::mojom::WorkerMainScriptLoadParams::New();
    if (!response_head->parsed_headers) {
      response_head->parsed_headers = network::mojom::ParsedHeaders::New();
    }
    main_script_load_params->response_head = std::move(response_head);
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult rv =
        mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    main_script_load_params->response_body = std::move(consumer_handle);
    auto subresource_loader_factories =
        std::make_unique<blink::PendingURLLoaderFactoryBundle>();

    SubresourceLoaderParams subresource_loader_params =
        SubresourceLoaderParams();
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        loader_factory_remote =
            network::NotImplementedURLLoaderFactory::Create();

    // Set up for service worker.
    auto service_worker_handle =
        std::make_unique<ServiceWorkerMainResourceHandle>(
            helper_->context_wrapper(), base::DoNothing());
    service_worker_handle->set_service_worker_client(
        helper_->context()
            ->service_worker_client_owner()
            .CreateServiceWorkerClientForWorker(
                mock_render_process_host_->GetID(),
                ServiceWorkerClientInfo(host->token())));
    host->SetServiceWorkerHandle(std::move(service_worker_handle));

    TestContentBrowserClient client;
    host->Start(std::move(factory),
                blink::mojom::FetchClientSettingsObject::New(
                    network::mojom::ReferrerPolicy::kDefault,
                    /*outgoing_referrer=*/GURL(),
                    blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade),
                &client,
                WorkerScriptFetcherResult(
                    std::move(subresource_loader_factories),
                    std::move(main_script_load_params),
                    PolicyContainerPolicies(), final_response_url));
  }

  MessagePortChannel AddClient(
      SharedWorkerHost* host,
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client) {
    GlobalRenderFrameHostId dummy_render_frame_host_id(
        mock_render_process_host_->GetID(), 22);

    blink::MessagePortDescriptorPair port_pair;
    MessagePortChannel local_port(port_pair.TakePort0());
    MessagePortChannel remote_port(port_pair.TakePort1());
    host->AddClient(std::move(client), dummy_render_frame_host_id,
                    std::move(remote_port), kClientUkmSourceId);
    return local_port;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  MockRenderProcessHostFactory mock_render_process_host_factory_;
  raw_ptr<MockRenderProcessHost> mock_render_process_host_;

  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<SiteInstanceImpl> site_instance_;

  SharedWorkerServiceImpl service_;
};

TEST_F(SharedWorkerHostTest, Normal) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  MockSharedWorkerFactory factory_impl(
      factory.InitWithNewPipeAndPassReceiver());
  StartWorker(host.get(), std::move(factory));

  // Add the initiating client.
  MockSharedWorkerClient client;
  mojo::PendingRemote<blink::mojom::SharedWorkerClient> remote_client;
  client.Bind(remote_client.InitWithNewPipeAndPassReceiver());
  MessagePortChannel local_port =
      AddClient(host.get(), std::move(remote_client));
  base::RunLoop().RunUntilIdle();

  // The factory should have gotten the CreateSharedWorker message.
  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
      host->instance().url(), host->instance().name(),
      host->content_security_policies(), &worker_host, &worker_receiver));
  {
    MockSharedWorker worker(std::move(worker_receiver));
    base::RunLoop().RunUntilIdle();

    // The worker and client should have gotten initial messages.
    int connection_request_id;
    MessagePortChannel port;
    EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));
    EXPECT_TRUE(client.CheckReceivedOnCreated());
    // Simulate events the shared worker would send.

    // Create message pipes. We may need to keep |devtools_agent_receiver| and
    // |devtools_agent_host_remote| if we want not to invoke connection error
    // handlers.
    mojo::PendingRemote<blink::mojom::DevToolsAgent> devtools_agent_remote;
    mojo::PendingReceiver<blink::mojom::DevToolsAgent> devtools_agent_receiver =
        devtools_agent_remote.InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<blink::mojom::DevToolsAgentHost>
        devtools_agent_host_remote;
    worker_host->OnReadyForInspection(
        std::move(devtools_agent_remote),
        devtools_agent_host_remote.InitWithNewPipeAndPassReceiver());

    worker_host->OnConnected(connection_request_id);
    base::RunLoop().RunUntilIdle();

    // The client should be connected.
    EXPECT_TRUE(client.CheckReceivedOnConnected({
        blink::mojom::WebFeature::kCoepNoneSharedWorker,
    }));

    // Close the client. The host should detect that there are no clients left
    // and ask the worker to terminate.
    client.Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(worker.CheckReceivedTerminate());

    // Simulate the worker terminating by breaking the Mojo connection when
    // |worker| goes out of scope.
  }
  base::RunLoop().RunUntilIdle();

  // The host should have self-destructed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, TerminateBeforeStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Add a client.
  MockSharedWorkerClient client;
  mojo::PendingRemote<blink::mojom::SharedWorkerClient> remote_client;
  client.Bind(remote_client.InitWithNewPipeAndPassReceiver());
  MessagePortChannel local_port =
      AddClient(host.get(), std::move(remote_client));
  base::RunLoop().RunUntilIdle();

  // Terminate the host before starting.
  service_.DestroyHost(host.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host);

  // The client should be told startup failed.
  EXPECT_TRUE(client.CheckReceivedOnScriptLoadFailed());
}

TEST_F(SharedWorkerHostTest, TerminateAfterStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Create the factory.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  MockSharedWorkerFactory factory_impl(
      factory.InitWithNewPipeAndPassReceiver());
  // Start the worker.
  StartWorker(host.get(), std::move(factory));

  // Add a client.
  MockSharedWorkerClient client;
  mojo::PendingRemote<blink::mojom::SharedWorkerClient> remote_client;
  client.Bind(remote_client.InitWithNewPipeAndPassReceiver());
  MessagePortChannel local_port =
      AddClient(host.get(), std::move(remote_client));
  base::RunLoop().RunUntilIdle();

  {
    mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
    mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
    EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
        host->instance().url(), host->instance().name(),
        host->content_security_policies(), &worker_host, &worker_receiver));
    MockSharedWorker worker(std::move(worker_receiver));

    // Terminate after starting.
    service_.DestroyHost(host.get());
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(worker.CheckReceivedTerminate());
    // We simulate the worker terminating by breaking the Mojo connection when
    // it goes out of scope.
  }

  // Simulate the worker in the renderer terminating.
  base::RunLoop().RunUntilIdle();

  // The client should not have been told startup failed.
  EXPECT_FALSE(client.CheckReceivedOnScriptLoadFailed());
  // The host should no longer exist.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, OnContextClosed) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  MockSharedWorkerFactory factory_impl(
      factory.InitWithNewPipeAndPassReceiver());
  StartWorker(host.get(), std::move(factory));

  // Add a client.
  MockSharedWorkerClient client;
  mojo::PendingRemote<blink::mojom::SharedWorkerClient> remote_client;
  client.Bind(remote_client.InitWithNewPipeAndPassReceiver());
  MessagePortChannel local_port =
      AddClient(host.get(), std::move(remote_client));
  base::RunLoop().RunUntilIdle();

  {
    mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
    mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
    EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
        host->instance().url(), host->instance().name(),
        host->content_security_policies(), &worker_host, &worker_receiver));
    MockSharedWorker worker(std::move(worker_receiver));

    // Simulate the worker calling OnContextClosed().
    worker_host->OnContextClosed();
    base::RunLoop().RunUntilIdle();

    // Close the client. The host should detect that there are no clients left
    // and terminate the worker.
    client.Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(worker.CheckReceivedTerminate());

    // Simulate the worker terminating by breaking the Mojo connection when
    // |worker| goes out of scope.
  }
  base::RunLoop().RunUntilIdle();

  // The host should no longer exist.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, CreateNetworkFactoryParamsForSubresources) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  MockSharedWorkerFactory factory_impl(
      factory.InitWithNewPipeAndPassReceiver());
  StartWorker(host.get(), std::move(factory));

  network::mojom::URLLoaderFactoryParamsPtr params =
      host->CreateNetworkFactoryParamsForSubresources();
  EXPECT_EQ(host->GetStorageKey().origin(),
            params->isolation_info.frame_origin());
  EXPECT_FALSE(params->isolation_info.nonce().has_value());
}

TEST_F(SharedWorkerHostTest,
       CreateNetworkFactoryParamsForSubresourcesWithNonce) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  SharedWorkerInstance instance(
      kWorkerUrl, blink::mojom::ScriptType::kClassic,
      network::mojom::CredentialsMode::kSameOrigin, "name",
      blink::StorageKey::CreateWithNonce(url::Origin::Create(kWorkerUrl),
                                         nonce),
      blink::mojom::SharedWorkerCreationContextType::kSecure,
      blink::mojom::SharedWorkerSameSiteCookies::kNone);
  auto host = std::make_unique<SharedWorkerHost>(
      &service_, instance, site_instance_,
      std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      base::MakeRefCounted<PolicyContainerHost>());

  // Start the worker.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  MockSharedWorkerFactory factory_impl(
      factory.InitWithNewPipeAndPassReceiver());
  StartWorker(host.get(), std::move(factory));

  network::mojom::URLLoaderFactoryParamsPtr params =
      host->CreateNetworkFactoryParamsForSubresources();
  EXPECT_EQ(url::Origin::Create(kWorkerUrl),
            params->isolation_info.frame_origin());
  EXPECT_THAT(params->isolation_info.nonce(), testing::Optional(nonce));
}

// Enable PrivateNetworkAccessForWorkers.
class SharedWorkerHostTestWithPNAEnabled : public SharedWorkerHostTest {
 public:
  SharedWorkerHostTestWithPNAEnabled() {
    feature_list_.InitWithFeatures(
        {
            features::kPrivateNetworkAccessSendPreflights,
            features::kPrivateNetworkAccessForWorkers,
        },
        {});
  }

  ~SharedWorkerHostTestWithPNAEnabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SharedWorkerHostTestWithPNAEnabled,
       CreateNetworkFactoryParamsForSubresources) {
  SharedWorkerInstance instance(
      kWorkerUrl, blink::mojom::ScriptType::kClassic,
      network::mojom::CredentialsMode::kSameOrigin, "name",
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kWorkerUrl)),
      blink::mojom::SharedWorkerCreationContextType::kSecure,
      blink::mojom::SharedWorkerSameSiteCookies::kAll);
  PolicyContainerPolicies policies;
  policies.cross_origin_embedder_policy.value =
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;
  policies.is_web_secure_context = true;
  network::CrossOriginEmbedderPolicy worker_cross_origin_embedder_policy;
  worker_cross_origin_embedder_policy.value =
      network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
  network::mojom::URLResponseHeadPtr response_head =
      network::mojom::URLResponseHead::New();
  response_head->parsed_headers = network::mojom::ParsedHeaders::New();
  response_head->parsed_headers->cross_origin_embedder_policy =
      worker_cross_origin_embedder_policy;
  auto host = std::make_unique<SharedWorkerHost>(
      &service_, instance, site_instance_,
      std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      base::MakeRefCounted<PolicyContainerHost>(std::move(policies)));

  // Start the worker.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  MockSharedWorkerFactory factory_impl(
      factory.InitWithNewPipeAndPassReceiver());
  StartWorker(host.get(), std::move(factory), GURL("devtools://test.url"),
              std::move(response_head));

  network::mojom::URLLoaderFactoryParamsPtr params =
      host->CreateNetworkFactoryParamsForSubresources();
  ASSERT_TRUE(params->client_security_state);
  EXPECT_TRUE(params->client_security_state->is_web_secure_context);
  EXPECT_EQ(params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kLocal);
  EXPECT_EQ(params->client_security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(params->client_security_state->cross_origin_embedder_policy.value,
            network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless);
}

}  // namespace content
