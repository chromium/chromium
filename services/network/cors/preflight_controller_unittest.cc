// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/shared_dictionary_error.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/client_security_state_builder.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network::cors {

namespace {

using ::testing::Optional;
using WithTrustedHeaderClient = PreflightController::WithTrustedHeaderClient;
using PreflightMode = PreflightController::PreflightMode;
using PreflightType = PreflightController::PreflightType;

TEST(PreflightControllerCreatePreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");
  request.headers.SetHeader("Apple", "Red");
  request.headers.SetHeader("Kiwifruit", "Green");
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");
  request.headers.SetHeader("Strawberry", "Red");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ("null",
            preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin));

  EXPECT_EQ(
      "apple,content-type,kiwifruit,orange,strawberry",
      preflight->headers.GetHeader(header_names::kAccessControlRequestHeaders));
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Accept", "everything");
  request.headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                            "everything");
  request.headers.SetHeader("Content-Language", "everything");
  request.headers.SetHeader("Save-Data", "on");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  EXPECT_FALSE(
      preflight->headers.GetHeader(header_names::kAccessControlRequestHeaders));
}

TEST(PreflightControllerCreatePreflightRequestTest, Credentials) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ(mojom::CredentialsMode::kOmit, preflight->credentials_mode);
}

TEST(PreflightControllerCreatePreflightRequestTest,
     ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "text/plain");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  // Empty list also; see comment in test above.
  EXPECT_FALSE(
      preflight->headers.GetHeader(header_names::kAccessControlRequestHeaders));
}

TEST(PreflightControllerCreatePreflightRequestTest, IncludeSecFetchModeHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ("cors", preflight->headers.GetHeader("Sec-Fetch-Mode"));
}

TEST(PreflightControllerCreatePreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ("x-custom-header", preflight->headers.GetHeader(
                                   header_names::kAccessControlRequestHeaders));
}

TEST(PreflightControllerCreatePreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ("content-type", preflight->headers.GetHeader(
                                header_names::kAccessControlRequestHeaders));
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeForbiddenHeaders) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("referer", "https://www.google.com/");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_FALSE(
      preflight->headers.GetHeader(header_names::kAccessControlRequestHeaders));
}

TEST(PreflightControllerCreatePreflightRequestTest, Tainted) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request, true);

  EXPECT_EQ(preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin),
            "null");
}

TEST(PreflightControllerCreatePreflightRequestTest, FetchWindowId) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");
  request.fetch_window_id = base::UnguessableToken::Create();

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ(request.fetch_window_id, preflight->fetch_window_id);
}

TEST(PreflightControllerCreatePreflightRequestTest, SubframeNavigation) {
  const auto kTopFrameOrigin = url::Origin::Create(GURL("https://a.com/"));
  const auto kFrameOrigin = url::Origin::Create(GURL("https://b.com/"));

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.destination = network::mojom::RequestDestination::kIframe;
  request.request_initiator = kTopFrameOrigin;
  request.site_for_cookies = net::SiteForCookies::FromOrigin(kTopFrameOrigin);

  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kSubFrame, kTopFrameOrigin, kFrameOrigin,
      request.site_for_cookies);

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ(preflight->mode, mojom::RequestMode::kCors);
  EXPECT_EQ(preflight->destination,
            network::mojom::RequestDestination::kIframe);
  EXPECT_EQ(preflight->request_initiator, kTopFrameOrigin);
  EXPECT_TRUE(preflight->site_for_cookies.IsEquivalent(net::SiteForCookies()));
  EXPECT_TRUE(preflight->trusted_params->isolation_info.IsEqualForTesting(
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 kTopFrameOrigin, kFrameOrigin,
                                 net::SiteForCookies())));
}

// Requests might have TrustedParams but an empty IsolationInfo (for instance,
// requests associated with the system network context, or requests from
// embedders that haven't enabled network state partitioning). Verify that
// creating a preflight for such a request doesn't crash and otherwise behaves
// correctly.
TEST(PreflightControllerCreatePreflightRequestTest, EmptyIsolationInfo) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();

  request.trusted_params = ResourceRequest::TrustedParams();
  ASSERT_TRUE(request.trusted_params->isolation_info.IsEmpty());

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_TRUE(preflight->trusted_params->isolation_info.IsEmpty());
}

