// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace cors {

namespace {

constexpr int kProcessId = 123;
constexpr int kRequestId = 456;
constexpr int kRouteId = 789;

}  // namespace

class CorsURLLoaderFactoryTest : public testing::Test {
 public:
  CorsURLLoaderFactoryTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    net::URLRequestContextBuilder context_builder;
    context_builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());
    url_request_context_ = context_builder.Build();
  }

 protected:
  // testing::Test implementation.
  void SetUp() override {
    test_server_.AddDefaultHandlers();
    ASSERT_TRUE(test_server_.Start());

    network_service_ = NetworkService::CreateForTesting();

    auto context_params = mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    factory_params->process_id = kProcessId;
    factory_params->request_initiator_origin_lock =
        url::Origin::Create(test_server_.base_url());
    auto resource_scheduler_client =
        base::MakeRefCounted<ResourceSchedulerClient>(
            kProcessId, kRouteId, &resource_scheduler_,
            url_request_context_->network_quality_estimator());
    cors_url_loader_factory_ = std::make_unique<CorsURLLoaderFactory>(
        network_context_.get(), std::move(factory_params),
        resource_scheduler_client,
        cors_url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        &origin_access_list_);
  }

  void CreateLoaderAndStart(const ResourceRequest& request) {
    url_loaders_.emplace_back(mojo::Remote<mojom::URLLoader>());
    test_cors_loader_clients_.emplace_back(
        std::make_unique<TestURLLoaderClient>());
    cors_url_loader_factory_->CreateLoaderAndStart(
        url_loaders_.back().BindNewPipeAndPassReceiver(), kRequestId,
        mojom::kURLLoadOptionNone, request,
        test_cors_loader_clients_.back()->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void ResetFactory() { cors_url_loader_factory_.reset(); }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

  std::vector<std::unique_ptr<TestURLLoaderClient>>&
  test_cors_loader_clients() {
    return test_cors_loader_clients_;
  }

 private:
  // Test environment.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  ResourceScheduler resource_scheduler_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  net::test_server::EmbeddedTestServer test_server_;

  // CorsURLLoaderFactory instance under tests.
  std::unique_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> cors_url_loader_factory_remote_;

  // Holds the URLLoaders that CreateLoaderAndStart() creates.
  std::vector<mojo::Remote<mojom::URLLoader>> url_loaders_;

  // TestURLLoaderClients that record callback activities.
  std::vector<std::unique_ptr<TestURLLoaderClient>> test_cors_loader_clients_;

  // Holds for allowed origin access lists.
  OriginAccessList origin_access_list_;

  DISALLOW_COPY_AND_ASSIGN(CorsURLLoaderFactoryTest);
};

// Regression test for https://crbug.com/906305.
TEST_F(CorsURLLoaderFactoryTest, DestructionOrder) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/hung");
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = url::Origin::Create(url);

  // As of r609458 setting |keepalive| to true was triggerring a dereference of
  // |factory_params_| in the destructor of network::URLLoader.  This
  // dereference assumes that the network::URLLoaderFactory (which keeps
  // |factory_params_| alive) lives longer than the network::URLLoaders created
  // via the factory (which necessitates being careful with the destruction
  // order of fields of network::cors::CorsURLLoaderFactory which owns both
  // network::URLLoaderFactory and the network::URLLoaders it creates).
  request.keepalive = true;

  // Create a loader and immediately (while the loader is still stored in
  // CorsURLLoaderFactory::loaders_ / not released via test_cors_loader_client_)
  // destroy the factory.  If ASAN doesn't complain then the test passes.
  CreateLoaderAndStart(request);
  ResetFactory();
}

TEST_F(CorsURLLoaderFactoryTest, CleanupWithSharedCacheObjectInUse) {
  // Create a loader for a response that hangs after receiving headers, and run
  // it until headers are received.
  ResourceRequest request;
  GURL url = test_server()->GetURL("/hung-after-headers");
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  CreateLoaderAndStart(request);
  test_cors_loader_clients().back()->RunUntilResponseReceived();

  // Read only requests will fail synchonously on destruction of the request
  // they're waiting on if they're in the |done_headers_queue| when the other
  // request fails. Make a large number of such requests, spin the message loop
  // so they end up blocked on the hung request, and then destroy all loads. A
  // large number of loaders is needed because they're stored in a set, indexed
  // by address, so teardown order is random.
  request.load_flags =
      net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
  for (int i = 0; i < 10; ++i)
    CreateLoaderAndStart(request);
  base::RunLoop().RunUntilIdle();

  // This should result in a crash if tearing down one URLLoaderFactory
  // resulting in a another one failing causes a crash during teardown. See
  // https://crbug.com/1209769.
  ResetFactory();
}

}  // namespace cors
}  // namespace network
