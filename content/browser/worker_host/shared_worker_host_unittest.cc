// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/worker_host/mock_shared_worker.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/not_implemented_network_url_loader_factory.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "url/origin.h"

using blink::MessagePortChannel;

namespace content {

class SharedWorkerHostTest : public testing::Test {
 public:
  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  SharedWorkerHostTest()
      : default_mock_url_loader_factory_(
            std::make_unique<NotImplementedNetworkURLLoaderFactory>()),
        mock_render_process_host_(&browser_context_),
        service_(nullptr /* storage_partition */,
                 nullptr /* service_worker_context */,
                 nullptr /* appcache_service */) {
    mock_render_process_host_.OverrideURLLoaderFactory(
        default_mock_url_loader_factory_.get());
  }

  base::WeakPtr<SharedWorkerHost> CreateHost() {
    GURL url("http://www.example.com/w.js");
    std::string name("name");
    url::Origin origin = url::Origin::Create(url);
    std::string content_security_policy;
    network::mojom::ContentSecurityPolicyType content_security_policy_type =
        network::mojom::ContentSecurityPolicyType::kReport;
    network::mojom::IPAddressSpace creation_address_space =
        network::mojom::IPAddressSpace::kPublic;
    blink::mojom::SharedWorkerCreationContextType creation_context_type =
        blink::mojom::SharedWorkerCreationContextType::kSecure;

    SharedWorkerInstance instance(
        service_.next_shared_worker_instance_id_++, url, name, origin,
        content_security_policy, content_security_policy_type,
        creation_address_space, creation_context_type);
    auto host = std::make_unique<SharedWorkerHost>(
        &service_, instance, mock_render_process_host_.GetID());
    auto weak_host = host->AsWeakPtr();
    service_.worker_hosts_.insert(std::move(host));
    return weak_host;
  }

  void StartWorker(
      SharedWorkerHost* host,
      mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory) {
    auto main_script_load_params =
        blink::mojom::WorkerMainScriptLoadParams::New();
    main_script_load_params->response_head =
        network::mojom::URLResponseHead::New();
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult rv =
        mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    main_script_load_params->response_body = std::move(consumer_handle);
    auto subresource_loader_factories =
        std::make_unique<blink::URLLoaderFactoryBundleInfo>();

    base::Optional<SubresourceLoaderParams> subresource_loader_params =
        SubresourceLoaderParams();
    mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<NotImplementedNetworkURLLoaderFactory>(),
        loader_factory_remote.InitWithNewPipeAndPassReceiver());
    subresource_loader_params->pending_appcache_loader_factory =
        std::move(loader_factory_remote);

    // Set up for service worker.
    auto service_worker_handle =
        std::make_unique<ServiceWorkerNavigationHandle>(
            helper_->context_wrapper());
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver;
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    provider_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    host_receiver =
        provider_info->host_remote.InitWithNewEndpointAndPassReceiver();
    base::WeakPtr<ServiceWorkerProviderHost> service_worker_host =
        ServiceWorkerProviderHost::CreateForWebWorker(
            helper_->context()->AsWeakPtr(), mock_render_process_host_.GetID(),
            blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
            std::move(host_receiver), std::move(client_remote));
    service_worker_handle->OnCreatedProviderHost(std::move(provider_info));
    host->SetServiceWorkerHandle(std::move(service_worker_handle));

    host->Start(std::move(factory), std::move(main_script_load_params),
                std::move(subresource_loader_factories),
                nullptr /* controller */,
                nullptr /* controller_service_worker_object_host */);
  }

  MessagePortChannel AddClient(
      SharedWorkerHost* host,
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client) {
    mojo::MessagePipe message_pipe;
    MessagePortChannel local_port(std::move(message_pipe.handle0));
    MessagePortChannel remote_port(std::move(message_pipe.handle1));
    host->AddClient(std::move(client), mock_render_process_host_.GetID(),
                    22 /* dummy frame_id */, std::move(remote_port));
    return local_port;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  std::unique_ptr<network::mojom::URLLoaderFactory>
      default_mock_url_loader_factory_;
  MockRenderProcessHost mock_render_process_host_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  SharedWorkerServiceImpl service_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHostTest);
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
      host->instance().content_security_policy_type(), &worker_host,
      &worker_receiver));
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
    EXPECT_TRUE(
        client.CheckReceivedOnConnected({} /* expected_used_features */));

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
        host->instance().content_security_policy_type(), &worker_host,
        &worker_receiver));
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
        host->instance().content_security_policy_type(), &worker_host,
        &worker_receiver));
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

}  // namespace content
