// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFlow.

#include "google_apis/gaia/oauth2_api_call_flow.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::HttpRequestHeaders;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

namespace {

const char kAccessToken[] = "access_token";

static std::string CreateBody() {
  return "some body";
}

static GURL CreateApiUrl() {
  return GURL("https://www.googleapis.com/someapi");
}


class MockApiCallFlow : public OAuth2ApiCallFlow {
 public:
  MockApiCallFlow() {}
  ~MockApiCallFlow() override {}

  MOCK_METHOD0(CreateApiCallUrl, GURL());
  MOCK_METHOD0(CreateApiCallBody, std::string());
  MOCK_METHOD0(CreateApiCallHeaders, net::HttpRequestHeaders());
  MOCK_METHOD2(ProcessApiCallSuccess,
               void(const network::mojom::URLResponseHead* head,
                    std::unique_ptr<std::string> body));
  MOCK_METHOD3(ProcessApiCallFailure,
               void(int net_error,
                    const network::mojom::URLResponseHead* head,
                    std::unique_ptr<std::string> body));
  MOCK_METHOD1(ProcessNewAccessToken, void(const std::string& access_token));
  MOCK_METHOD1(ProcessMintAccessTokenFailure,
               void(const GoogleServiceAuthError& error));

  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override {
    return PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS;
  }
};

}  // namespace

class OAuth2ApiCallFlowTest : public testing::Test {
 protected:
  OAuth2ApiCallFlowTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void AddFetchResult(const GURL& url,
                      bool fetch_succeeds,
                      net::HttpStatusCode response_code,
                      const std::string& body) {
    net::Error error = fetch_succeeds ? net::OK : net::ERR_FAILED;

    auto http_head = network::CreateURLResponseHead(response_code);
    test_url_loader_factory_.AddResponse(
        url, std::move(http_head), body,
        network::URLLoaderCompletionStatus(error));
  }

 protected:
  void SetupApiCall(bool succeeds, net::HttpStatusCode status) {
    std::string body(CreateBody());
    GURL url(CreateApiUrl());
    EXPECT_CALL(flow_, CreateApiCallBody()).WillOnce(Return(body));
    EXPECT_CALL(flow_, CreateApiCallUrl()).WillOnce(Return(url));
    EXPECT_CALL(flow_, CreateApiCallHeaders());

    AddFetchResult(url, succeeds, status, std::string());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  StrictMock<MockApiCallFlow> flow_;
};

TEST_F(OAuth2ApiCallFlowTest, ApiCallSucceedsHttpOk) {
  SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(flow_, ProcessApiCallSuccess(_, _));
  flow_.Start(shared_factory_, kAccessToken);
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2ApiCallFlowTest, ApiCallSucceedsHttpNoContent) {
  SetupApiCall(true, net::HTTP_NO_CONTENT);
  EXPECT_CALL(flow_, ProcessApiCallSuccess(_, _));
  flow_.Start(shared_factory_, kAccessToken);
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2ApiCallFlowTest, ApiCallFailure) {
  SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  EXPECT_CALL(flow_, ProcessApiCallFailure(_, _, _));
  flow_.Start(shared_factory_, kAccessToken);
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2ApiCallFlowTest, ExpectedHTTPHeaders) {
  std::string body = CreateBody();
  GURL url(CreateApiUrl());

  SetupApiCall(true, net::HTTP_OK);
  // ... never mind the HTTP response part of the setup --- don't want
  // TestURLLoaderFactory replying to it just yet as it would prevent examining
  // the request headers.
  test_url_loader_factory_.ClearResponses();

  flow_.Start(shared_factory_, kAccessToken);
  const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
      *test_url_loader_factory_.pending_requests();
  ASSERT_EQ(1u, pending.size());
  EXPECT_EQ(url, pending[0].request.url);

  EXPECT_THAT(pending[0].request.headers.GetHeader("Authorization"),
              testing::Optional(std::string("Bearer access_token")));
  EXPECT_EQ(body, network::GetUploadData(pending[0].request));
}

net::HttpRequestHeaders CreateHeaders() {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Test-Header-Field", "test content");
  return headers;
}

TEST_F(OAuth2ApiCallFlowTest, ExpectedMultipleHTTPHeaders) {
  std::string body = CreateBody();
  GURL url(CreateApiUrl());
  SetupApiCall(true, net::HTTP_OK);

  // Overwrite EXPECT_CALL default return so that we get multiple headers.
  ON_CALL(flow_, CreateApiCallHeaders).WillByDefault(Return(CreateHeaders()));

  // ... never mind the HTTP response part of the setup --- don't want
  // TestURLLoaderFactory replying to it just yet as it would prevent examining
  // the request headers.
  test_url_loader_factory_.ClearResponses();

  flow_.Start(shared_factory_, kAccessToken);
  const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
      *test_url_loader_factory_.pending_requests();
  ASSERT_EQ(1u, pending.size());
  EXPECT_EQ(url, pending[0].request.url);

  const auto& headers = pending[0].request.headers;
  EXPECT_THAT(headers.GetHeader("Authorization"),
              testing::Optional(std::string("Bearer access_token")));
  EXPECT_THAT(headers.GetHeader("Test-Header-Field"),
              testing::Optional(std::string("test content")));

  EXPECT_EQ(body, network::GetUploadData(pending[0].request));
}