TEST(PreflightControllerOptionsTest, CheckOptions) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestURLLoaderFactory url_loader_factory;
  PreflightController preflight_controller(/*network_service=*/nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin();
  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::URL_REQUEST);

  for (const PreflightMode& preflight_mode :
       {PreflightMode{PreflightType::kCors},
        PreflightMode{PreflightType::kPrivateNetworkAccess},
        PreflightMode{PreflightType::kCors,
                      PreflightType::kPrivateNetworkAccess}}) {
    request.target_ip_address_space =
        preflight_mode.Has(PreflightType::kPrivateNetworkAccess)
            ? network::mojom::IPAddressSpace::kPrivate
            : network::mojom::IPAddressSpace::kUnknown;
    preflight_controller.PerformPreflightCheck(
        base::BindOnce([](int, std::optional<CorsErrorStatus>, bool) {}),
        request, WithTrustedHeaderClient(false),
        NonWildcardRequestHeadersSupport(false),
        PrivateNetworkAccessPreflightBehavior::kWarn, /*tainted=*/false,
        TRAFFIC_ANNOTATION_FOR_TESTS, &url_loader_factory, net::IsolationInfo(),
        /*client_security_state=*/nullptr,
        /*devtools_observer=*/
        base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>>(), net_log, true,
        mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>(),
        preflight_mode);

    preflight_controller.PerformPreflightCheck(
        base::BindOnce([](int, std::optional<CorsErrorStatus>, bool) {}),
        request, WithTrustedHeaderClient(true),
        NonWildcardRequestHeadersSupport(false),
        PrivateNetworkAccessPreflightBehavior::kWarn, /*tainted=*/false,
        TRAFFIC_ANNOTATION_FOR_TESTS, &url_loader_factory, net::IsolationInfo(),
        /*client_security_state=*/nullptr,
        /*devtools_observer=*/
        base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>>(), net_log, true,
        mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>(),
        preflight_mode);
  }

  ASSERT_EQ(6, url_loader_factory.NumPending());
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight,
            url_loader_factory.GetPendingRequest(0)->options);
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight |
                mojom::kURLLoadOptionUseHeaderClient,
            url_loader_factory.GetPendingRequest(1)->options);
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight,
            url_loader_factory.GetPendingRequest(2)->options);
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight |
                mojom::kURLLoadOptionUseHeaderClient,
            url_loader_factory.GetPendingRequest(3)->options);
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight,
            url_loader_factory.GetPendingRequest(4)->options);
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight |
                mojom::kURLLoadOptionUseHeaderClient,
            url_loader_factory.GetPendingRequest(5)->options);
}

class MockDevToolsObserver : public mojom::DevToolsObserver {
 public:
  explicit MockDevToolsObserver(
      mojo::PendingReceiver<mojom::DevToolsObserver> receiver) {
    receivers_.Add(this, std::move(receiver));
  }
  ~MockDevToolsObserver() override = default;

  MockDevToolsObserver(const MockDevToolsObserver&) = delete;
  MockDevToolsObserver& operator=(const MockDevToolsObserver&) = delete;

