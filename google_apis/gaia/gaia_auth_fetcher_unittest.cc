// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaAuthFetcher.
// Originally ported from GoogleAuthenticator tests.
#include "google_apis/gaia/gaia_auth_fetcher.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Invoke;
using ::testing::_;

namespace {

constexpr char kVersionListHeader[] = "Sec-CH-UA-Full-Version-List";

const char kGetTokenPairValidResponse[] =
    R"({
        "refresh_token": "rt1",
        "access_token": "at1",
        "expires_in": 3600,
        "token_type": "Bearer",
        "id_token": "it1"
     })";

const char kGetTokenPairValidBoundKeyResponse[] =
    R"({
        "refresh_token": "rt1",
        "access_token": "at1",
        "expires_in": 3600,
        "token_type": "Bearer",
        "id_token": "it1",
        "refresh_token_type": "BOUND_TO_KEY"
     })";

std::string GetRequestBodyAsString(const network::ResourceRequest* request) {
  if (!request->request_body || !request->request_body->elements() ||
      !request->request_body->elements()->size()) {
    return "";
  }
  const network::DataElement& elem = request->request_body->elements()->at(0);
  if (elem.type() != network::DataElement::Tag::kBytes) {
    return "";
  }
  return std::string(elem.As<network::DataElementBytes>().AsStringPiece());
}

}  // namespace

class GaiaAuthFetcherTest : public testing::Test {
 protected:
  GaiaAuthFetcherTest()
      : oauth2_token_source_(GaiaUrls::GetInstance()->oauth2_token_url()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    test_url_loader_factory_.SetInterceptor(base::BindRepeating(
        &GaiaAuthFetcherTest::OnResourceIntercepted, base::Unretained(this)));
  }

  void RunErrorParsingTest(const std::string& data,
                           const std::string& error,
                           const std::string& error_url) {
    std::string out_error;
    std::string out_error_url;

    GaiaAuthFetcher::ParseFailureResponse(data, &out_error, &out_error_url);
    EXPECT_EQ(error, out_error);
    EXPECT_EQ(error_url, out_error_url);
  }

  GURL oauth2_token_source_;

 protected:
  void OnResourceIntercepted(const network::ResourceRequest& resource) {
    received_requests_.push_back(resource);
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
    return test_shared_loader_factory_;
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::vector<network::ResourceRequest> received_requests_;
};

class MockGaiaConsumer : public GaiaAuthConsumer {
 public:
  MockGaiaConsumer() {}
  ~MockGaiaConsumer() override {}

  MOCK_METHOD1(OnClientOAuthCode, void(const std::string& data));
  MOCK_METHOD1(OnClientOAuthSuccess,
               void(const GaiaAuthConsumer::ClientOAuthResult& result));
  MOCK_METHOD1(OnOAuthMultiloginFinished,
               void(const OAuthMultiloginResult& result));
  MOCK_METHOD1(OnClientOAuthFailure,
      void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnOAuth2RevokeTokenCompleted,
               void(GaiaAuthConsumer::TokenRevocationStatus status));
  MOCK_METHOD1(OnListAccountsSuccess, void(const std::string& data));
  MOCK_METHOD0(OnLogOutSuccess, void());
  MOCK_METHOD1(OnLogOutFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnGetCheckConnectionInfoSuccess, void(const std::string& data));
  MOCK_METHOD1(OnReAuthProofTokenSuccess, void(const std::string& rapt_token));
  MOCK_METHOD1(OnReAuthProofTokenFailure,
               void(GaiaAuthConsumer::ReAuthProofTokenStatus status));
};

#if BUILDFLAG(IS_WIN)
#define MAYBE_ErrorComparator DISABLED_ErrorComparator
#else
#define MAYBE_ErrorComparator ErrorComparator
#endif

TEST_F(GaiaAuthFetcherTest, MAYBE_ErrorComparator) {
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(-101);

  GoogleServiceAuthError matching_error =
      GoogleServiceAuthError::FromConnectionError(-101);

  EXPECT_TRUE(expected_error == matching_error);

  expected_error = GoogleServiceAuthError::FromConnectionError(6);

  EXPECT_FALSE(expected_error == matching_error);

  expected_error = GoogleServiceAuthError(GoogleServiceAuthError::NONE);

  EXPECT_FALSE(expected_error == matching_error);

  matching_error = GoogleServiceAuthError(GoogleServiceAuthError::NONE);

  EXPECT_TRUE(expected_error == matching_error);
}

// A version of GaiaAuthFetcher that exposes some of the protected methods.
class TestGaiaAuthFetcher : public GaiaAuthFetcher {
 public:
  TestGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : GaiaAuthFetcher(consumer,
                        gaia::GaiaSource::kChrome,
                        url_loader_factory) {}

