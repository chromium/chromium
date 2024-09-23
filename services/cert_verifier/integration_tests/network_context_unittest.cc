// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/log/test_net_log.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cert_verifier {
namespace {

network::mojom::CertVerifierServiceRemoteParamsPtr
GetNewCertVerifierServiceRemoteParams(
    mojom::CertVerifierServiceFactory* cert_verifier_service_factory,
    mojom::CertVerifierCreationParamsPtr creation_params) {
  mojo::PendingRemote<mojom::CertVerifierService> cert_verifier_remote;
  mojo::PendingReceiver<mojom::CertVerifierServiceClient> cert_verifier_client;
  cert_verifier_service_factory->GetNewCertVerifier(
      cert_verifier_remote.InitWithNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cert_verifier_client.InitWithNewPipeAndPassRemote(),
      std::move(creation_params));
  return network::mojom::CertVerifierServiceRemoteParams::New(
      std::move(cert_verifier_remote), std::move(cert_verifier_client));
}

}  // namespace

class NetworkContextWithRealCertVerifierTest : public testing::Test {
 public:
  explicit NetworkContextWithRealCertVerifierTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          time_source),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        network_service_(network::NetworkService::CreateForTesting()) {}
  ~NetworkContextWithRealCertVerifierTest() override = default;

  std::unique_ptr<network::NetworkContext> CreateContextWithParams(
      network::mojom::NetworkContextParamsPtr context_params) {
    network_context_remote_.reset();
    return std::make_unique<network::NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
  }

  network::mojom::CertVerifierServiceRemoteParamsPtr GetCertVerifierParams(
      mojom::CertVerifierCreationParamsPtr cert_verifier_creation_params =
          mojom::CertVerifierCreationParams::New()) {
    if (!cert_verifier_service_factory_) {
      cert_verifier_service_factory_ =
          std::make_unique<CertVerifierServiceFactoryImpl>(
              cert_verifier_service_factory_remote_
                  .BindNewPipeAndPassReceiver());
      InitializeCertVerifierServiceFactory(
          cert_verifier_service_factory_.get());
    }

    // Create a cert verifier service.
    return GetNewCertVerifierServiceRemoteParams(
        cert_verifier_service_factory_.get(),
        std::move(cert_verifier_creation_params));
  }

  virtual void InitializeCertVerifierServiceFactory(
      mojom::CertVerifierServiceFactory* factory) {}

  network::mojom::NetworkService* network_service() const {
    return network_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<network::NetworkService> network_service_;
  // Stores the mojo::Remote<NetworkContext> of the most recently created
  // NetworkContext. Not strictly needed, but seems best to mimic real-world
  // usage.
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;

  mojo::Remote<mojom::CertVerifierServiceFactory>
      cert_verifier_service_factory_remote_;
  std::unique_ptr<mojom::CertVerifierServiceFactory>
      cert_verifier_service_factory_;
};

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
namespace {

network::mojom::NetworkContextParamsPtr CreateContextParams() {
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

std::unique_ptr<network::TestURLLoaderClient> FetchRequest(
    const network::ResourceRequest& request,
    network::NetworkContext* network_context,
    int url_loader_options = network::mojom::kURLLoadOptionNone,
    int process_id = network::mojom::kBrowserProcessId,
    network::mojom::URLLoaderFactoryParamsPtr params = nullptr) {
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
  if (!params)
    params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = process_id;
  params->is_orb_enabled = false;

  // If |site_for_cookies| is null, any non-empty NIK is fine. Otherwise, the
  // NIK must be consistent with |site_for_cookies|.
  if (request.site_for_cookies.IsNull()) {
    params->isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://abc.invalid")),
        url::Origin::Create(GURL("https://xyz.invalid")),
        request.site_for_cookies);
  } else {
    params->isolation_info = net::IsolationInfo::CreateForInternalRequest(
        url::Origin::Create(request.site_for_cookies.RepresentativeUrl()));
  }

  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  auto client = std::make_unique<network::TestURLLoaderClient>();
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      url_loader_options, request, client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client->RunUntilComplete();
  return client;
}

}  // namespace

class NetworkContextChromeRootStoreIsUsedTest
    : public NetworkContextWithRealCertVerifierTest,
      public testing::WithParamInterface<bool> {
 public:
  void InitializeCertVerifierServiceFactory(
      mojom::CertVerifierServiceFactory* factory) override {
    factory->SetUseChromeRootStore(use_chrome_root_store(), base::DoNothing());
  }

  bool use_chrome_root_store() const { return GetParam(); }
};

TEST_P(NetworkContextChromeRootStoreIsUsedTest,
       ChromeRootStoreCanBeDisabledTest) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  network::mojom::NetworkContextParamsPtr params = CreateContextParams();
  params->cert_verifier_params = GetCertVerifierParams();
  std::unique_ptr<network::NetworkContext> network_context =
      CreateContextWithParams(std::move(params));

  net::RecordingNetLogObserver net_log_observer(
      net::NetLogCaptureMode::kDefault);

  network::ResourceRequest request;
  request.url = test_server.GetURL("/nocontent");
  base::HistogramTester histogram_tester;
  std::unique_ptr<network::TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  std::vector<net::NetLogEntry> crs_netlog_entries =
      net_log_observer.GetEntriesWithType(
          net::NetLogEventType::CERT_VERIFY_PROC_CHROME_ROOT_STORE_VERSION);
  EXPECT_EQ(use_chrome_root_store(), !crs_netlog_entries.empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         NetworkContextChromeRootStoreIsUsedTest,
                         ::testing::Bool());
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)

}  // namespace cert_verifier