  mojo::PendingRemote<mojom::DevToolsObserver> Bind() {
    mojo::PendingRemote<mojom::DevToolsObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void WaitUntilRequestCompleted() {
    if (completed_)
      return;
    base::RunLoop run_loop;
    wait_for_completed_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool on_raw_request_called() const { return on_raw_request_called_; }
  bool on_raw_response_called() const { return on_raw_response_called_; }
  const network::mojom::URLRequestDevToolsInfoPtr& preflight_request() const {
    return preflight_request_info_;
  }
  const network::mojom::URLResponseHeadDevToolsInfoPtr& preflight_response()
      const {
    return preflight_response_;
  }
  const std::optional<network::URLLoaderCompletionStatus>& preflight_status()
      const {
    return preflight_status_;
  }
  const std::string& initiator_devtools_request_id() const {
    return initiator_devtools_request_id_;
  }

 private:
  // mojom::DevToolsObserver:
  void OnRawRequest(
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const base::TimeTicks timestamp,
      network::mojom::ClientSecurityStatePtr client_security_state,
      network::mojom::OtherPartitionInfoPtr other_partition_info) override {
    on_raw_request_called_ = true;
  }
  void OnRawResponse(
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const std::optional<std::string>& raw_response_headers,
      network::mojom::IPAddressSpace resource_address_space,
      int32_t http_status_code,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key)
      override {
    on_raw_response_called_ = true;
  }
  void OnEarlyHintsResponse(
      const std::string& devtools_request_id,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers) override {}
  void OnCorsPreflightRequest(
      const base::UnguessableToken& devtool_request_id,
      const net::HttpRequestHeaders& request_headers,
      network::mojom::URLRequestDevToolsInfoPtr request_info,
      const GURL& initiator_url,
      const std::string& initiator_devtools_request_id) override {
    preflight_request_info_ = std::move(request_info);
    initiator_devtools_request_id_ = initiator_devtools_request_id;
  }
  void OnCorsPreflightResponse(
      const base::UnguessableToken& devtool_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadDevToolsInfoPtr head) override {
    preflight_response_ = std::move(head);
  }
  void OnCorsPreflightRequestCompleted(
      const base::UnguessableToken& devtool_request_id,
      const network::URLLoaderCompletionStatus& status) override {
    completed_ = true;
    preflight_status_ = status;
    if (wait_for_completed_)
      std::move(wait_for_completed_).Run();
  }

  void OnSubresourceWebBundleMetadata(const std::string& devtools_request_id,
                                      const std::vector<GURL>& urls) override {}

  void OnSubresourceWebBundleMetadataError(
      const std::string& devtools_request_id,
      const std::string& error_message) override {}

  void OnSubresourceWebBundleInnerResponse(
      const std::string& inner_request_devtools_id,
      const ::GURL& url,
      const std::optional<std::string>& bundle_request_devtools_id) override {}

  void OnSubresourceWebBundleInnerResponseError(
      const std::string& inner_request_devtools_id,
      const ::GURL& url,
      const std::string& error_message,
      const std::optional<std::string>& bundle_request_devtools_id) override {}

  void OnSharedDictionaryError(
      const std::string& devtool_request_id,
      const GURL& url,
      network::mojom::SharedDictionaryError error) override {}

  void OnCorsError(const std::optional<std::string>& devtool_request_id,
                   const std::optional<::url::Origin>& initiator_origin,
                   mojom::ClientSecurityStatePtr client_security_state,
                   const GURL& url,
                   const network::CorsErrorStatus& status,
                   bool is_warning) override {}

  void OnOrbError(const std::optional<std::string>& devtools_request_id,
                  const GURL& url) override {}

  void Clone(mojo::PendingReceiver<DevToolsObserver> observer) override {
    receivers_.Add(this, std::move(observer));
  }
  void OnPrivateNetworkRequest(
      const std::optional<std::string>& devtool_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override {}
  void OnTrustTokenOperationDone(
      const std::string& devtool_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override {}

  bool completed_ = false;
  base::OnceClosure wait_for_completed_;
  bool on_raw_request_called_ = false;
  bool on_raw_response_called_ = false;
  network::mojom::URLRequestDevToolsInfoPtr preflight_request_info_;
  network::mojom::URLResponseHeadDevToolsInfoPtr preflight_response_;
  std::optional<network::URLLoaderCompletionStatus> preflight_status_;
  std::string initiator_devtools_request_id_;

  mojo::ReceiverSet<mojom::DevToolsObserver> receivers_;
};

class PreflightControllerTest : public testing::Test {
 public:
  PreflightControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        test_initiator_origin_(
            url::Origin::Create(GURL("http://example.com/"))),
        access_control_allow_origin_(test_initiator_origin_) {
    CorsURLLoaderFactory::SetAllowExternalPreflightsForTesting(true);
    mojo::Remote<mojom::NetworkService> network_service_remote;
    network_service_ = NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());

    auto context_params = mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_service_remote->CreateNetworkContext(
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    // We use network::CorsURLLoaderFactory for "internal" URLLoaderFactory
    // used by the PreflightController. Hence here we disable CORS as otherwise
    // the URLLoader would create a CORS-preflight for the preflight request.
    params->disable_web_security = true;
    params->is_orb_enabled = false;
    // Allow setting TrustedParams on requests, specifically to pass
    // ClientSecurityState to the underlying URLLoader.
    params->is_trusted = true;
    devtools_observer_ = std::make_unique<MockDevToolsObserver>(
        params->devtools_observer.InitWithNewPipeAndPassReceiver());
    network_context_remote_->CreateURLLoaderFactory(
        url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        std::move(params));
  }
  ~PreflightControllerTest() override {
    CorsURLLoaderFactory::SetAllowExternalPreflightsForTesting(false);
  }

 protected:
  void HandleRequestCompletion(int net_error,
                               std::optional<CorsErrorStatus> status,
                               bool has_authorization_covered_by_wildcard) {
    net_error_ = net_error;
    status_ = status;
    has_authorization_covered_by_wildcard_ =
        has_authorization_covered_by_wildcard;

    run_loop_->Quit();
  }

  GURL GetURL(const std::string& path) { return test_server_.GetURL(path); }

  void PerformPreflightCheck(
      const ResourceRequest& request,
      bool tainted = false,
      net::IsolationInfo isolation_info = net::IsolationInfo(),
      PrivateNetworkAccessPreflightBehavior private_network_access_behavior =
          PrivateNetworkAccessPreflightBehavior::kWarn,
      mojom::ClientSecurityStatePtr client_security_state = nullptr,
      const PreflightMode& preflight_mode = PreflightMode{
          PreflightType::kCors}) {
    DCHECK(preflight_controller_);
    run_loop_ = std::make_unique<base::RunLoop>();

    mojo::Remote<mojom::DevToolsObserver> devtools_observer(
        devtools_observer_->Bind());
    base::WeakPtrFactory<mojo::Remote<mojom::DevToolsObserver>>
        weak_devtools_observer_factory(&devtools_observer);
    preflight_controller_->PerformPreflightCheck(
        base::BindOnce(&PreflightControllerTest::HandleRequestCompletion,
                       base::Unretained(this)),
        request, WithTrustedHeaderClient(false),
        non_wildcard_request_headers_support_, private_network_access_behavior,
        tainted, TRAFFIC_ANNOTATION_FOR_TESTS, url_loader_factory_remote_.get(),
        isolation_info, std::move(client_security_state),
        weak_devtools_observer_factory.GetWeakPtr(),
        net::NetLogWithSource::Make(net::NetLog::Get(),
                                    net::NetLogSourceType::URL_REQUEST),
        true, mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>(),
        preflight_mode);
    run_loop_->Run();
  }

  void SetAccessControlAllowOrigin(const url::Origin origin) {
    access_control_allow_origin_ = origin;
  }
  void SetNonWildcardRequestHeadersSupport(bool value) {
    non_wildcard_request_headers_support_ =
        NonWildcardRequestHeadersSupport(value);
  }

  const url::Origin& test_initiator_origin() const {
    return test_initiator_origin_;
  }
  const url::Origin& access_control_allow_origin() const {
    return access_control_allow_origin_;
  }
  int net_error() const { return net_error_; }
  std::optional<CorsErrorStatus> status() { return status_; }
  bool has_authorization_covered_by_wildcard() const {
    return has_authorization_covered_by_wildcard_;
  }
  std::optional<CorsErrorStatus> success() { return std::nullopt; }
  size_t access_count() { return access_count_; }
  MockDevToolsObserver* devtools_observer() { return devtools_observer_.get(); }

 private:
  void SetUp() override {
    SetAccessControlAllowOrigin(test_initiator_origin_);

    preflight_controller_ =
        std::make_unique<PreflightController>(network_service_.get());

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &PreflightControllerTest::ServePreflight, base::Unretained(this)));

    EXPECT_TRUE(test_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> ServePreflight(
      const net::test_server::HttpRequest& request) {
    access_count_++;
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.method != net::test_server::METHOD_OPTIONS)
      return response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (net::test_server::ShouldHandle(request, "/404") ||
        net::test_server::ShouldHandle(request, "/allow") ||
        net::test_server::ShouldHandle(request, "/tainted") ||
        net::test_server::ShouldHandle(request, "/wildcard_headers")) {
      response->set_code(net::test_server::ShouldHandle(request, "/404")
                             ? net::HTTP_NOT_FOUND
                             : net::HTTP_OK);
      const url::Origin origin =
          net::test_server::ShouldHandle(request, "/tainted")
              ? url::Origin()
              : access_control_allow_origin();
      response->AddCustomHeader(header_names::kAccessControlAllowOrigin,
                                origin.Serialize());
      response->AddCustomHeader(header_names::kAccessControlAllowMethods,
                                "GET, OPTIONS");
      response->AddCustomHeader(header_names::kAccessControlMaxAge, "1000");
      response->AddCustomHeader(net::HttpRequestHeaders::kCacheControl,
                                "no-store");

      if (net::test_server::ShouldHandle(request, "/wildcard_headers")) {
        response->AddCustomHeader(header_names::kAccessControlAllowHeaders,
                                  "*");
      }
    }

    return response;
  }

  base::test::TaskEnvironment task_environment_;
  const url::Origin test_initiator_origin_;
  url::Origin access_control_allow_origin_;
  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<MockDevToolsObserver> devtools_observer_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_remote_;

  net::test_server::EmbeddedTestServer test_server_;
  size_t access_count_ = 0;
  NonWildcardRequestHeadersSupport non_wildcard_request_headers_support_;

  std::unique_ptr<PreflightController> preflight_controller_;
  int net_error_ = net::OK;
  std::optional<CorsErrorStatus> status_;
  bool has_authorization_covered_by_wildcard_ = false;
};

TEST_F(PreflightControllerTest, CheckInvalidRequest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/404");
  request.request_initiator = test_initiator_origin();

  PerformPreflightCheck(request);
  EXPECT_EQ(net::ERR_FAILED, net_error());
  ASSERT_TRUE(status());
  EXPECT_EQ(mojom::CorsError::kPreflightInvalidStatus, status()->cors_error);
  EXPECT_EQ(1u, access_count());
}

TEST_F(PreflightControllerTest, CheckValidRequest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  request.request_initiator = test_initiator_origin();

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.

  // Verify if cache related flags work to skip the preflight cache.
  request.load_flags = net::LOAD_VALIDATE_CACHE;
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(2u, access_count());

  request.load_flags = net::LOAD_BYPASS_CACHE;
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(3u, access_count());

  request.load_flags = net::LOAD_DISABLE_CACHE;
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(4u, access_count());
}

TEST_F(PreflightControllerTest, CheckRequestNetworkIsolationKey) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  const url::Origin& origin = test_initiator_origin();
  request.request_initiator = origin;
  ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, origin, net::SiteForCookies());
  request.trusted_params = {trusted_params};

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.

  url::Origin second_origin = url::Origin::Create(GURL("https://example.com/"));
  request.request_initiator = second_origin;
  SetAccessControlAllowOrigin(second_origin);
  request.trusted_params->isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, second_origin, net::SiteForCookies());
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(2u, access_count());
}

