// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_response.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "google_apis/gaia/oauth2_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gaia {

namespace {

using testing::_;

constexpr char kOAuth2MintTokenResponseHistogram[] =
    "Signin.OAuth2MintToken.Response";

class MockMintTokenFlowDelegate : public OAuth2MintTokenFlow::Delegate {
 public:
  MockMintTokenFlowDelegate() = default;
  ~MockMintTokenFlowDelegate() override = default;

  MOCK_METHOD(void,
              OnMintTokenSuccess,
              (const OAuth2MintTokenFlow::MintTokenResult& result),
              ());
  MOCK_METHOD(void,
              OnMintTokenFailure,
              (const GoogleServiceAuthError& error),
              (override));
  MOCK_METHOD(void,
              OnRemoteConsentSuccess,
              (const RemoteConsentResolutionData& resolution_data),
              (override));
};

class MockAccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockAccessTokenConsumer() = default;
  ~MockAccessTokenConsumer() override = default;

  std::string GetConsumerName() const override { return "test_consumer"; }

  MOCK_METHOD(void,
              OnGetTokenSuccess,
              (const TokenResponse& token_response),
              (override));
  MOCK_METHOD(void,
              OnGetTokenFailure,
              (const GoogleServiceAuthError& error),
              (override));
};

OAuth2MintTokenFlow::Parameters CreateIssueTokenFlowParameters() {
  return OAuth2MintTokenFlow::Parameters::CreateForClientFlow(
      "test_client_id", {"test_scope"}, "test_version", "test_channel");
}

}  // namespace

class FakeOAuth2TokenResponseTest : public testing::Test {
 public:
  FakeOAuth2TokenResponseTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(FakeOAuth2TokenResponseTest, GetTokenSuccess) {
  MockAccessTokenConsumer consumer;
  base::RunLoop run_loop;
  EXPECT_CALL(consumer, OnGetTokenSuccess).WillOnce([&] { run_loop.Quit(); });
  std::unique_ptr<GaiaAccessTokenFetcher> fetcher =
      GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
          &consumer, test_url_loader_factory_.GetSafeWeakWrapper(),
          "refresh_token");

  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::Success("access_token");
  response.AddToTestURLLoaderFactory(
      test_url_loader_factory_,
      FakeOAuth2TokenResponse::ApiEndpoint::kGetToken);
  base::HistogramTester histogram_tester;

  fetcher->Start("client_id", "client_secret", {});
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName, OAuth2Response::kOk,
      1);
}

TEST_F(FakeOAuth2TokenResponseTest, GetTokenNetError) {
  MockAccessTokenConsumer consumer;
  base::RunLoop run_loop;
  EXPECT_CALL(consumer, OnGetTokenFailure).WillOnce([&] { run_loop.Quit(); });
  std::unique_ptr<GaiaAccessTokenFetcher> fetcher =
      GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
          &consumer, test_url_loader_factory_.GetSafeWeakWrapper(),
          "refresh_token");

  const net::Error net_error = net::ERR_FAILED;
  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::NetError(net_error);
  response.AddToTestURLLoaderFactory(
      test_url_loader_factory_,
      FakeOAuth2TokenResponse::ApiEndpoint::kGetToken);
  base::HistogramTester histogram_tester;

  fetcher->Start("client_id", "client_secret", {});
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2NetResponseCodeHistogramName, net_error,
      1);
}

class FakeOAuth2TokenResponseGetTokenErrorTest
    : public FakeOAuth2TokenResponseTest,
      public testing::WithParamInterface<OAuth2Response> {};

