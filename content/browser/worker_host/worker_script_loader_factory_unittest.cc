// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_loader_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

namespace {

const int kProcessId = 1;

}  // namespace

class WorkerScriptLoaderFactoryTest : public testing::Test {
 public:
  WorkerScriptLoaderFactoryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~WorkerScriptLoaderFactoryTest() override = default;

  void SetUp() override {
    // Set up the service worker system.
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());

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
        helper_->context_wrapper(), base::DoNothing());
  }

 protected:
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
      kProcessId, DedicatedOrSharedWorkerToken(),
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
              /*coep_reporter=*/{}, ukm::kInvalidSourceId);
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

// Test a null service worker handle. This typically only happens during
// shutdown or after a fatal error occurred in the service worker system.
TEST_F(WorkerScriptLoaderFactoryTest, NullServiceWorkerHandle) {
  GURL url("https://www.example.com/worker.js");

  // Make the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      kProcessId, DedicatedOrSharedWorkerToken(),
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
      kProcessId, DedicatedOrSharedWorkerToken(),
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

// TODO(falken): Add a test for a shared worker that's controlled by a service
// worker.

}  // namespace content