TEST_F(PreflightControllerTest, CheckFactoryNetworkIsolationKey) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  const url::Origin& origin = test_initiator_origin();
  request.request_initiator = origin;

  const net::IsolationInfo isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, origin, net::SiteForCookies());

  PerformPreflightCheck(request, false, isolation_info);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request, false, isolation_info);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.

  PerformPreflightCheck(request, false, net::IsolationInfo());
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(2u, access_count());  // Should not be from the preflight cache.
}

TEST_F(PreflightControllerTest, CheckTaintedRequest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/tainted");
  request.request_initiator = test_initiator_origin();

  PerformPreflightCheck(request, /*tainted=*/true);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());
}

TEST_F(PreflightControllerTest, CheckResponseWithNullHeaders) {
  GURL url = GURL("https://google.com/finullurl");
  const mojom::URLResponseHead response_head;
  ResourceRequest request;
  request.url = url;
  request.request_initiator = test_initiator_origin();
  const bool tainted = false;
  std::optional<CorsErrorStatus> detected_error_status;

  EXPECT_FALSE(response_head.headers);

  std::unique_ptr<PreflightResult> result =
      PreflightController::CreatePreflightResultForTesting(
          url, response_head, request, tainted,
          PrivateNetworkAccessPreflightBehavior::kEnforce,
          &detected_error_status);

  EXPECT_FALSE(result);
}