  void CreateAndStartGaiaFetcherForTesting(
      const std::string& body,
      const std::string& headers,
      const GURL& gaia_gurl,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) {
    CreateAndStartGaiaFetcher(body, /*content_type=*/"", headers, gaia_gurl,
                              credentials_mode, traffic_annotation);
  }

  void TestOnURLLoadCompleteInternal(
      net::Error net_error,
      int response_code = net::HTTP_OK,
      std::string response_body = "") {
    OnURLLoadCompleteInternal(net_error, response_code, response_body);
  }
};

TEST_F(GaiaAuthFetcherTest, ParseErrorRequest) {
  RunErrorParsingTest(
      "Url=U\n"
      "Error=E\n",
      "E", "U");
  RunErrorParsingTest(
      "Error=E\n"
      "Url=U\n",
      "E", "U");
  RunErrorParsingTest(
      "\n\n\nError=E\n"
      "\nUrl=U\n",
      "E", "U");
}

TEST_F(GaiaAuthFetcherTest, BadAuthenticationError) {
  std::string data = "Error=BadAuthentication\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
}

TEST_F(GaiaAuthFetcherTest, BadAuthenticationShortError) {
  std::string data = "Error=badauth\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
}

TEST_F(GaiaAuthFetcherTest, IncomprehensibleError) {
  std::string data = "Error=Gobbledygook\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

TEST_F(GaiaAuthFetcherTest, ServiceUnavailableError) {
  std::string data = "Error=ServiceUnavailable\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

TEST_F(GaiaAuthFetcherTest, ServiceUnavailableShortError) {
  std::string data = "Error=ire\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

TEST_F(GaiaAuthFetcherTest, StartAuthCodeForOAuth2TokenExchangeSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientOAuthCode("test-code")).Times(0);
  EXPECT_CALL(
      consumer,
      OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
          "rt1", "at1", 3600, /*is_child_account=*/false,
          /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false)))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchange("auth_code");
  ASSERT_EQ(received_requests_.size(), 1U);
  EXPECT_EQ(google_apis::GetOmitCredentialsModeForGaiaRequests(),
            received_requests_.at(0).credentials_mode);
  std::string body = GetRequestBodyAsString(&received_requests_.at(0));
  EXPECT_FALSE(base::Contains(body, "device_type=chrome"));
  EXPECT_TRUE(auth.HasPendingFetch());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK,
                                     kGetTokenPairValidResponse);
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, StartAuthCodeForOAuth2TokenExchangeDeviceId) {
  MockGaiaConsumer consumer;
  GaiaAuthFetcher auth(&consumer, gaia::GaiaSource::kChrome,
                       GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchangeWithDeviceId("auth_code",
                                                       "device_ABCDE_1");

  ASSERT_EQ(1U, received_requests_.size());
  EXPECT_EQ(google_apis::GetOmitCredentialsModeForGaiaRequests(),
            received_requests_.at(0).credentials_mode);
  std::string body = GetRequestBodyAsString(&received_requests_.at(0));
  EXPECT_TRUE(base::Contains(body, "device_type=chrome"));
  EXPECT_TRUE(base::Contains(body, "device_id=device_ABCDE_1"));
}

TEST_F(GaiaAuthFetcherTest,
       StartAuthCodeForOAuth2TokenExchangeBindingRegistrationToken) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(
      consumer,
      OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
          "rt1", "at1", 3600, /*is_child_account=*/false,
          /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/true)));

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchange("auth_code", "version_list",
                                           "registration_jwt");
  ASSERT_EQ(received_requests_.size(), 1U);
  EXPECT_EQ(google_apis::GetOmitCredentialsModeForGaiaRequests(),
            received_requests_.at(0).credentials_mode);
  std::string body = GetRequestBodyAsString(&received_requests_.at(0));
  EXPECT_TRUE(
      base::Contains(body, "bound_token_registration_jwt=registration_jwt"));
  EXPECT_THAT(received_requests_.at(0).headers.GetHeader(kVersionListHeader),
              testing::Optional(std::string("version_list")));
  EXPECT_TRUE(auth.HasPendingFetch());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK,
                                     kGetTokenPairValidBoundKeyResponse);
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(
    GaiaAuthFetcherTest,
    StartAuthCodeForOAuth2TokenExchangeBindingRegistrationTokenNoVersionHeader) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(
      consumer,
      OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
          "rt1", "at1", 3600, /*is_child_account=*/false,
          /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/true)));

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchange("auth_code", "", "registration_jwt");
  ASSERT_EQ(received_requests_.size(), 1U);
  EXPECT_EQ(google_apis::GetOmitCredentialsModeForGaiaRequests(),
            received_requests_.at(0).credentials_mode);
  std::string body = GetRequestBodyAsString(&received_requests_.at(0));
  EXPECT_TRUE(
      base::Contains(body, "bound_token_registration_jwt=registration_jwt"));
  EXPECT_FALSE(received_requests_.at(0).headers.HasHeader(kVersionListHeader));
  EXPECT_TRUE(auth.HasPendingFetch());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK,
                                     kGetTokenPairValidBoundKeyResponse);
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, StartAuthCodeForOAuth2TokenExchangeFailure) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientOAuthFailure(_)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchange("auth_code");
  EXPECT_TRUE(auth.HasPendingFetch());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_FORBIDDEN);
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, MultiloginRequestFormat) {
  MockGaiaConsumer consumer;
  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  std::vector<GaiaAuthFetcher::MultiloginTokenIDPair> accounts;
  accounts.push_back({"token1", "id1"});
  accounts.push_back({"token2", "id2"});
  auth.StartOAuthMultilogin(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER, accounts,
      "cc_result");
  ASSERT_TRUE(auth.HasPendingFetch());

  const network::ResourceRequest& request0 = received_requests_.at(0);
  EXPECT_EQ("POST", request0.method);
  EXPECT_THAT(
      request0.headers.GetHeader("Authorization"),
      testing::Optional(std::string("MultiBearer id1:token1,id2:token2")));
  EXPECT_EQ("source=ChromiumBrowser&reuseCookies=0&externalCcResult=cc_result",
            request0.url.query());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, std::string());
  EXPECT_FALSE(auth.HasPendingFetch());

  auth.StartOAuthMultilogin(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER, accounts,
      "cc_result");
  ASSERT_TRUE(auth.HasPendingFetch());

  const network::ResourceRequest& request1 = received_requests_.at(1);
  EXPECT_EQ("source=ChromiumBrowser&reuseCookies=1&externalCcResult=cc_result",
            request1.url.query());
}