TEST_P(FakeOAuth2TokenResponseGetTokenErrorTest, GetTokenFailure) {
  MockAccessTokenConsumer consumer;
  base::RunLoop run_loop;
  EXPECT_CALL(consumer, OnGetTokenFailure).WillOnce([&] { run_loop.Quit(); });
  std::unique_ptr<GaiaAccessTokenFetcher> fetcher =
      GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
          &consumer, test_url_loader_factory_.GetSafeWeakWrapper(),
          "refresh_token");

  const OAuth2Response error = GetParam();
  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::OAuth2Error(error);
  response.AddToTestURLLoaderFactory(
      test_url_loader_factory_,
      FakeOAuth2TokenResponse::ApiEndpoint::kGetToken);
  base::HistogramTester histogram_tester;

  fetcher->Start("client_id", "client_secret", {});
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName, error, 1);
}

constexpr OAuth2Response kGetTokenErrorTestParams[] = {
    OAuth2Response::kUnknownError,
    OAuth2Response::kOkUnexpectedFormat,
    OAuth2Response::kErrorUnexpectedFormat,
    OAuth2Response::kInvalidRequest,
    OAuth2Response::kInvalidClient,
    OAuth2Response::kInvalidGrant,
    OAuth2Response::kUnauthorizedClient,
    OAuth2Response::kUnsuportedGrantType,
    OAuth2Response::kInvalidScope,
    OAuth2Response::kRestrictedClient,
    OAuth2Response::kRateLimitExceeded,
    OAuth2Response::kInternalFailure,
    OAuth2Response::kAdminPolicyEnforced,
    OAuth2Response::kAccessDenied,
};

constexpr OAuth2Response kNotTestedGetTokenErrors[] = {
    OAuth2Response::kOk,
    OAuth2Response::kTokenBindingChallenge,
    OAuth2Response::kConsentRequired,
};

// Please update this static_assert accordingly when updating `OAuth2Response`.
static_assert(std::size(kGetTokenErrorTestParams) ==
              static_cast<size_t>(OAuth2Response::kMaxValue) + 1 -
                  std::size(kNotTestedGetTokenErrors));

INSTANTIATE_TEST_SUITE_P(
    ,
    FakeOAuth2TokenResponseGetTokenErrorTest,
    testing::ValuesIn(kGetTokenErrorTestParams),
    [](const testing::TestParamInfo<OAuth2Response>& info) {
      return "Error" + base::NumberToString(static_cast<int>(info.param));
    });

TEST_F(FakeOAuth2TokenResponseTest, IssueTokenSuccess) {
  const std::string access_token = "access_token";
  const base::TimeDelta expires_in = base::Hours(1);
  MockMintTokenFlowDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(
      delegate,
      OnMintTokenSuccess(testing::AllOf(
          testing::Field(&OAuth2MintTokenFlow::MintTokenResult::access_token,
                         access_token),
          testing::Field(&OAuth2MintTokenFlow::MintTokenResult::time_to_live,
                         expires_in))))
      .WillOnce([&]() { run_loop.Quit(); });
  OAuth2MintTokenFlow flow(&delegate, CreateIssueTokenFlowParameters());

  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::Success(access_token, expires_in);
  response.AddToTestURLLoaderFactory(
      test_url_loader_factory_,
      FakeOAuth2TokenResponse::ApiEndpoint::kIssueToken);
  base::HistogramTester histogram_tester;

  flow.Start(test_url_loader_factory_.GetSafeWeakWrapper(), access_token);
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                      OAuth2Response::kOk, 1);
}

TEST_F(FakeOAuth2TokenResponseTest, IssueTokenNetError) {
  MockMintTokenFlowDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnMintTokenFailure).WillOnce([&] { run_loop.Quit(); });
  OAuth2MintTokenFlow flow(&delegate, CreateIssueTokenFlowParameters());

  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::NetError(net::ERR_FAILED);
  response.AddToTestURLLoaderFactory(
      test_url_loader_factory_,
      FakeOAuth2TokenResponse::ApiEndpoint::kIssueToken);

  flow.Start(test_url_loader_factory_.GetSafeWeakWrapper(), "access_token");
  run_loop.Run();
}

class FakeOAuth2TokenResponseIssueTokenErrorTest
    : public FakeOAuth2TokenResponseTest,
      public testing::WithParamInterface<OAuth2Response> {};

