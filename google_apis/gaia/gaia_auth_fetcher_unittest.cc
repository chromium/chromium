// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaAuthFetcher.
// Originally ported from GoogleAuthenticator tests.

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Invoke;
using ::testing::_;

namespace {

const char kGetTokenPairValidResponse[] =
    R"({
        "refresh_token": "rt1",
        "access_token": "at1",
        "expires_in": 3600,
        "token_type": "Bearer",
        "id_token": "it1"
     })";

std::string GetRequestBodyAsString(const network::ResourceRequest* request) {
  if (!request->request_body || !request->request_body->elements() ||
      !request->request_body->elements()->size()) {
    return "";
  }
  const network::DataElement& elem = request->request_body->elements()->at(0);
  return std::string(elem.bytes(), elem.length());
}

}  // namespace

class GaiaAuthFetcherTest : public testing::Test {
 protected:
  GaiaAuthFetcherTest()
      : oauth2_token_source_(GaiaUrls::GetInstance()->oauth2_token_url()),
        token_auth_source_(GaiaUrls::GetInstance()->token_auth_url()),
        merge_session_source_(GaiaUrls::GetInstance()->merge_session_url()),
        uberauth_token_source_(
            GaiaUrls::GetInstance()->oauth1_login_url().Resolve(
                "?source=&issueuberauth=1")),
        oauth_login_gurl_(GaiaUrls::GetInstance()->oauth1_login_url()),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    test_url_loader_factory_.SetInterceptor(base::BindRepeating(
        &GaiaAuthFetcherTest::OnResourceIntercepted, base::Unretained(this)));
  }

  void RunParsingTest(const std::string& data,
                      const std::string& sid,
                      const std::string& lsid,
                      const std::string& token) {
    std::string out_sid;
    std::string out_lsid;
    std::string out_token;

    GaiaAuthFetcher::ParseClientLoginResponse(data,
                                              &out_sid,
                                              &out_lsid,
                                              &out_token);
    EXPECT_EQ(lsid, out_lsid);
    EXPECT_EQ(sid, out_sid);
    EXPECT_EQ(token, out_token);
  }

  void RunErrorParsingTest(const std::string& data,
                           const std::string& error,
                           const std::string& error_url,
                           const std::string& captcha_url,
                           const std::string& captcha_token) {
    std::string out_error;
    std::string out_error_url;
    std::string out_captcha_url;
    std::string out_captcha_token;

    GaiaAuthFetcher::ParseClientLoginFailure(data,
                                             &out_error,
                                             &out_error_url,
                                             &out_captcha_url,
                                             &out_captcha_token);
    EXPECT_EQ(error, out_error);
    EXPECT_EQ(error_url, out_error_url);
    EXPECT_EQ(captcha_url, out_captcha_url);
    EXPECT_EQ(captcha_token, out_captcha_token);
  }

  GURL oauth2_token_source_;
  GURL token_auth_source_;
  GURL merge_session_source_;
  GURL uberauth_token_source_;
  GURL oauth_login_gurl_;

 protected:
  void OnResourceIntercepted(const network::ResourceRequest& resource) {
    received_requests_.push_back(resource);
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
    return test_shared_loader_factory_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::vector<network::ResourceRequest> received_requests_;
};

class MockGaiaConsumer : public GaiaAuthConsumer {
 public:
  MockGaiaConsumer() {}
  ~MockGaiaConsumer() override {}

