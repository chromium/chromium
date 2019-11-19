// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2AccessTokenFetcherImpl.

#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace {

using ScopeList = std::vector<std::string>;

constexpr char kValidTokenResponse[] = R"(
    {
      "access_token": "at1",
      "expires_in": 3600,
      "token_type": "Bearer",
      "id_token": "id_token"
    })";

constexpr char kTokenResponseNoAccessToken[] = R"(
    {
      "expires_in": 3600,
      "token_type": "Bearer"
    })";

constexpr char kValidFailureTokenResponse[] = R"(
    {
      "error": "invalid_grant"
    })";

class MockOAuth2AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuth2AccessTokenConsumer() {}
  ~MockOAuth2AccessTokenConsumer() override {}

  MOCK_METHOD1(OnGetTokenSuccess,
               void(const OAuth2AccessTokenConsumer::TokenResponse&));
  MOCK_METHOD1(OnGetTokenFailure, void(const GoogleServiceAuthError& error));
};

class URLLoaderFactoryInterceptor {
 public:
  MOCK_METHOD1(Intercept, void(const network::ResourceRequest&));
};

MATCHER_P(resourceRequestUrlEquals, url, "") {
  return arg.url == url;
}

}  // namespace

class OAuth2AccessTokenFetcherImplTest : public testing::Test {
 public:
  OAuth2AccessTokenFetcherImplTest()
      : fetcher_(&consumer_,
                 url_loader_factory_.GetSafeWeakWrapper(),
                 "refresh_token") {
    url_loader_factory_.SetInterceptor(base::BindRepeating(
        &URLLoaderFactoryInterceptor::Intercept,
        base::Unretained(&url_loader_factory_interceptor_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetupGetAccessToken(int net_error_code,
                           net::HttpStatusCode http_response_code,
                           const std::string& body) {
    GURL url(GaiaUrls::GetInstance()->oauth2_token_url());
    if (net_error_code == net::OK) {
      url_loader_factory_.AddResponse(url.spec(), body, http_response_code);
    } else {
      url_loader_factory_.AddResponse(
          url, network::mojom::URLResponseHead::New(), body,
          network::URLLoaderCompletionStatus(net_error_code));
    }

    EXPECT_CALL(url_loader_factory_interceptor_,
                Intercept(resourceRequestUrlEquals(url)));
  }

  void SetupProxyError() {
    GURL url(GaiaUrls::GetInstance()->oauth2_token_url());
    url_loader_factory_.AddResponse(
        url,
        network::CreateURLResponseHead(net::HTTP_PROXY_AUTHENTICATION_REQUIRED),
        std::string(),
        network::URLLoaderCompletionStatus(net::ERR_TUNNEL_CONNECTION_FAILED),
        network::TestURLLoaderFactory::Redirects(),
        network::TestURLLoaderFactory::kSendHeadersOnNetworkError);

    EXPECT_CALL(url_loader_factory_interceptor_,
                Intercept(resourceRequestUrlEquals(url)));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockOAuth2AccessTokenConsumer consumer_;
  URLLoaderFactoryInterceptor url_loader_factory_interceptor_;
  network::TestURLLoaderFactory url_loader_factory_;
  OAuth2AccessTokenFetcherImpl fetcher_;
};

// These four tests time out, see http://crbug.com/113446.
TEST_F(OAuth2AccessTokenFetcherImplTest, GetAccessTokenRequestFailure) {
  SetupGetAccessToken(net::ERR_FAILED, net::HTTP_OK, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2AccessTokenFetcherImplTest, GetAccessTokenResponseCodeFailure) {
  SetupGetAccessToken(net::OK, net::HTTP_FORBIDDEN, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
}

// Regression test for https://crbug.com/914672
TEST_F(OAuth2AccessTokenFetcherImplTest, ProxyFailure) {
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(
          net::ERR_TUNNEL_CONNECTION_FAILED);
  ASSERT_TRUE(expected_error.IsTransientError());
  SetupProxyError();
  EXPECT_CALL(consumer_, OnGetTokenFailure(expected_error)).Times(1);
  fetcher_.Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2AccessTokenFetcherImplTest, Success) {
  SetupGetAccessToken(net::OK, net::HTTP_OK, kValidTokenResponse);
  EXPECT_CALL(consumer_, OnGetTokenSuccess(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2AccessTokenFetcherImplTest, MakeGetAccessTokenBodyNoScope) {
  std::string body =
      "client_id=cid1&"
      "client_secret=cs1&"
      "grant_type=refresh_token&"
      "refresh_token=rt1";
  EXPECT_EQ(body, OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenBody(
                      "cid1", "cs1", "rt1", ScopeList()));
}

TEST_F(OAuth2AccessTokenFetcherImplTest, MakeGetAccessTokenBodyOneScope) {
  std::string body =
      "client_id=cid1&"
      "client_secret=cs1&"
      "grant_type=refresh_token&"
      "refresh_token=rt1&"
      "scope=https://www.googleapis.com/foo";
  ScopeList scopes = {"https://www.googleapis.com/foo"};
  EXPECT_EQ(body, OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenBody(
                      "cid1", "cs1", "rt1", scopes));
}

TEST_F(OAuth2AccessTokenFetcherImplTest, MakeGetAccessTokenBodyMultipleScopes) {
  std::string body =
      "client_id=cid1&"
      "client_secret=cs1&"
      "grant_type=refresh_token&"
      "refresh_token=rt1&"
      "scope=https://www.googleapis.com/foo+"
      "https://www.googleapis.com/bar+"
      "https://www.googleapis.com/baz";
  ScopeList scopes = {"https://www.googleapis.com/foo",
                      "https://www.googleapis.com/bar",
                      "https://www.googleapis.com/baz"};
  EXPECT_EQ(body, OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenBody(
                      "cid1", "cs1", "rt1", scopes));
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenResponseNoBody) {
  std::string at;
  int expires_in;
  std::string id_token;
  auto empty_body = std::make_unique<std::string>("");
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      std::move(empty_body), &at, &expires_in, &id_token));
  EXPECT_TRUE(at.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenResponseBadJson) {
  std::string at;
  int expires_in;
  std::string id_token;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      std::make_unique<std::string>("foo"), &at, &expires_in, &id_token));
  EXPECT_TRUE(at.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest,
       ParseGetAccessTokenResponseNoAccessToken) {
  std::string at;
  int expires_in;
  std::string id_token;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      std::make_unique<std::string>(kTokenResponseNoAccessToken), &at,
      &expires_in, &id_token));
  EXPECT_TRUE(at.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenResponseSuccess) {
  std::string at;
  int expires_in;
  std::string id_token;
  EXPECT_TRUE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      std::make_unique<std::string>(kValidTokenResponse), &at, &expires_in,
      &id_token));
  EXPECT_EQ("at1", at);
  EXPECT_EQ(3600, expires_in);
  EXPECT_EQ("id_token", id_token);
}

TEST_F(OAuth2AccessTokenFetcherImplTest,
       ParseGetAccessTokenFailureInvalidError) {
  std::string error;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
      std::make_unique<std::string>(kTokenResponseNoAccessToken), &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenFailure) {
  std::string error;
  EXPECT_TRUE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
      std::make_unique<std::string>(kValidFailureTokenResponse), &error));
  EXPECT_EQ("invalid_grant", error);
}
