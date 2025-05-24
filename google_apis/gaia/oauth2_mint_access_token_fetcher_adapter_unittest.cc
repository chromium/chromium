// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_access_token_fetcher_adapter.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "google_apis/gaia/token_binding_response_encryption_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Ge;
using testing::IsEmpty;
using testing::Le;
using testing::Matcher;
using testing::Property;

constexpr char kTestClientId[] = "test_client_id";
constexpr char kTestClientSecret[] = "test_client_secret";
constexpr char kTestScope[] = "test_scope";
constexpr char kTestRefreshToken[] = "test_refresh_token";
constexpr GaiaId::Literal kTestUserGaiaId("test_gaia_id");
constexpr char kTestAccessToken[] = "test_access_token";
constexpr char kTestDeviceId[] = "test_device_id";
constexpr char kTestVersion[] = "test_version";
constexpr char kTestChannel[] = "test_channel";
constexpr char kTestAssertion[] = "test_assertion";

constexpr char kAssertionSentinel[] = "DBSC_CHALLENGE_IF_REQUIRED";

constexpr char kFetchAuthErrorHistogram[] =
    "Signin.OAuth2MintToken.BoundFetchAuthError";
constexpr std::string_view kFetchAuthErrorChallengeSentinelHistogram =
    "Signin.OAuth2MintToken.BoundFetchAuthError.ChallengeSentinel";
constexpr std::string_view kFetchAuthErrorAssertionFailedHistogram =
    "Signin.OAuth2MintToken.BoundFetchAuthError.AssertionFailed";
constexpr std::string_view kFetchAuthErrorSignedAssertionHistogram =
    "Signin.OAuth2MintToken.BoundFetchAuthError.SignedAssertion";
constexpr char kFetchEncryptionErrorHistogram[] =
    "Signin.OAuth2MintToken.BoundFetchEncryptionError";

class MockOAuth2AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuth2AccessTokenConsumer() = default;
  ~MockOAuth2AccessTokenConsumer() override = default;

  MOCK_METHOD(void,
              OnGetTokenSuccess,
              (const OAuth2AccessTokenConsumer::TokenResponse&),
              (override));
  MOCK_METHOD(void,
              OnGetTokenFailure,
              (const GoogleServiceAuthError& error),
              (override));

  std::string GetConsumerName() const override {
    return "oauth2_mint_access_token_fetcher_adapter_unittest";
  }
};

class MockOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  MockOAuth2MintTokenFlow(Delegate* delegate, Parameters params)
      : OAuth2MintTokenFlow(delegate, params.Clone()),
        delegate_(delegate),
        params_(std::move(params)) {}

  MOCK_METHOD(
      void,
      Start,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       const std::string& access_token),
      (override));

  void SimulateMintTokenSuccess(const std::string& access_token,
                                const std::set<std::string>& granted_scopes,
                                int time_to_live,
                                bool is_encrypted) {
    MintTokenResult result;
    result.access_token = access_token;
    result.granted_scopes = granted_scopes;
    result.time_to_live = base::Seconds(time_to_live);
    result.is_token_encrypted = is_encrypted;
    delegate_->OnMintTokenSuccess(result);
  }
  void SimulateMintTokenFailure(const GoogleServiceAuthError& error) {
    delegate_->OnMintTokenFailure(error);
  }
  void SimulateRemoteConsentSuccess(
      const RemoteConsentResolutionData& resolution_data) {
    delegate_->OnRemoteConsentSuccess(resolution_data);
  }

  const Parameters& params() { return params_; }

  base::WeakPtr<MockOAuth2MintTokenFlow> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<Delegate> delegate_ = nullptr;
  const Parameters params_;
  base::WeakPtrFactory<MockOAuth2MintTokenFlow> weak_ptr_factory_{this};
};