  MOCK_METHOD1(OnClientLoginSuccess, void(const ClientLoginResult& result));
  MOCK_METHOD1(OnClientOAuthCode, void(const std::string& data));
  MOCK_METHOD1(OnClientOAuthSuccess,
               void(const GaiaAuthConsumer::ClientOAuthResult& result));
  MOCK_METHOD1(OnMergeSessionSuccess, void(const std::string& data));
  MOCK_METHOD1(OnOAuthMultiloginFinished,
               void(const OAuthMultiloginResult& result));
  MOCK_METHOD1(OnUberAuthTokenSuccess, void(const std::string& data));
  MOCK_METHOD1(OnClientLoginFailure,
      void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnClientOAuthFailure,
      void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnOAuth2RevokeTokenCompleted,
               void(GaiaAuthConsumer::TokenRevocationStatus status));
  MOCK_METHOD1(OnMergeSessionFailure, void(
      const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnUberAuthTokenFailure, void(
      const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnListAccountsSuccess, void(const std::string& data));
  MOCK_METHOD0(OnLogOutSuccess, void());
  MOCK_METHOD1(OnLogOutFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnGetCheckConnectionInfoSuccess, void(const std::string& data));
};

#if defined(OS_WIN)
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
      const std::string& source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : GaiaAuthFetcher(consumer, source, url_loader_factory) {}

  void CreateAndStartGaiaFetcherForTesting(
      const std::string& body,
      const std::string& headers,
      const GURL& gaia_gurl,
      int load_flags,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) {
    CreateAndStartGaiaFetcher(body, headers, gaia_gurl, load_flags,
                              traffic_annotation);
  }

  void TestOnURLLoadCompleteInternal(
      net::Error net_error,
      int response_code = net::HTTP_OK,
      const std::vector<std::string>& cookies = {},
      std::string response_body = "") {
    net::HttpRawRequestHeaders::HeaderVector headers;
    for (auto& cookie : cookies) {
      headers.push_back(std::make_pair("Set-Cookie", cookie));
    }
    OnURLLoadCompleteInternal(net_error, response_code, headers, response_body);
  }
};

TEST_F(GaiaAuthFetcherTest, ParseRequest) {
  RunParsingTest("SID=sid\nLSID=lsid\nAuth=auth\n", "sid", "lsid", "auth");
  RunParsingTest("LSID=lsid\nSID=sid\nAuth=auth\n", "sid", "lsid", "auth");
  RunParsingTest("SID=sid\nLSID=lsid\nAuth=auth", "sid", "lsid", "auth");
  RunParsingTest("SID=sid\nAuth=auth\n", "sid", std::string(), "auth");
  RunParsingTest("LSID=lsid\nAuth=auth\n", std::string(), "lsid", "auth");
  RunParsingTest("\nAuth=auth\n", std::string(), std::string(), "auth");
  RunParsingTest("SID=sid", "sid", std::string(), std::string());
}

TEST_F(GaiaAuthFetcherTest, ParseErrorRequest) {
  RunErrorParsingTest("Url=U\n"
                      "Error=E\n"
                      "CaptchaToken=T\n"
                      "CaptchaUrl=C\n", "E", "U", "C", "T");
  RunErrorParsingTest("CaptchaToken=T\n"
                      "Error=E\n"
                      "Url=U\n"
                      "CaptchaUrl=C\n", "E", "U", "C", "T");
  RunErrorParsingTest("\n\n\nCaptchaToken=T\n"
                      "\nError=E\n"
                      "\nUrl=U\n"
                      "CaptchaUrl=C\n", "E", "U", "C", "T");
}

TEST_F(GaiaAuthFetcherTest, CheckTwoFactorResponse) {
  std::string response =
      base::StringPrintf("Error=BadAuthentication\n%s\n",
                         GaiaAuthFetcher::kSecondFactor);
  EXPECT_TRUE(GaiaAuthFetcher::IsSecondFactorSuccess(response));
}

TEST_F(GaiaAuthFetcherTest, CheckNormalErrorCode) {
  std::string response = "Error=BadAuthentication\n";
  EXPECT_FALSE(GaiaAuthFetcher::IsSecondFactorSuccess(response));
}

TEST_F(GaiaAuthFetcherTest, CaptchaParse) {
  std::string data = "Url=http://www.google.com/login/captcha\n"
                     "Error=CaptchaRequired\n"
                     "CaptchaToken=CCTOKEN\n"
                     "CaptchaUrl=Captcha?ctoken=CCTOKEN\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);

  std::string token = "CCTOKEN";
  GURL image_url("http://accounts.google.com/Captcha?ctoken=CCTOKEN");
  GURL unlock_url("http://www.google.com/login/captcha");

  EXPECT_EQ(error.state(), GoogleServiceAuthError::CAPTCHA_REQUIRED);
  EXPECT_EQ(error.captcha().token, token);
  EXPECT_EQ(error.captcha().image_url, image_url);
  EXPECT_EQ(error.captcha().unlock_url, unlock_url);
}

TEST_F(GaiaAuthFetcherTest, AccountDeletedError) {
  std::string data = "Error=AccountDeleted\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::ACCOUNT_DELETED);
}

TEST_F(GaiaAuthFetcherTest, AccountDisabledError) {
  std::string data = "Error=AccountDisabled\n";
  GoogleServiceAuthError error =
      GaiaAuthFetcher::GenerateAuthError(data, net::OK);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::ACCOUNT_DISABLED);
}

TEST_F(GaiaAuthFetcherTest, BadAuthenticationError) {
  std::string data = "Error=BadAuthentication\n";
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

TEST_F(GaiaAuthFetcherTest, StartAuthCodeForOAuth2TokenExchange_Success) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientOAuthCode("test-code")).Times(0);
  EXPECT_CALL(consumer,
              OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
                  "rt1", "at1", 3600, false /* is_child_account */,
                  false /* is_advanced_protection */)))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchange("auth_code");
  ASSERT_EQ(received_requests_.size(), 1U);
  EXPECT_EQ(net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES,
            received_requests_.at(0).load_flags);
  std::string body = GetRequestBodyAsString(&received_requests_.at(0));
  EXPECT_EQ(std::string::npos, body.find("device_type=chrome"));
  EXPECT_TRUE(auth.HasPendingFetch());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {},
                                     kGetTokenPairValidResponse);
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, StartAuthCodeForOAuth2TokenExchange_DeviceId) {
  MockGaiaConsumer consumer;
  GaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchangeWithDeviceId("auth_code",
                                                       "device_ABCDE_1");

  ASSERT_EQ(1U, received_requests_.size());
  EXPECT_EQ(net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES,
            received_requests_.at(0).load_flags);
  std::string body = GetRequestBodyAsString(&received_requests_.at(0));
  EXPECT_NE(std::string::npos, body.find("device_type=chrome"));
  EXPECT_NE(std::string::npos, body.find("device_id=device_ABCDE_1"));
}

