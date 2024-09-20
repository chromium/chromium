// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2AccessTokenFetcherImpl.

#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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

constexpr char kRaptRequiredErrorResponse[] = R"(
    {
      "error": "invalid_grant",
      "error_subtype": "rapt_required",
      "error_description": "reauth related error"
    })";

constexpr char kInvalidRaptErrorResponse[] = R"(
    {
      "error": "invalid_grant",
      "error_subtype": "invalid_rapt",
      "error_description": "reauth related error"
    })";

class MockOAuth2AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuth2AccessTokenConsumer() = default;
  ~MockOAuth2AccessTokenConsumer() override = default;

  MOCK_METHOD1(OnGetTokenSuccess,
               void(const OAuth2AccessTokenConsumer::TokenResponse&));
  MOCK_METHOD1(OnGetTokenFailure, void(const GoogleServiceAuthError& error));

  std::string GetConsumerName() const override {
    return "oauth2_access_token_fetcher_impl_unittest";
  }
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
      : fetcher_(GaiaAccessTokenFetcher::
                     CreateExchangeRefreshTokenForAccessTokenInstance(
                         &consumer_,
                         url_loader_factory_.GetSafeWeakWrapper(),
                         "refresh_token")) {
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

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockOAuth2AccessTokenConsumer consumer_;
  URLLoaderFactoryInterceptor url_loader_factory_interceptor_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<GaiaAccessTokenFetcher> fetcher_;
  base::HistogramTester histogram_tester_;
};

// These four tests time out, see http://crbug.com/113446.
TEST_F(OAuth2AccessTokenFetcherImplTest, GetAccessTokenRequestFailure) {
  SetupGetAccessToken(net::ERR_FAILED, net::HTTP_OK, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2NetResponseCodeHistogramName,
      net::ERR_FAILED, 1);
  histogram_tester()->ExpectTotalCount(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName, 0);
}

TEST_F(OAuth2AccessTokenFetcherImplTest, GetAccessTokenResponseCodeFailure) {
  SetupGetAccessToken(net::OK, net::HTTP_FORBIDDEN, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
  // Error tag does not exist in the response body.
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName,
      OAuth2AccessTokenFetcherImpl::kErrorUnexpectedFormat, 1);
}

// Regression test for https://crbug.com/914672
TEST_F(OAuth2AccessTokenFetcherImplTest, ProxyFailure) {
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(
          net::ERR_TUNNEL_CONNECTION_FAILED);
  ASSERT_TRUE(expected_error.IsTransientError());
  SetupProxyError();
  EXPECT_CALL(consumer_, OnGetTokenFailure(expected_error)).Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OAuth2AccessTokenFetcherImplTest, Success) {
  SetupGetAccessToken(net::OK, net::HTTP_OK, kValidTokenResponse);
  EXPECT_CALL(consumer_, OnGetTokenSuccess(_)).Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2NetResponseCodeHistogramName, net::HTTP_OK,
      1);
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName,
      OAuth2AccessTokenFetcherImpl::kOk, 1);
}

TEST_F(OAuth2AccessTokenFetcherImplTest, SuccessUnexpectedFormat) {
  SetupGetAccessToken(net::OK, net::HTTP_OK, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(GoogleServiceAuthError(
                             GoogleServiceAuthError::SERVICE_UNAVAILABLE)))
      .Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2NetResponseCodeHistogramName, net::HTTP_OK,
      1);
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName,
      OAuth2AccessTokenFetcherImpl::kOkUnexpectedFormat, 1);
}