Matcher<const OAuth2AccessTokenConsumer::TokenResponse&> HasAccessTokenWithTtl(
    const std::string& access_token,
    base::TimeDelta time_to_live) {
  using TokenResponse = OAuth2AccessTokenConsumer::TokenResponse;
  return AllOf(
      Field("access_token", &TokenResponse::access_token, access_token),
      Field("refresh_token", &TokenResponse::refresh_token, IsEmpty()),
      Field("id_token", &TokenResponse::id_token, IsEmpty()),
      Field(
          "expiration_time", &TokenResponse::expiration_time,
          AllOf(Ge(base::Time::Now()), Le(base::Time::Now() + time_to_live))));
}

Matcher<const OAuth2MintTokenFlow::Parameters&> ParamsEq(
    const OAuth2MintTokenFlow::Parameters& expected) {
  using Parameters = OAuth2MintTokenFlow::Parameters;
  return AllOf(
      Field("extension_id", &Parameters::extension_id, expected.extension_id),
      Field("client_id", &Parameters::client_id, expected.client_id),
      Field("scopes", &Parameters::scopes, expected.scopes),
      Field("enable_granular_permissions",
            &Parameters::enable_granular_permissions,
            expected.enable_granular_permissions),
      Field("device_id", &Parameters::device_id, expected.device_id),
      Field("selected_user_id", &Parameters::selected_user_id,
            expected.selected_user_id),
      Field("consent_result", &Parameters::consent_result,
            expected.consent_result),
      Field("version", &Parameters::version, expected.version),
      Field("channel", &Parameters::channel, expected.channel),
      Field("mode", &Parameters::mode, expected.mode),
      Field("bound_oauth_token", &Parameters::bound_oauth_token,
            expected.bound_oauth_token));
}

}  // namespace

class OAuth2MintAccessTokenFetcherAdapterTest : public testing::Test {
 public:
  OAuth2MintAccessTokenFetcherAdapterTest() = default;
  ~OAuth2MintAccessTokenFetcherAdapterTest() override = default;

  std::unique_ptr<OAuth2MintAccessTokenFetcherAdapter> CreateFetcher() {
    auto fetcher = std::make_unique<OAuth2MintAccessTokenFetcherAdapter>(
        &mock_consumer_, url_loader_factory_.GetSafeWeakWrapper(),
        kTestUserGaiaId, kTestRefreshToken, kTestDeviceId, kTestVersion,
        kTestChannel);
    fetcher->SetOAuth2MintTokenFlowFactoryForTesting(base::BindRepeating(
        &OAuth2MintAccessTokenFetcherAdapterTest::CreateMockFlow,
        base::Unretained(this)));
    return fetcher;
  }

  std::unique_ptr<OAuth2MintTokenFlow> CreateMockFlow(
      OAuth2MintTokenFlow::Delegate* delegate,
      OAuth2MintTokenFlow::Parameters params) {
    CHECK(!mock_flow_);
    auto mock_flow =
        std::make_unique<MockOAuth2MintTokenFlow>(delegate, std::move(params));
    mock_flow_ = mock_flow->GetWeakPtr();
    EXPECT_CALL(*mock_flow_, Start(_, kTestRefreshToken));
    return mock_flow;
  }

  void VerifyFetchAuthErrorHistograms(GoogleServiceAuthError::State error,
                                      std::string_view suffixed_histogram) {
    histogram_tester().ExpectUniqueSample(kFetchAuthErrorHistogram, error,
                                          /*expected_bucket_count=*/1);
    histogram_tester().ExpectUniqueSample(suffixed_histogram, error,
                                          /*expected_bucket_count=*/1);
  }

  MockOAuth2AccessTokenConsumer* mock_consumer() { return &mock_consumer_; }