TEST_F(GaiaAuthFetcherTest, StartAuthCodeForOAuth2TokenExchange_Failure) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientOAuthFailure(_)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartAuthCodeForOAuth2TokenExchange("auth_code");
  EXPECT_TRUE(auth.HasPendingFetch());

  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_FORBIDDEN);
  EXPECT_FALSE(auth.HasPendingFetch());
}


TEST_F(GaiaAuthFetcherTest, MergeSessionSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnMergeSessionSuccess("<html></html>")).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartMergeSession("myubertoken", std::string());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {},
                                     "<html></html>");

  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, MultiloginSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuthMultiloginFinished(::testing::Property(
                  &OAuthMultiloginResult::error,
                  ::testing::Eq(GoogleServiceAuthError::AuthErrorNone()))))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartOAuthMultilogin(
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {},
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
  EXPECT_CALL(consumer, OnOAuthMultiloginFinished(::testing::Property(
                            &OAuthMultiloginResult::error,
                            ::testing::Eq(GoogleServiceAuthError(
                                GoogleServiceAuthError::REQUEST_CANCELED)))))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartOAuthMultilogin(
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::ERR_ABORTED, net::HTTP_OK, {},
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
  EXPECT_CALL(consumer, OnOAuthMultiloginFinished(::testing::Property(
                            &OAuthMultiloginResult::error,
                            ::testing::Eq(GoogleServiceAuthError(
                                GoogleServiceAuthError::SERVICE_ERROR)))))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartOAuthMultilogin(
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>());

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {},
                                     "\n{\"status\": \"ERROR\"}");

  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, UberAuthTokenSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnUberAuthTokenSuccess("uberToken")).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.StartTokenFetchForUberAuthExchange("myAccessToken",
                                          true /* is_bound_to_channel_id */);

  EXPECT_TRUE(auth.HasPendingFetch());
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {}, "uberToken");

  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthFetcherTest, StartOAuthLogin) {
  // OAuthLogin returns the same as the ClientLogin endpoint, minus CAPTCHA
  // responses.
  std::string data("SID=sid\nLSID=lsid\nAuth=auth\n");

  GaiaAuthConsumer::ClientLoginResult result;
  result.lsid = "lsid";
  result.sid = "sid";
  result.token = "auth";
  result.data = data;

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginSuccess(result)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(/*body=*/"", /*headers=*/"",
                                           oauth_login_gurl_, /*load_flags=*/0,
                                           NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {}, data);
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

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(std::string()),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  ASSERT_EQ(received_requests_.size(), 1U);
  EXPECT_EQ(net::LOAD_NORMAL, received_requests_.at(0).load_flags);
  EXPECT_EQ(GaiaUrls::GetInstance()->gaia_url(),
            received_requests_.at(0).site_for_cookies);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {}, data);
}

TEST_F(GaiaAuthFetcherTest, LogOutSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnLogOutSuccess()).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->LogOutURLWithSource(std::string()),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK);
}

TEST_F(GaiaAuthFetcherTest, LogOutFailure) {
  net::Error error_no = net::ERR_CONNECTION_RESET;
  net::URLRequestStatus status(net::URLRequestStatus::FAILED, error_no);

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(error_no);
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnLogOutFailure(expected_error)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->LogOutURLWithSource(std::string()),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(error_no);
}

TEST_F(GaiaAuthFetcherTest, GetCheckConnectionInfo) {
  std::string data(R"(
      [{"carryBackToken": "token1", "url": "http://www.google.com"}])");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnGetCheckConnectionInfoSuccess(data)).Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());

  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"",
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          std::string()),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {}, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenSuccess) {
  std::string data("{}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnOAuth2RevokeTokenCompleted(
                            GaiaAuthConsumer::TokenRevocationStatus::kSuccess))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_OK, {}, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenCanceled) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kConnectionCanceled))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::ERR_ABORTED);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenFailed) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kConnectionFailed))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::ERR_CERT_CONTAINS_ERRORS);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenTimeout) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kConnectionTimeout))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::ERR_TIMED_OUT);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenInvalidToken) {
  std::string data("{\"error\" : \"invalid_token\"}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kInvalidToken))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_BAD_REQUEST, {}, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenInvalidRequest) {
  std::string data("{\"error\" : \"invalid_request\"}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kInvalidRequest))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_BAD_REQUEST, {}, data);
}

TEST_F(GaiaAuthFetcherTest, RevokeOAuth2TokenServerError) {
  std::string data("{}");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuth2RevokeTokenCompleted(
                  GaiaAuthConsumer::TokenRevocationStatus::kServerError))
      .Times(1);

  TestGaiaAuthFetcher auth(&consumer, std::string(), GetURLLoaderFactory());
  auth.CreateAndStartGaiaFetcherForTesting(
      /*body=*/"", /*headers=*/"", GaiaUrls::GetInstance()->oauth2_revoke_url(),
      /*load_flags=*/0, NO_TRAFFIC_ANNOTATION_YET);
  auth.TestOnURLLoadCompleteInternal(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                                     {}, data);
}