TEST_F(PreflightControllerTest, CheckPrivateNetworkAccessRequest) {
  GURL url = GetURL("/allow");
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = url;
  request.request_initiator = test_initiator_origin();
  request.target_ip_address_space = network::mojom::IPAddressSpace::kLocal;

  mojom::ClientSecurityStatePtr client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  // Set the client security state in the request's trusted params, because the
  // test uses a shared factory with no client security state in its factory
  // params, and URLLoader expects requests with a target IP address space to
  // carry a client security state.
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state = client_security_state.Clone();

  PerformPreflightCheck(request, /*tainted=*/false, net::IsolationInfo(),
                        PrivateNetworkAccessPreflightBehavior::kEnforce,
                        std::move(client_security_state),
                        PreflightMode{PreflightType::kPrivateNetworkAccess});
  EXPECT_EQ(net::ERR_FAILED, net_error());

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork, "");
  expected_status.target_address_space = mojom::IPAddressSpace::kLocal;
  EXPECT_THAT(status(), Optional(expected_status));
  EXPECT_EQ(1u, access_count());
}

TEST_F(PreflightControllerTest, CheckPrivateNetworkAccessRequestWarningOnly) {
  GURL url = GetURL("/allow");
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = url;
  request.request_initiator = test_initiator_origin();
  request.target_ip_address_space = network::mojom::IPAddressSpace::kLocal;

  mojom::ClientSecurityStatePtr client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  // Set the client security state in the request's trusted params, because the
  // test uses a shared factory with no client security state in its factory
  // params, and URLLoader expects requests with a target IP address space to
  // carry a client security state.
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state = client_security_state.Clone();

  PerformPreflightCheck(request, /*tainted=*/false, net::IsolationInfo(),
                        PrivateNetworkAccessPreflightBehavior::kWarn,
                        std::move(client_security_state),
                        PreflightMode{PreflightType::kPrivateNetworkAccess});
  EXPECT_EQ(net::OK, net_error());

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork, "");
  expected_status.target_address_space = mojom::IPAddressSpace::kLocal;
  EXPECT_THAT(status(), Optional(expected_status));
  EXPECT_EQ(1u, access_count());
}