  base::WeakPtr<MockOAuth2MintTokenFlow> mock_flow() { return mock_flow_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory url_loader_factory_;
  MockOAuth2AccessTokenConsumer mock_consumer_;
  base::WeakPtr<MockOAuth2MintTokenFlow> mock_flow_ = nullptr;
  base::HistogramTester histogram_tester_;
};

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, Params) {
  auto fetcher = CreateFetcher();
  // Need to start a fetcher to create a mock flow.
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  EXPECT_TRUE(mock_flow());
  OAuth2MintTokenFlow::Parameters expected_params;
  expected_params.client_id = kTestClientId;
  expected_params.version = kTestVersion;
  expected_params.channel = kTestChannel;
  expected_params.device_id = kTestDeviceId;
  expected_params.enable_granular_permissions = false;
  expected_params.mode = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
  expected_params.scopes = {kTestScope};
  expected_params.bound_oauth_token = gaia::CreateBoundOAuthToken(
      kTestUserGaiaId, kTestRefreshToken, kAssertionSentinel);
  EXPECT_THAT(mock_flow()->params(), ParamsEq(expected_params));
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, ParamsWithBindingKeyAssertion) {
  auto fetcher = CreateFetcher();
  fetcher->SetBindingKeyAssertion(kTestAssertion);
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  EXPECT_TRUE(mock_flow());
  OAuth2MintTokenFlow::Parameters expected_params;
  expected_params.client_id = kTestClientId;
  expected_params.version = kTestVersion;
  expected_params.channel = kTestChannel;
  expected_params.device_id = kTestDeviceId;
  expected_params.enable_granular_permissions = false;
  expected_params.mode = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
  expected_params.scopes = {kTestScope};
  expected_params.bound_oauth_token = gaia::CreateBoundOAuthToken(
      kTestUserGaiaId, kTestRefreshToken, kTestAssertion);
  EXPECT_THAT(mock_flow()->params(), ParamsEq(expected_params));
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, Success) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(*mock_consumer(), OnGetTokenSuccess(HasAccessTokenWithTtl(
                                    kTestAccessToken, kTimeToLive)));
  mock_flow()->SimulateMintTokenSuccess(kTestAccessToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/false);
  VerifyFetchAuthErrorHistograms(GoogleServiceAuthError::NONE,
                                 kFetchAuthErrorChallengeSentinelHistogram);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, SuccessWithSignedAssertion) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  fetcher->SetBindingKeyAssertion("test_assertion");
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(*mock_consumer(), OnGetTokenSuccess(HasAccessTokenWithTtl(
                                    kTestAccessToken, kTimeToLive)));
  mock_flow()->SimulateMintTokenSuccess(kTestAccessToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/false);
  VerifyFetchAuthErrorHistograms(GoogleServiceAuthError::NONE,
                                 kFetchAuthErrorSignedAssertionHistogram);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, SuccessWithFailedAssertion) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  fetcher->SetBindingKeyAssertion(
      std::string(GaiaConstants::kTokenBindingAssertionFailedPlaceholder));
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(*mock_consumer(), OnGetTokenSuccess(HasAccessTokenWithTtl(
                                    kTestAccessToken, kTimeToLive)));
  mock_flow()->SimulateMintTokenSuccess(kTestAccessToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/false);
  VerifyFetchAuthErrorHistograms(GoogleServiceAuthError::NONE,
                                 kFetchAuthErrorAssertionFailedHistogram);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, SuccessWithEncryption) {
  const std::string kTestEncryptedToken = "test_encrypted_token";
  auto fetcher = CreateFetcher();
  base::MockCallback<OAuth2MintAccessTokenFetcherAdapter::TokenDecryptor>
      mock_decryptor;
  fetcher->SetTokenDecryptor(mock_decryptor.Get());
  EXPECT_CALL(mock_decryptor, Run(kTestEncryptedToken))
      .WillOnce(testing::Return(kTestAccessToken));
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(*mock_consumer(), OnGetTokenSuccess(HasAccessTokenWithTtl(
                                    kTestAccessToken, kTimeToLive)));
  mock_flow()->SimulateMintTokenSuccess(kTestEncryptedToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/true);
  VerifyFetchAuthErrorHistograms(GoogleServiceAuthError::NONE,
                                 kFetchAuthErrorChallengeSentinelHistogram);
  histogram_tester().ExpectUniqueSample(
      kFetchEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kSuccessfullyDecrypted,
      /*expected_bucket_count=*/1);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, SuccessDecryptorUnused) {
  auto fetcher = CreateFetcher();
  base::MockCallback<OAuth2MintAccessTokenFetcherAdapter::TokenDecryptor>
      mock_decryptor;
  fetcher->SetTokenDecryptor(mock_decryptor.Get());
  EXPECT_CALL(mock_decryptor, Run).Times(0);
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(*mock_consumer(), OnGetTokenSuccess(HasAccessTokenWithTtl(
                                    kTestAccessToken, kTimeToLive)));
  mock_flow()->SimulateMintTokenSuccess(kTestAccessToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/false);
  VerifyFetchAuthErrorHistograms(GoogleServiceAuthError::NONE,
                                 kFetchAuthErrorChallengeSentinelHistogram);
  histogram_tester().ExpectUniqueSample(
      kFetchEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kSuccessNoEncryption,
      /*expected_bucket_count=*/1);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, Failure) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  EXPECT_CALL(*mock_consumer(), OnGetTokenFailure(error));
  mock_flow()->SimulateMintTokenFailure(error);
  VerifyFetchAuthErrorHistograms(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
      kFetchAuthErrorChallengeSentinelHistogram);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, ChallengeRequired) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromTokenBindingChallenge("challenge");
  EXPECT_CALL(*mock_consumer(), OnGetTokenFailure(error));
  mock_flow()->SimulateMintTokenFailure(error);
  VerifyFetchAuthErrorHistograms(
      GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED,
      kFetchAuthErrorChallengeSentinelHistogram);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, DecryptionFailure) {
  const std::string kTestEncryptedToken = "test_encrypted_token";
  auto fetcher = CreateFetcher();
  base::MockCallback<OAuth2MintAccessTokenFetcherAdapter::TokenDecryptor>
      mock_decryptor;
  fetcher->SetTokenDecryptor(mock_decryptor.Get());
  EXPECT_CALL(mock_decryptor, Run(kTestEncryptedToken))
      .WillOnce(testing::Return(std::string()));
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(
      *mock_consumer(),
      OnGetTokenFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Failed to decrypt token")));
  mock_flow()->SimulateMintTokenSuccess(kTestEncryptedToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/true);
  VerifyFetchAuthErrorHistograms(
      GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
      kFetchAuthErrorChallengeSentinelHistogram);
  histogram_tester().ExpectUniqueSample(
      kFetchEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kDecryptionFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, NoDecryptorFailure) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(
      *mock_consumer(),
      OnGetTokenFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Unexpectedly received an encrypted token")));
  mock_flow()->SimulateMintTokenSuccess(kTestAccessToken, {kTestScope},
                                        kTimeToLive.InSeconds(),
                                        /*is_encrypted=*/true);
  VerifyFetchAuthErrorHistograms(
      GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
      kFetchAuthErrorChallengeSentinelHistogram);
  histogram_tester().ExpectUniqueSample(
      kFetchEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kResponseUnexpectedlyEncrypted,
      /*expected_bucket_count=*/1);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, UnexpectedConsentResult) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  EXPECT_CALL(*mock_consumer(),
              OnGetTokenFailure(Property(
                  "state", &GoogleServiceAuthError::state,
                  GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR)));
  mock_flow()->SimulateRemoteConsentSuccess(RemoteConsentResolutionData());
  VerifyFetchAuthErrorHistograms(
      GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR,
      kFetchAuthErrorChallengeSentinelHistogram);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, CancelRequest) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  fetcher->CancelRequest();
  EXPECT_FALSE(mock_flow());
  EXPECT_THAT(
      histogram_tester().GetTotalCountsForPrefix(kFetchAuthErrorHistogram),
      testing::IsEmpty());
}
