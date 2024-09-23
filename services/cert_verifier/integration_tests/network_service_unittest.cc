// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cert_verifier/mojo_cert_verifier.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/ssl_config_service_mojo.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cert_verifier {
namespace {
const base::FilePath::CharType kServicesTestData[] =
    FILE_PATH_LITERAL("services/test/data");
}  // namespace

// Base class for any tests that just need a NetworkService and a
// TaskEnvironment, and to create NetworkContexts using the NetworkService.
class NetworkServiceIntegrationTest : public testing::Test {
 public:
  NetworkServiceIntegrationTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        service_(network::NetworkService::CreateForTesting()),
        cert_verifier_service_impl_(
            cert_verifier_service_remote_.BindNewPipeAndPassReceiver()) {}
  ~NetworkServiceIntegrationTest() override = default;

  void DestroyService() { service_.reset(); }

  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
    mojo::PendingRemote<mojom::CertVerifierService> cv_service_remote;
    mojo::PendingReceiver<mojom::CertVerifierServiceClient> cv_service_client;

    // Create a cert verifier service.
    cert_verifier_service_impl_.GetNewCertVerifierForTesting(
        cv_service_remote.InitWithNewPipeAndPassReceiver(),
        /*updater_receiver=*/mojo::NullReceiver(),
        cv_service_client.InitWithNewPipeAndPassRemote(),
        mojom::CertVerifierCreationParams::New(),
        &cert_net_fetcher_url_loader_);

    network::mojom::NetworkContextParamsPtr params =
        network::mojom::NetworkContextParams::New();
    params->cert_verifier_params =
        network::mojom::CertVerifierServiceRemoteParams::New(
            std::move(cv_service_remote), std::move(cv_service_client));
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    return params;
  }

  void CreateNetworkContext(network::mojom::NetworkContextParamsPtr params) {
    network_context_ = std::make_unique<network::NetworkContext>(
        service_.get(), network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(params));
  }

  void LoadURL(const GURL& url,
               int options = network::mojom::kURLLoadOptionNone) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();
    StartLoadingURL(request, 0 /* process_id */, options);
    client_->RunUntilComplete();
  }

  void StartLoadingURL(const network::ResourceRequest& request,
                       uint32_t process_id,
                       int options = network::mojom::kURLLoadOptionNone) {
    client_ = std::make_unique<network::TestURLLoaderClient>();
    mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id;
    params->is_orb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    loader_.reset();
    loader_factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 1, options, request,
        client_->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  network::TestURLLoaderClient* client() { return client_.get(); }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  network::NetworkService* service() const { return service_.get(); }

  network::NetworkContext* network_context() { return network_context_.get(); }

  mojo::Remote<network::mojom::NetworkContext>& network_context_remote() {
    return network_context_remote_;
  }

  CertNetFetcherURLLoader* cert_net_fetcher_url_loader() {
    return cert_net_fetcher_url_loader_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkService> service_;

  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<network::NetworkContext> network_context_;

  mojo::Remote<mojom::CertVerifierServiceFactory> cert_verifier_service_remote_;
  CertVerifierServiceFactoryImpl cert_verifier_service_impl_;
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher_url_loader_;

  std::unique_ptr<network::TestURLLoaderClient> client_;
  mojo::Remote<network::mojom::URLLoader> loader_;
};

// TODO(crbug.com/41399468): AIA tests fail on iOS
#if BUILDFLAG(IS_IOS)
#define MAYBE(test_name) DISABLED_##test_name
#else
#define MAYBE(test_name) test_name
#endif

class NetworkServiceAIATest : public NetworkServiceIntegrationTest {
 public:
  NetworkServiceAIATest() : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~NetworkServiceAIATest() override = default;

  void SetUp() override {
    NetworkServiceIntegrationTest::SetUp();
    network::mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParams();
    CreateNetworkContext(std::move(context_params));

    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.intermediate =
        net::EmbeddedTestServer::IntermediateType::kByAIA;
    test_server_.SetSSLConfig(cert_config);
    test_server_.AddDefaultHandlers(base::FilePath(kServicesTestData));
    ASSERT_TRUE(test_server_.Start());
  }

  void PerformAIATest() {
    LoadURL(test_server_.GetURL("/echo"),
            network::mojom::kURLLoadOptionSendSSLInfoWithResponse);
    EXPECT_EQ(net::OK, client()->completion_status().error_code);
    ASSERT_TRUE(client()->response_head());
    EXPECT_EQ(0u, client()->response_head()->cert_status &
                      net::CERT_STATUS_ALL_ERRORS);
    ASSERT_TRUE(client()->ssl_info());
    ASSERT_TRUE(client()->ssl_info()->cert);
    EXPECT_EQ(2u, client()->ssl_info()->cert->intermediate_buffers().size());
    ASSERT_TRUE(client()->ssl_info()->unverified_cert);
    EXPECT_EQ(
        0u,
        client()->ssl_info()->unverified_cert->intermediate_buffers().size());
  }

 private:
  net::EmbeddedTestServer test_server_;
};

TEST_F(NetworkServiceAIATest, MAYBE(AIAFetching)) {
  PerformAIATest();
}

// Check that AIA fetching still succeeds even after the URLLoaderFactory
// backing the CertNetFetcherURLLoader disconnects.
// Only relevant if testing with the CertVerifierService, and the underlying
// CertVerifier uses the CertNetFetcher.
TEST_F(NetworkServiceAIATest,
       MAYBE(AIAFetchingWithURLLoaderFactoryDisconnect)) {
  if (!cert_net_fetcher_url_loader()) {
    // TODO(crbug.com/40103822): Switch to GTEST_SKIP().
    LOG(WARNING) << "Skipping AIA reconnection test because the underlying "
                    "cert verifier does not use a CertNetFetcherURLLoader.";
    return;
  }
  PerformAIATest();
  // Disconnect the URLLoaderFactory used by the CertNetFetcherURLLoader.
  cert_net_fetcher_url_loader()->DisconnectURLLoaderFactoryForTesting();
  // Try running the test again to test reconnection of the
  // CertNetFetcherURLLoader.
  PerformAIATest();
  // Repeat one more time to be sure.
  cert_net_fetcher_url_loader()->DisconnectURLLoaderFactoryForTesting();
  PerformAIATest();
}

}  // namespace cert_verifier