// Set custom DelayedHttpResponse for test server.
std::unique_ptr<net::test_server::HttpResponse> AllowPrivateNetworkAccess(
    const net::test_server::HttpRequest& request) {
  // Warning preflights time out in 100ms. Delay the response by significantly
  // longer than that in order to test whether the timeout triggers or not.
  auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
      base::Milliseconds(500));
  response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  response->AddCustomHeader("Access-Control-Allow-Private-Network", "true");
  return std::move(response);
}

TEST_F(PreflightControllerTest,
       CheckPrivateNetworkAccessRequestTimeoutBehaviorEnforce) {
  net::EmbeddedTestServer delayed_server;
  delayed_server.RegisterRequestHandler(
      base::BindRepeating(&AllowPrivateNetworkAccess));
  ASSERT_TRUE(delayed_server.Start());
  ResourceRequest request;
  request.method = std::string("GET");
  GURL url = delayed_server.GetURL("/");
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.target_ip_address_space = network::mojom::IPAddressSpace::kLocal;

  mojom::ClientSecurityStatePtr client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
          .Build();

  // Set the client security state in the request's trusted params, because the
  // test uses a shared factory with no client security state in its factory
  // params, and URLLoader expects requests with a target IP address space to
  // carry a client security state.
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state = client_security_state.Clone();

  PerformPreflightCheck(request, /*tainted=*/false, net::IsolationInfo(),
                        PrivateNetworkAccessPreflightBehavior::kEnforce,
                        /*client_security_state=*/nullptr,
                        PreflightMode{PreflightType::kPrivateNetworkAccess});
  EXPECT_EQ(net::OK, net_error());
}