TEST_F(GaiaAuthFetcherTest, MultiloginSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnOAuthMultiloginFinished(::testing::Property(
                            &OAuthMultiloginResult::status,
                            ::testing::Eq(OAuthMultiloginResponseStatus::kOk))))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartOAuthMultilogin(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>(), std::string());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK,
                                     R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )");

  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, MultiloginFailureNetError) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuthMultiloginFinished(::testing::Property(
                  &OAuthMultiloginResult::status,
                  ::testing::Eq(OAuthMultiloginResponseStatus::kRetry))))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartOAuthMultilogin(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>(), std::string());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::ERR_ABORTED, net::HTTP_OK,
                                     R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )");

  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, MultiloginFailureServerError) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuthMultiloginFinished(::testing::Property(
                  &OAuthMultiloginResult::status,
                  ::testing::Eq(OAuthMultiloginResponseStatus::kError))))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.StartOAuthMultilogin(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>(), std::string());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK,
                                     "\n{\"status\": \"ERROR\"}");

  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, ListAccounts) {
  std::string data(
      R"(["gaia.l.a.r",
           [
             ["gaia.l.a", 1, "First Last", "user@gmail.com",
              "//googleusercontent.com/A/B/C/D/photo.jpg", 1, 1, 0
              ]
           ]
         ])");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnListAccountsSuccess(data)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(received_requests_.size(), 1U);
  EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
            received_requests_.at(0).credentials_mode);
  EXPECT_TRUE(received_requests_.at(0).site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GaiaUrls::GetInstance()->gaia_url())));
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, data);
}