TEST_P(FakeOAuth2TokenResponseIssueTokenErrorTest,
       MapsToCorrectGoogleServiceAuthError) {
  MockMintTokenFlowDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnMintTokenFailure).WillOnce([&] { run_loop.Quit(); });
  OAuth2MintTokenFlow flow(&delegate, CreateIssueTokenFlowParameters());

  const OAuth2Response error = GetParam();
  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::OAuth2Error(error);
  response.AddToTestURLLoaderFactory(
      test_url_loader_factory_,
      FakeOAuth2TokenResponse::ApiEndpoint::kIssueToken);
  base::HistogramTester histogram_tester;

  flow.Start(test_url_loader_factory_.GetSafeWeakWrapper(), "access_token");
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram, error,
                                      1);
}

constexpr OAuth2Response kIssueTokenErrorTestParams[] = {
    OAuth2Response::kUnknownError,          OAuth2Response::kOkUnexpectedFormat,
    OAuth2Response::kErrorUnexpectedFormat, OAuth2Response::kInvalidRequest,
    OAuth2Response::kInvalidClient,         OAuth2Response::kInvalidGrant,
    OAuth2Response::kInvalidScope,          OAuth2Response::kRestrictedClient,
    OAuth2Response::kRateLimitExceeded,     OAuth2Response::kInternalFailure};

constexpr OAuth2Response kNotTestedIssueTokenErrors[] = {
    OAuth2Response::kOk,
    OAuth2Response::kTokenBindingChallenge,
    OAuth2Response::kConsentRequired,
    OAuth2Response::kUnauthorizedClient,
    OAuth2Response::kUnsuportedGrantType,
    OAuth2Response::kAdminPolicyEnforced,
    OAuth2Response::kAccessDenied,
};

// Please update this static_assert accordingly when updating `OAuth2Response`.
static_assert(std::size(kIssueTokenErrorTestParams) ==
              static_cast<size_t>(OAuth2Response::kMaxValue) + 1 -
                  std::size(kNotTestedIssueTokenErrors));

INSTANTIATE_TEST_SUITE_P(
    ,
    FakeOAuth2TokenResponseIssueTokenErrorTest,
    testing::ValuesIn(kIssueTokenErrorTestParams),
    [](const testing::TestParamInfo<OAuth2Response>& info) {
      return "Error" + base::NumberToString(static_cast<int>(info.param));
    });

TEST_F(FakeOAuth2TokenResponseTest, BothEndpointsSuccess) {
  MockAccessTokenConsumer consumer;
  base::RunLoop get_token_run_loop;
  EXPECT_CALL(consumer, OnGetTokenSuccess).WillOnce([&] {
    get_token_run_loop.Quit();
  });
  std::unique_ptr<GaiaAccessTokenFetcher> get_token_fetcher =
      GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
          &consumer, test_url_loader_factory_.GetSafeWeakWrapper(),
          "refresh_token");

  MockMintTokenFlowDelegate delegate;
  base::RunLoop issue_token_run_loop;
  EXPECT_CALL(delegate, OnMintTokenSuccess).WillOnce([&]() {
    issue_token_run_loop.Quit();
  });
  OAuth2MintTokenFlow issue_token_fetcher(&delegate,
                                          CreateIssueTokenFlowParameters());

  FakeOAuth2TokenResponse response =
      FakeOAuth2TokenResponse::Success("access_token");
  response.AddToTestURLLoaderFactory(test_url_loader_factory_);

  base::HistogramTester histogram_tester;
  get_token_fetcher->Start("client_id", "client_secret", {});
  issue_token_fetcher.Start(test_url_loader_factory_.GetSafeWeakWrapper(),
                            "refresh_token");
  get_token_run_loop.Run();
  issue_token_run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      GaiaAccessTokenFetcher::kOAuth2ResponseHistogramName, OAuth2Response::kOk,
      1);
  histogram_tester.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                      OAuth2Response::kOk, 1);
}

}  // namespace gaia
