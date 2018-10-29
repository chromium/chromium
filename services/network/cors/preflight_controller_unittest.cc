// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/preflight_timing_info.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

TEST(PreflightControllerCreatePreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");
  request.headers.SetHeader("Apple", "Red");
  request.headers.SetHeader("Kiwifruit", "Green");
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");
  request.headers.SetHeader("Strawberry", "Red");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(
      preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin, &header));
  EXPECT_EQ("null", header);

  EXPECT_TRUE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
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
  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, Credentials) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ(mojom::FetchCredentialsMode::kOmit,
            preflight->fetch_credentials_mode);
  EXPECT_TRUE(preflight->load_flags & net::LOAD_DO_NOT_SAVE_COOKIES);
  EXPECT_TRUE(preflight->load_flags & net::LOAD_DO_NOT_SEND_COOKIES);
  EXPECT_TRUE(preflight->load_flags & net::LOAD_DO_NOT_SEND_AUTH_DATA);
}

TEST(PreflightControllerCreatePreflightRequestTest,
     ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "text/plain");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  // Empty list also; see comment in test above.
  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("x-custom-header", header);
}

TEST(PreflightControllerCreatePreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("content-type", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeForbiddenHeaders) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("referer", "https://www.google.com/");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, Tainted) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request, true);

  std::string header;
  EXPECT_TRUE(
      preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin, &header));
  EXPECT_EQ(header, "null");
}

class PreflightControllerTest : public testing::Test {
 public:
  PreflightControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    mojom::NetworkServicePtr network_service_ptr;
    mojom::NetworkServiceRequest network_service_request =
        mojo::MakeRequest(&network_service_ptr);
    network_service_ = NetworkService::Create(
        std::move(network_service_request), nullptr /* net_log */);

    network_service_ptr->CreateNetworkContext(
        mojo::MakeRequest(&network_context_ptr_),
        mojom::NetworkContextParams::New());

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context_ptr_->CreateURLLoaderFactory(
        mojo::MakeRequest(&url_loader_factory_ptr_), std::move(params));
  }

 protected:
  void HandleRequestCompletion(
      int net_error,
      base::Optional<CORSErrorStatus> status,
      base::Optional<PreflightTimingInfo> timing_info) {
    net_error_ = net_error;
    status_ = status;
    run_loop_->Quit();
  }

  GURL GetURL(const std::string& path) { return test_server_.GetURL(path); }

  void PerformPreflightCheck(const ResourceRequest& request,
                             bool tainted = false) {
    DCHECK(preflight_controller_);
    run_loop_ = std::make_unique<base::RunLoop>();
    preflight_controller_->PerformPreflightCheck(
        base::BindOnce(&PreflightControllerTest::HandleRequestCompletion,
                       base::Unretained(this)),
        0 /* request_id */, request, tainted, TRAFFIC_ANNOTATION_FOR_TESTS,
        url_loader_factory_ptr_.get(),
        base::BindOnce(&PreflightControllerTest::CancelPreflight,
                       base::Unretained(this)));
    run_loop_->Run();
  }

  int net_error() const { return net_error_; }
  base::Optional<CORSErrorStatus> status() { return status_; }
  base::Optional<CORSErrorStatus> success() { return base::nullopt; }
  size_t access_count() { return access_count_; }
  bool cancel_preflight_called() const { return cancel_preflight_called_; }

 private:
  void SetUp() override {
    preflight_controller_ = std::make_unique<PreflightController>();

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
        net::test_server::ShouldHandle(request, "/tainted")) {
      response->set_code(net::test_server::ShouldHandle(request, "/404")
                             ? net::HTTP_NOT_FOUND
                             : net::HTTP_OK);
      const url::Origin origin =
          net::test_server::ShouldHandle(request, "/tainted")
              ? url::Origin()
              : url::Origin::Create(test_server_.base_url());
      response->AddCustomHeader(header_names::kAccessControlAllowOrigin,
                                origin.Serialize());
      response->AddCustomHeader(header_names::kAccessControlAllowMethods,
                                "GET, OPTIONS");
      response->AddCustomHeader(header_names::kAccessControlMaxAge, "1000");
      response->AddCustomHeader(net::HttpRequestHeaders::kCacheControl,
                                "no-store");
    }

    return response;
  }

  void CancelPreflight() { cancel_preflight_called_ = true; }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<mojom::NetworkService> network_service_;
  mojom::NetworkContextPtr network_context_ptr_;
  mojom::URLLoaderFactoryPtr url_loader_factory_ptr_;

  net::test_server::EmbeddedTestServer test_server_;
  size_t access_count_ = 0;
  bool cancel_preflight_called_ = false;

  std::unique_ptr<PreflightController> preflight_controller_;
  int net_error_ = net::OK;
  base::Optional<CORSErrorStatus> status_;
};

TEST_F(PreflightControllerTest, CheckInvalidRequest) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.url = GetURL("/404");
  request.request_initiator = url::Origin::Create(request.url);

  PerformPreflightCheck(request);
  EXPECT_EQ(net::ERR_FAILED, net_error());
  ASSERT_TRUE(status());
  EXPECT_EQ(mojom::CORSError::kPreflightInvalidStatus, status()->cors_error);
  EXPECT_EQ(1u, access_count());
}

TEST_F(PreflightControllerTest, CheckValidRequest) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.url = GetURL("/allow");
  request.request_initiator = url::Origin::Create(request.url);

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.
}

TEST_F(PreflightControllerTest, CheckTaintedRequest) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.url = GetURL("/tainted");
  request.request_initiator = url::Origin::Create(request.url);

  PerformPreflightCheck(request, true /* tainted */);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());
}

// TODO(yhirano): Remove this test case when the network service is fully
// enabled.
TEST_F(PreflightControllerTest, CancelPreflightIsCalled) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.url = GetURL("/allow");
  request.request_initiator = url::Origin::Create(request.url);

  EXPECT_FALSE(cancel_preflight_called());
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_TRUE(cancel_preflight_called());
  EXPECT_EQ(1u, access_count());
}

}  // namespace

}  // namespace cors

}  // namespace network