TEST_F(GaiaAuthFetcherTest, LogOutSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnLogOutSuccess()).Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->LogOutURLWithSource(
          GaiaConstants::kChromeSource),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK);
}

TEST_F(GaiaAuthFetcherTest, LogOutFailure) {
  net::Error error_no = net::ERR_CONNECTION_RESET;

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(error_no);
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnLogOutFailure(expected_error)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->LogOutURLWithSource(
          GaiaConstants::kChromeSource),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(error_no);
}

TEST_F(GaiaAuthFetcherTest, GetCheckConnectionInfo) {
  std::string data(R"(
      [{"carryBackToken": "token1", "url": "http://www.google.com"}])");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnGetCheckConnectionInfoSuccess(data)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());

  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          GaiaConstants::kChromeSource),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenSuccess) {
  std::string data("{}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnOAuth2RevokeTokenCompleted(
                            GaiaAuthConsumer::TokenRevocationStatus::kSuccess))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenCanceled) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kConnectionCanceled))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::ERR_ABORTED);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenFailed) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kConnectionFailed))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::ERR_CERT_CONTAINS_ERRORS);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenTimeout) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kConnectionTimeout))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::ERR_TIMED_OUT);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenInvalidToken) {
  std::string data("{\"error\" : \"invalid_token\"}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kInvalidToken))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_BAD_REQUEST, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenInvalidRequest) {
  std::string data("{\"error\" : \"invalid_request\"}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kInvalidRequest))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_BAD_REQUEST, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenServerError) {
  std::string data("{}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kServerError))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      network::mojom::CredentialsMode::kInclude, TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                                     data);
}

TEST_F(GaiaAuthFetcherTest, ReAuthTokenSuccess) {
  std::string data("{\"encodedRapt\" : \"rapt_token\"}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnReAuthProofTokenSuccess("rapt_token")).Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->reauth_api_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, data);
}

TEST_F(GaiaAuthFetcherTest, ReAuthTokenInvalidRequest) {
  std::string data("{\"error\" : { \"message\" :  \"INVALID_REQUEST\"} }");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnReAuthProofTokenFailure(
                  GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidRequest))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->reauth_api_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_BAD_REQUEST, data);
}

TEST_F(GaiaAuthFetcherTest, ReAuthTokenInvalidGrant) {
  std::string data("{\"error\" : { \"message\" :  \"INVALID_GRANT\"} }");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnReAuthProofTokenFailure(
                  GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->reauth_api_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_BAD_REQUEST, data);
}

TEST_F(GaiaAuthFetcherTest, ReAuthTokenUnauthorizedClient) {
  std::string data("{\"error\" : { \"message\" :  \"UNAUTHORIZED_CLIENT\"} }");
  MockGaiaConsumer consumer;
  EXPECT_CALL(
      consumer,
      OnReAuthProofTokenFailure(
          GaiaAuthConsumer::ReAuthProofTokenStatus::kUnauthorizedClient))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->reauth_api_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_FORBIDDEN, data);
}

TEST_F(GaiaAuthFetcherTest, ReAuthTokenInsufficientScope) {
  std::string data("{\"error\" : { \"message\" :  \"INSUFFICIENT_SCOPE\"} }");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnReAuthProofTokenFailure(
                  GaiaAuthConsumer::ReAuthProofTokenStatus::kInsufficientScope))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->reauth_api_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_FORBIDDEN, data);
}

TEST_F(GaiaAuthFetcherTest, ReAuthTokenCredentialNotSet) {
  std::string data("{\"error\" : { \"message\" :  \"CREDENTIAL_NOT_SET\"} }");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnReAuthProofTokenFailure(
                  GaiaAuthConsumer::ReAuthProofTokenStatus::kCredentialNotSet))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->reauth_api_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_FORBIDDEN, data);
}