TEST_F(PreflightControllerTest,
       CheckPrivateNetworkAccessRequestTimeoutBehaviorWarnWithTimeout) {
  net::EmbeddedTestServer delayed_server;
  delayed_server.RegisterRequestHandler(
      base::BindRepeating(&AllowPrivateNetworkAccess));
  ASSERT_TRUE(delayed_server.Start());
  ResourceRequest request;
  request.method = std::string("GET");
  GURL url = delayed_server.GetURL("/");
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.target_ip_address_space = network::mojom::IPAddressSpace::kLocal;

  mojom::ClientSecurityStatePtr client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  // Set the client security state in the request's trusted params, because the
  // test uses a shared factory with no client security state in its factory
  // params, and URLLoader expects requests with a target IP address space to
  // carry a client security state.
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state = client_security_state.Clone();

  PerformPreflightCheck(request, /*tainted=*/false, net::IsolationInfo(),
                        PrivateNetworkAccessPreflightBehavior::kWarnWithTimeout,
                        /*client_security_state=*/nullptr,
                        PreflightMode{PreflightType::kPrivateNetworkAccess});
  EXPECT_EQ(net::ERR_TIMED_OUT, net_error());
}

TEST_F(PreflightControllerTest,
       CheckPrivateNetworkAccessRequestPreflightTimeoutBehaviorWarn) {
  net::EmbeddedTestServer delayed_server;
  delayed_server.RegisterRequestHandler(
      base::BindRepeating(&AllowPrivateNetworkAccess));
  ASSERT_TRUE(delayed_server.Start());
  ResourceRequest request;
  request.method = std::string("GET");
  GURL url = delayed_server.GetURL("/");
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.target_ip_address_space = network::mojom::IPAddressSpace::kLocal;

  mojom::ClientSecurityStatePtr client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
          .Build();

  // Set the client security state in the request's trusted params, because the
  // test uses a shared factory with no client security state in its factory
  // params, and URLLoader expects requests with a target IP address space to
  // carry a client security state.
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state = client_security_state.Clone();

  PerformPreflightCheck(request, /*tainted=*/false, net::IsolationInfo(),
                        PrivateNetworkAccessPreflightBehavior::kWarn,
                        /*client_security_state=*/nullptr,
                        PreflightMode{PreflightType::kPrivateNetworkAccess});
  EXPECT_EQ(net::OK, net_error());
}