TEST_F(OAuth2AccessTokenFetcherImplTest, CancelOngoingRequest) {
  SetupGetAccessToken(net::OK, net::HTTP_OK, kValidTokenResponse);
  // `OnGetTokenSuccess()` should not be called.
  EXPECT_CALL(consumer_, OnGetTokenSuccess(_)).Times(0);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  fetcher_->CancelRequest();
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(OAuth2AccessTokenFetcherImplTest, GetAccessTokenRaptRequiredFailure) {
  SetupGetAccessToken(net::OK, net::HTTP_BAD_REQUEST,
                      kRaptRequiredErrorResponse);
  EXPECT_CALL(consumer_,
              OnGetTokenFailure(
                  GoogleServiceAuthError::FromScopeLimitedUnrecoverableError(
                      "reauth related error")))
      .Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(OAuth2AccessTokenFetcherImplTest, MakeGetAccessTokenBodyNoScope) {
  std::string body =
      "client_id=cid1&"
      "client_secret=cs1&"
      "grant_type=refresh_token&"
      "refresh_token=rt1";
  EXPECT_EQ(body, OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenBody(
                      "cid1", "cs1", "rt1", std::string(), ScopeList()));
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
                      "cid1", "cs1", "rt1", std::string(), scopes));
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
                      "cid1", "cs1", "rt1", std::string(), scopes));
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenResponseNoBody) {
  OAuth2AccessTokenConsumer::TokenResponse token_response;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      "", &token_response));
  EXPECT_TRUE(token_response.access_token.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenResponseBadJson) {
  OAuth2AccessTokenConsumer::TokenResponse token_response;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      "foo", &token_response));
  EXPECT_TRUE(token_response.access_token.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest,
       ParseGetAccessTokenResponseNoAccessToken) {
  OAuth2AccessTokenConsumer::TokenResponse token_response;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      kTokenResponseNoAccessToken, &token_response));
  EXPECT_TRUE(token_response.access_token.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenResponseSuccess) {
  OAuth2AccessTokenConsumer::TokenResponse token_response;
  EXPECT_TRUE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
      kValidTokenResponse, &token_response));
  EXPECT_EQ("at1", token_response.access_token);
  base::TimeDelta expires_in =
      token_response.expiration_time - base::Time::Now();
  EXPECT_LT(0, expires_in.InSeconds());
  EXPECT_GT(3600, expires_in.InSeconds());
  EXPECT_EQ("id_token", token_response.id_token);
}

TEST_F(OAuth2AccessTokenFetcherImplTest,
       ParseGetAccessTokenFailureInvalidError) {
  std::string error, error_subtype, error_description;
  EXPECT_FALSE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
      kTokenResponseNoAccessToken, &error, &error_subtype, &error_description));
  EXPECT_TRUE(error.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest, ParseGetAccessTokenFailure) {
  std::string error, error_subtype, error_description;
  EXPECT_TRUE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
      kValidFailureTokenResponse, &error, &error_subtype, &error_description));
  EXPECT_EQ("invalid_grant", error);
  EXPECT_TRUE(error_subtype.empty());
  EXPECT_TRUE(error_description.empty());
}

TEST_F(OAuth2AccessTokenFetcherImplTest,
       ParseGetAccessTokenFailureForMissingRaptError) {
  std::string error, error_subtype, error_description;
  EXPECT_TRUE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
      kRaptRequiredErrorResponse, &error, &error_subtype, &error_description));
  EXPECT_EQ("invalid_grant", error);
  EXPECT_EQ("rapt_required", error_subtype);
  EXPECT_EQ("reauth related error", error_description);
}

TEST_F(OAuth2AccessTokenFetcherImplTest,
       ParseGetAccessTokenFailureForInvalidRaptError) {
  std::string error, error_subtype, error_description;
  EXPECT_TRUE(OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
      kInvalidRaptErrorResponse, &error, &error_subtype, &error_description));
  EXPECT_EQ("invalid_grant", error);
  EXPECT_EQ("invalid_rapt", error_subtype);
  EXPECT_EQ("reauth related error", error_description);
}

struct OAuth2ErrorCodesTestParam {
  const char* error_code;
  net::HttpStatusCode httpStatusCode;
  OAuth2AccessTokenFetcherImpl::OAuth2Response expected_sample;
  GoogleServiceAuthError::State expected_error_state;
};

class OAuth2ErrorCodesTest
    : public OAuth2AccessTokenFetcherImplTest,
      public ::testing::WithParamInterface<OAuth2ErrorCodesTestParam> {
 public:
  OAuth2ErrorCodesTest() = default;

  OAuth2ErrorCodesTest(const OAuth2ErrorCodesTest&) = delete;
  OAuth2ErrorCodesTest& operator=(const OAuth2ErrorCodesTest&) = delete;

  GoogleServiceAuthError GetGoogleServiceAuthError(
      const std::string& error_message) {
    switch (GetParam().expected_error_state) {
      case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
        return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER);
      case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
        return GoogleServiceAuthError::FromServiceUnavailable(error_message);
      case GoogleServiceAuthError::SERVICE_ERROR:
        return GoogleServiceAuthError::FromServiceError(error_message);
      case GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
        return GoogleServiceAuthError::FromScopeLimitedUnrecoverableError(
            error_message);

      case GoogleServiceAuthError::NONE:
      case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      case GoogleServiceAuthError::CONNECTION_FAILED:
      case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
      case GoogleServiceAuthError::REQUEST_CANCELED:
      case GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED:
      case GoogleServiceAuthError::NUM_STATES:
        NOTREACHED();
    }
  }
};