class PreflightControllerNoPNAPreflightShortTimeoutTest
    : public PreflightControllerTest {
 public:
  PreflightControllerNoPNAPreflightShortTimeoutTest() {
    feature_list_.InitAndDisableFeature(
        features::kPrivateNetworkAccessPreflightShortTimeout);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PreflightControllerNoPNAPreflightShortTimeoutTest,
       CheckPrivateNetworkAccessRequestTimeoutBehaviorWarnWithTimeout) {
  net::EmbeddedTestServer delayed_server;
  delayed_server.RegisterRequestHandler(
      base::BindRepeating(&AllowPrivateNetworkAccess));
  ASSERT_TRUE(delayed_server.Start());
  ResourceRequest request;
  request.method = std::string("GET");
  GURL url = delayed_server.GetURL("/");
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.target_ip_address_space = network::mojom::IPAddressSpace::kLocal;

  mojom::ClientSecurityStatePtr client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  // Set the client security state in the request's trusted params, because the
  // test uses a shared factory with no client security state in its factory
  // params, and URLLoader expects requests with a target IP address space to
  // carry a client security state.
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state = client_security_state.Clone();

  PerformPreflightCheck(request, /*tainted=*/false, net::IsolationInfo(),
                        PrivateNetworkAccessPreflightBehavior::kWarnWithTimeout,
                        /*client_security_state=*/nullptr,
                        PreflightMode{PreflightType::kPrivateNetworkAccess});
  EXPECT_EQ(net::OK, net_error());
}

TEST_F(PreflightControllerTest, DevToolsEvents) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  request.request_initiator = test_initiator_origin();
  // Set the devtools id to trigger the DevTools event call on
  // NetworkServiceClient.
  request.devtools_request_id = "TEST";

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  // Check the DevTools event results.
  devtools_observer()->WaitUntilRequestCompleted();
  EXPECT_TRUE(devtools_observer()->on_raw_request_called());
  EXPECT_TRUE(devtools_observer()->on_raw_response_called());
  ASSERT_TRUE(devtools_observer()->preflight_request());
  EXPECT_EQ(request.url, devtools_observer()->preflight_request()->url);
  EXPECT_EQ("OPTIONS", devtools_observer()->preflight_request()->method);
  ASSERT_TRUE(devtools_observer()->preflight_response());
  ASSERT_TRUE(devtools_observer()->preflight_response()->headers);
  EXPECT_EQ(
      200, devtools_observer()->preflight_response()->headers->response_code());
  ASSERT_TRUE(devtools_observer()->preflight_status().has_value());
  EXPECT_EQ(net::OK, devtools_observer()->preflight_status()->error_code);
  EXPECT_EQ("TEST", devtools_observer()->initiator_devtools_request_id());
}

TEST_F(PreflightControllerTest, AuthorizationIsCoveredByWildcard) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/wildcard_headers");
  request.request_initiator = test_initiator_origin();
  request.headers.SetHeader("authorization", "foobar");

  SetNonWildcardRequestHeadersSupport(false);

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  EXPECT_EQ(status(), success());
  EXPECT_EQ(1u, access_count());
  EXPECT_TRUE(has_authorization_covered_by_wildcard());
}

TEST_F(PreflightControllerTest, AuthorizationIsNotCoveredByWildcard) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/wildcard_headers");
  request.request_initiator = test_initiator_origin();
  request.headers.SetHeader("authorization", "foobar");

  SetNonWildcardRequestHeadersSupport(true);

  PerformPreflightCheck(request);
  EXPECT_EQ(net::ERR_FAILED, net_error());
  ASSERT_NE(status(), success());
  EXPECT_EQ(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
            status()->cors_error);
  EXPECT_EQ(1u, access_count());
  EXPECT_TRUE(has_authorization_covered_by_wildcard());
}

TEST_F(PreflightControllerTest, CheckPreflightAccessDetectsErrorStatus) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const std::string allow_all_header("*");

  // Status 200-299 should pass.
  EXPECT_TRUE(PreflightController::CheckPreflightAccessForTesting(
                  response_url, 200, allow_all_header,
                  /*allow_credentials_header=*/std::nullopt,
                  network::mojom::CredentialsMode::kOmit, origin)
                  .has_value());
  EXPECT_TRUE(PreflightController::CheckPreflightAccessForTesting(
                  response_url, 299, allow_all_header,
                  /*allow_credentials_header=*/std::nullopt,
                  network::mojom::CredentialsMode::kOmit, origin)
                  .has_value());

  // Status 300 should fail.
  const auto result300 = PreflightController::CheckPreflightAccessForTesting(
      response_url, 300, allow_all_header,
      /*allow_credentials_header=*/std::nullopt,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result300.has_value());
  EXPECT_EQ(mojom::CorsError::kPreflightInvalidStatus,
            result300.error().cors_error);

  // Status 0 should fail too.
  const auto result0 = PreflightController::CheckPreflightAccessForTesting(
      response_url, 0, allow_all_header,
      /*allow_credentials_header=*/std::nullopt,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result0.has_value());
  EXPECT_EQ(mojom::CorsError::kPreflightInvalidStatus,
            result0.error().cors_error);
}

// TODO(crbug.com/40272627): Add test for private network access
// permission.

}  // namespace

}  // namespace network::cors