const OAuth2ErrorCodesTestParam kOAuth2ErrorCodesTable[] = {
    {"invalid_request", net::HTTP_BAD_REQUEST,
     OAuth2AccessTokenFetcherImpl::kInvalidRequest,
     GoogleServiceAuthError::SERVICE_ERROR},
    {"invalid_client", net::HTTP_UNAUTHORIZED,
     OAuth2AccessTokenFetcherImpl::kInvalidClient,
     GoogleServiceAuthError::SERVICE_ERROR},
    {"invalid_grant", net::HTTP_BAD_REQUEST,
     OAuth2AccessTokenFetcherImpl::kInvalidGrant,
     GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS},
    {"unauthorized_client", net::HTTP_UNAUTHORIZED,
     OAuth2AccessTokenFetcherImpl::kUnauthorizedClient,
     GoogleServiceAuthError::SERVICE_ERROR},
    {"unsupported_grant_type", net::HTTP_BAD_REQUEST,
     OAuth2AccessTokenFetcherImpl::kUnsuportedGrantType,
     GoogleServiceAuthError::SERVICE_ERROR},
    {"invalid_scope", net::HTTP_BAD_REQUEST,
     OAuth2AccessTokenFetcherImpl::kInvalidScope,
     GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR},
    {"restricted_client", net::HTTP_FORBIDDEN,
     OAuth2AccessTokenFetcherImpl::kRestrictedClient,
     GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR},
    {"rate_limit_exceeded", net::HTTP_FORBIDDEN,
     OAuth2AccessTokenFetcherImpl::kRateLimitExceeded,
     GoogleServiceAuthError::SERVICE_UNAVAILABLE},
    {"internal_failure", net::HTTP_INTERNAL_SERVER_ERROR,
     OAuth2AccessTokenFetcherImpl::kInternalFailure,
     GoogleServiceAuthError::SERVICE_UNAVAILABLE},
    {"", net::HTTP_BAD_REQUEST,
     OAuth2AccessTokenFetcherImpl::kErrorUnexpectedFormat,
     GoogleServiceAuthError::SERVICE_ERROR},
    {"", net::HTTP_OK, OAuth2AccessTokenFetcherImpl::kOkUnexpectedFormat,
     GoogleServiceAuthError::SERVICE_UNAVAILABLE},
    {"unknown_error", net::HTTP_BAD_REQUEST,
     OAuth2AccessTokenFetcherImpl::kUnknownError,
     GoogleServiceAuthError::SERVICE_ERROR},
    {"unknown_error", net::HTTP_INTERNAL_SERVER_ERROR,
     OAuth2AccessTokenFetcherImpl::kUnknownError,
     GoogleServiceAuthError::SERVICE_UNAVAILABLE}};

TEST_P(OAuth2ErrorCodesTest, TableRowTest) {
  std::string response_body = base::StringPrintf(R"(
    {
      "error": "%s"
    })",
                                                 GetParam().error_code);
  GoogleServiceAuthError expected_error =
      GetGoogleServiceAuthError(response_body);
  SetupGetAccessToken(net::OK, GetParam().httpStatusCode, response_body);
  EXPECT_CALL(consumer_, OnGetTokenFailure(expected_error)).Times(1);
  fetcher_->Start("client_id", "client_secret", ScopeList());
  base::RunLoop().RunUntilIdle();
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName,
      GetParam().expected_sample, 1);
  histogram_tester()->ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2NetResponseCodeHistogramName,
      GetParam().httpStatusCode, 1);
}

INSTANTIATE_TEST_SUITE_P(OAuth2ErrorCodesTableTest,
                         OAuth2ErrorCodesTest,
                         ::testing::ValuesIn(kOAuth2ErrorCodesTable));
