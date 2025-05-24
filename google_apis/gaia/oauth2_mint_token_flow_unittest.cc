// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFlow.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/cstring_view.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_response.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::ByRef;
using testing::Eq;
using testing::Field;
using testing::StrictMock;

namespace {

constexpr char kOAuth2MintTokenApiCallResultHistogram[] =
    "Signin.OAuth2MintToken.ApiCallResult";
constexpr char kOAuth2MintTokenResponseHistogram[] =
    "Signin.OAuth2MintToken.Response";

const char kValidTokenResponse[] =
    R"({
      "token": "at1",
      "issueAdvice": "Auto",
      "expiresIn": "3600",
      "grantedScopes": "http://scope1 http://scope2"
     })";

const char kValidTokenResponseEnrcypted[] =
    R"({
      "token": "at1",
      "issueAdvice": "Auto",
      "expiresIn": "3600",
      "grantedScopes": "http://scope1 http://scope2",
      "tokenBindingResponse" : {
        "directedResponse" : {}
      }
     })";

const char kTokenResponseNoGrantedScopes[] =
    R"({
      "token": "at1",
      "issueAdvice": "Auto",
      "expiresIn": "3600"
    })";

const char kTokenResponseEmptyGrantedScopes[] =
    R"({
      "token": "at1",
      "issueAdvice": "Auto",
      "expiresIn": "3600",
      "grantedScopes": ""
    })";

const char kTokenResponseNoAccessToken[] =
    R"({
     "issueAdvice": "Auto"
    })";

const char kValidRemoteConsentResponse[] =
    R"({
      "issueAdvice": "remoteConsent",
      "resolutionData": {
        "resolutionApproach": "resolveInBrowser",
        "resolutionUrl": "https://test.com/consent?param=value",
        "browserCookies": [
          {
            "name": "test_name",
            "value": "test_value",
            "domain": "test.com",
            "path": "/",
            "maxAgeSeconds": "60",
            "isSecure": false,
            "isHttpOnly": true,
            "sameSite": "none"
          },
          {
            "name": "test_name2",
            "value": "test_value2",
            "domain": "test.com"
          }
        ]
      }
    })";

const char kInvalidRemoteConsentResponse[] =
    R"({
      "issueAdvice": "remoteConsent",
      "resolutionData": {
        "resolutionApproach": "resolveInBrowser"
      }
    })";

constexpr std::string_view kVersion = "test_version";
constexpr std::string_view kChannel = "test_channel";
constexpr std::string_view kScopes[] = {"http://scope1", "http://scope2"};
constexpr std::string_view kClientId = "client1";

constexpr char kErrorTokenResponseInvalidCredentials[] =
    R"({
        "error": {
          "code": 401,
          "message": "Request had invalid authentication credentials.",
          "errors": [
            {
              "message": "Invalid Credentials",
              "domain": "global",
              "reason": "authError",
              "location": "Authorization",
              "locationType": "header"
            }
          ],
          "status": "UNAUTHENTICATED"
        }
      })";

constexpr char kErrorTokenResponseInvalidClientIdNoMessage[] =
    R"({
        "error": {
          "code": 400,
          "errors": [
            {
              "message": "bad client id: abcd",
              "domain": "com.google.oauth2",
              "reason": "invalidClientId"
            }
          ]
        }
      })";

constexpr char kErrorTokenResponseNoReason[] =
    R"({
      "error": {
        "code": 401,
        "message": "Some failure occured.",
        "errors": [
          {
            "domain": "global"
          }
        ],
        "status": "UNAUTHENTICATED"
      }
    })";

constexpr char kErrorTokenResponseNoMessageNoReason[] =
    R"({
      "error": {
        "code": 401,
        "errors": [
          {
            "domain": "global"
          }
        ],
        "status": "UNAUTHENTICATED"
      }
    })";

struct MintTokenFailureTestParam {
  std::string test_name;
  net::HttpStatusCode http_response_code = net::HTTP_OK;
  std::string response_body;
  GoogleServiceAuthError expected_error;
  OAuth2Response expected_oauth2_response = OAuth2Response::kOk;
};

std::string GetValidErrorTokenResponse(int http_response_code,
                                       base::cstring_view reason,
                                       base::cstring_view message) {
  static constexpr std::string_view kValidErrorTokenResponseFormat =
      R"({
        "error": {
          "code": %d,
          "message": "%s",
          "errors": [
            {
              "message": "%s",
              "domain": "com.google.oauth2",
              "reason": "%s"
            }
          ]
        }
      })";
  return base::StringPrintf(kValidErrorTokenResponseFormat, http_response_code,
                            message, message, reason);
}

std::vector<MintTokenFailureTestParam> GetMintTokenFailureTestParams() {
  return {
      {
          .test_name = "InvalidCredentials",
          .http_response_code = net::HTTP_UNAUTHORIZED,
          .response_body = kErrorTokenResponseInvalidCredentials,
          .expected_error =
              GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      CREDENTIALS_REJECTED_BY_SERVER),
          .expected_oauth2_response = OAuth2Response::kInvalidGrant,
      },
      {
          .test_name = "InvalidClientId",
          .http_response_code = net::HTTP_BAD_REQUEST,
          .response_body = GetValidErrorTokenResponse(
              net::HTTP_BAD_REQUEST, "invalidClientId", "bad client id: abcd"),
          .expected_error =
              GoogleServiceAuthError::FromServiceError("bad client id: abcd"),
          .expected_oauth2_response = OAuth2Response::kInvalidClient,
      },
      {
          .test_name = "RateLimitExceeded",
          .http_response_code = net::HTTP_FORBIDDEN,
          .response_body = GetValidErrorTokenResponse(
              net::HTTP_FORBIDDEN, "rateLimitExceeded", "rate limit exceeded"),
          .expected_error = GoogleServiceAuthError::FromServiceUnavailable(
              "rate limit exceeded"),
          .expected_oauth2_response = OAuth2Response::kRateLimitExceeded,
      },
      {
          .test_name = "BadRequest",
          .http_response_code = net::HTTP_BAD_REQUEST,
          .response_body = GetValidErrorTokenResponse(
              net::HTTP_BAD_REQUEST, "badRequest", "bad request"),
          .expected_error =
              GoogleServiceAuthError::FromServiceError("bad request"),
          .expected_oauth2_response = OAuth2Response::kInvalidRequest,
      },
      {
          .test_name = "InternalError",
          .http_response_code = net::HTTP_INTERNAL_SERVER_ERROR,
          .response_body = GetValidErrorTokenResponse(
              net::HTTP_INTERNAL_SERVER_ERROR, "internalError",
              "internal server error"),
          .expected_error = GoogleServiceAuthError::FromServiceUnavailable(
              "internal server error"),
          .expected_oauth2_response = OAuth2Response::kInternalFailure,
      },
      {
          .test_name = "InvalidScope",
          .http_response_code = net::HTTP_BAD_REQUEST,
          .response_body = GetValidErrorTokenResponse(
              net::HTTP_BAD_REQUEST, "invalidScope", "invalid scope: test"),
          .expected_error =
              GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
                  GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
                      kInvalidScope),
          .expected_oauth2_response = OAuth2Response::kInvalidScope,
      },
      {
          .test_name = "RestrictedClient",
          .http_response_code = net::HTTP_FORBIDDEN,
          .response_body = GetValidErrorTokenResponse(
              net::HTTP_FORBIDDEN, "restrictedClient",
              "request parameters violate OAuth2 client security restrictions"),
          .expected_error =
              GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
                  GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
                      kRestrictedClient),
          .expected_oauth2_response = OAuth2Response::kRestrictedClient,
      },
      {
          .test_name = "InvalidClientIdNoMessage",
          .http_response_code = net::HTTP_BAD_REQUEST,
          .response_body = kErrorTokenResponseInvalidClientIdNoMessage,
          .expected_error =
              GoogleServiceAuthError::FromServiceError("invalidClientId"),
          .expected_oauth2_response = OAuth2Response::kInvalidClient,
      },
      {
          .test_name = "ErrorNoReason",
          .http_response_code = net::HTTP_UNAUTHORIZED,
          .response_body = kErrorTokenResponseNoReason,
          .expected_error =
              GoogleServiceAuthError::FromServiceError("Some failure occured."),
          .expected_oauth2_response = OAuth2Response::kErrorUnexpectedFormat,
      },
      {
          .test_name = "ErrorNoMessageNoReason",
          .http_response_code = net::HTTP_UNAUTHORIZED,
          .response_body = kErrorTokenResponseNoMessageNoReason,
          .expected_error = GoogleServiceAuthError::FromServiceError(
              "Couldn't parse an error. HTTP code 401"),
          .expected_oauth2_response = OAuth2Response::kErrorUnexpectedFormat,
      },
      {
          .test_name = "UnknownError",
          .http_response_code = net::HTTP_UNAUTHORIZED,
          .response_body = GetValidErrorTokenResponse(net::HTTP_UNAUTHORIZED,
                                                      "thisErrorDoesNotExist",
                                                      "this is a fake error"),
          .expected_error =
              GoogleServiceAuthError::FromServiceError("this is a fake error"),
          .expected_oauth2_response = OAuth2Response::kUnknownError,
      },
      {
          .test_name = "NotAJson",
          .http_response_code = net::HTTP_UNAUTHORIZED,
          .response_body = "error=badFormat",
          .expected_error = GoogleServiceAuthError::FromServiceError(
              "Couldn't parse an error. HTTP code 401"),
          .expected_oauth2_response = OAuth2Response::kErrorUnexpectedFormat,
      },
      {
          .test_name = "NotAJson407",
          .http_response_code = net::HTTP_PROXY_AUTHENTICATION_REQUIRED,
          .response_body = "error=badFormat",
          .expected_error = GoogleServiceAuthError::FromServiceUnavailable(
              "Couldn't parse an error. HTTP code 407"),
          .expected_oauth2_response = OAuth2Response::kErrorUnexpectedFormat,
      },
      {
          .test_name = "NotAJson500",
          .http_response_code = net::HTTP_INTERNAL_SERVER_ERROR,
          .response_body = "error=badFormat",
          .expected_error = GoogleServiceAuthError::FromServiceUnavailable(
              "Couldn't parse an error. HTTP code 500"),
          .expected_oauth2_response = OAuth2Response::kErrorUnexpectedFormat,
      },
  };
}

MATCHER_P4(HasMintTokenResult,
           access_token,
           granted_scopes,
           time_to_live,
           is_token_encrypted,
           "") {
  return testing::ExplainMatchResult(
      AllOf(Field("access_token",
                  &OAuth2MintTokenFlow::MintTokenResult::access_token,
                  access_token),
            Field("granted_scopes",
                  &OAuth2MintTokenFlow::MintTokenResult::granted_scopes,
                  granted_scopes),
            Field("time_to_live",
                  &OAuth2MintTokenFlow::MintTokenResult::time_to_live,
                  time_to_live),
            Field("is_token_encrypted",
                  &OAuth2MintTokenFlow::MintTokenResult::is_token_encrypted,
                  is_token_encrypted)),
      arg, result_listener);
}

RemoteConsentResolutionData CreateRemoteConsentResolutionData() {
  RemoteConsentResolutionData resolution_data;
  resolution_data.url = GURL("https://test.com/consent?param=value");
  resolution_data.cookies.push_back(
      *net::CanonicalCookie::CreateSanitizedCookie(
          resolution_data.url, "test_name", "test_value", "test.com", "/",
          base::Time(), base::Time(), base::Time(), false, true,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_DEFAULT,
          std::nullopt, /*status=*/nullptr));
  resolution_data.cookies.push_back(
      *net::CanonicalCookie::CreateSanitizedCookie(
          resolution_data.url, "test_name2", "test_value2", "test.com", "/",
          base::Time(), base::Time(), base::Time(), false, false,
          net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_DEFAULT,
          std::nullopt, /*status=*/nullptr));
  return resolution_data;
}

class MockDelegate : public OAuth2MintTokenFlow::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD1(OnMintTokenSuccess,
               void(const OAuth2MintTokenFlow::MintTokenResult& result));
  MOCK_METHOD1(OnRemoteConsentSuccess,
               void(const RemoteConsentResolutionData& resolution_data));
  MOCK_METHOD1(OnMintTokenFailure, void(const GoogleServiceAuthError& error));
};

class MockMintTokenFlow : public OAuth2MintTokenFlow {
 public:
  explicit MockMintTokenFlow(MockDelegate* delegate,
                             OAuth2MintTokenFlow::Parameters parameters)
      : OAuth2MintTokenFlow(delegate, std::move(parameters)) {}
  ~MockMintTokenFlow() override = default;

  MOCK_METHOD0(CreateAccessTokenFetcher,
               std::unique_ptr<OAuth2AccessTokenFetcher>());

  // Moves methods to the public section to make them available for tests.
  std::string CreateApiCallBody() override {
    return OAuth2MintTokenFlow::CreateApiCallBody();
  }
  std::string CreateAuthorizationHeaderValue(
      const std::string& access_token) override {
    return OAuth2MintTokenFlow::CreateAuthorizationHeaderValue(access_token);
  }
  net::HttpRequestHeaders CreateApiCallHeaders() override {
    return OAuth2MintTokenFlow::CreateApiCallHeaders();
  }
};

}  // namespace

class OAuth2MintTokenFlowTest : public testing::Test {
 public:
  OAuth2MintTokenFlowTest()
      : head_200_(network::CreateURLResponseHead(net::HTTP_OK)) {}
  ~OAuth2MintTokenFlowTest() override {}

  const network::mojom::URLResponseHeadPtr head_200_;

  void CreateFlow(OAuth2MintTokenFlow::Mode mode) {
    return CreateFlow(&delegate_, mode, false, "", GaiaId(), "");
  }

  void CreateFlowWithEnableGranularPermissions(
      const bool enable_granular_permissions) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE,
                      enable_granular_permissions, "", GaiaId(), "");
  }

  void CreateFlowWithDeviceId(const std::string& device_id) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE, false,
                      device_id, GaiaId(), "");
  }

  void CreateFlowWithSelectedUserId(const GaiaId& selected_user_id) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE, false,
                      "", selected_user_id, "");
  }

  void CreateFlowWithConsentResult(const std::string& consent_result) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE,
                      false, "", GaiaId(), consent_result);
  }

  void CreateFlow(MockDelegate* delegate,
                  OAuth2MintTokenFlow::Mode mode,
                  const bool enable_granular_permissions,
                  const std::string& device_id,
                  const GaiaId& selected_user_id,
                  const std::string& consent_result) {
    const std::string_view kExtensionId = "ext1";
    flow_ = std::make_unique<MockMintTokenFlow>(
        delegate,
        OAuth2MintTokenFlow::Parameters::CreateForExtensionFlow(
            kExtensionId, kClientId, kScopes, mode, enable_granular_permissions,
            kVersion, kChannel, device_id, selected_user_id, consent_result));
  }

  void CreateClientFlow(const std::string& bound_oauth_token) {
    const std::string_view kDeviceId = "test_device_id";
    flow_ = std::make_unique<MockMintTokenFlow>(
        &delegate_, OAuth2MintTokenFlow::Parameters::CreateForClientFlow(
                        kClientId, kScopes, kVersion, kChannel, kDeviceId,
                        bound_oauth_token));
  }

  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) {
    flow_->ProcessApiCallSuccess(head, std::move(body));
  }

  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) {
    flow_->ProcessApiCallFailure(net_error, head, std::move(body));
  }

  // Expose functions publicly to the tests.
  std::optional<OAuth2MintTokenFlow::MintTokenResult> ParseMintTokenResponse(
      const base::Value::Dict& dict) {
    return OAuth2MintTokenFlow::ParseMintTokenResponse(dict);
  }
  bool ParseRemoteConsentResponse(
      const base::Value::Dict& dict,
      RemoteConsentResolutionData* resolution_data) {
    return OAuth2MintTokenFlow::ParseRemoteConsentResponse(dict,
                                                           resolution_data);
  }

 protected:
  std::unique_ptr<MockMintTokenFlow> flow_;
  StrictMock<MockDelegate> delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyIssueAdviceMode) {
  CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=none"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyRecordGrantMode) {
  CreateFlow(OAuth2MintTokenFlow::MODE_RECORD_GRANT);
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=true"
      "&response_type=none"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyMintTokenNoForceMode) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=token"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyMintTokenForceMode) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE);
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=true"
      "&response_type=token"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest,
       CreateApiCallBodyMintTokenWithGranularPermissionsEnabled) {
  CreateFlowWithEnableGranularPermissions(true);
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=none"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=true"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyMintTokenWithDeviceId) {
  CreateFlowWithDeviceId("device_id1");
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=none"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1"
      "&device_id=device_id1"
      "&device_type=chrome");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyMintTokenWithSelectedUserId) {
  CreateFlowWithSelectedUserId(GaiaId("user_id1"));
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=none"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1"
      "&selected_user_id=user_id1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyMintTokenWithConsentResult) {
  CreateFlowWithConsentResult("consent1");
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=token"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&origin=ext1"
      "&consent_result=consent1");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBodyClientAccessTokenFlow) {
  CreateClientFlow(/*bound_oauth_token=*/std::string());
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=token"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&device_id=test_device_id"
      "&device_type=chrome");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateAuthorizationHeaderValue) {
  CreateClientFlow(/*bound_oauth_token=*/std::string());
  std::string header =
      flow_->CreateAuthorizationHeaderValue("test_access_token");
  EXPECT_EQ(header, "Bearer test_access_token");
}

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallHeaders) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  net::HttpRequestHeaders headers = flow_->CreateApiCallHeaders();
  EXPECT_THAT(headers.GetHeader("X-OAuth-Client-ID"),
              testing::Optional(kClientId));
}

TEST_F(OAuth2MintTokenFlowTest,
       CreateApiCallBodyClientAccessTokenFlowWithBoundOAuthToken) {
  CreateClientFlow(/*bound_oauth_token=*/std::string());
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      "force=false"
      "&response_type=token"
      "&scope=http://scope1+http://scope2"
      "&enable_granular_permissions=false"
      "&client_id=client1"
      "&lib_ver=test_version"
      "&release_channel=test_channel"
      "&device_id=test_device_id"
      "&device_type=chrome");
  EXPECT_EQ(expected_body, body);
}

TEST_F(OAuth2MintTokenFlowTest, CreateAuthorizationHeaderValueBoundOAuthToken) {
  CreateClientFlow("test_bound_oauth_token");
  std::string header =
      flow_->CreateAuthorizationHeaderValue("test_access_token");
  EXPECT_EQ(header, "BoundOAuth test_bound_oauth_token");
}

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponseAccessTokenMissing) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kTokenResponseNoAccessToken);
  EXPECT_EQ(ParseMintTokenResponse(json), std::nullopt);
}

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponseEmptyGrantedScopes) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kTokenResponseEmptyGrantedScopes);
  EXPECT_EQ(ParseMintTokenResponse(json), std::nullopt);
}

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponseNoGrantedScopes) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kTokenResponseNoGrantedScopes);
  EXPECT_EQ(ParseMintTokenResponse(json), std::nullopt);
}

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponseGoodToken) {
  base::Value::Dict json = base::test::ParseJsonDict(kValidTokenResponse);
  EXPECT_THAT(
      ParseMintTokenResponse(json),
      testing::Optional(HasMintTokenResult(
          "at1", std::set<std::string>({"http://scope1", "http://scope2"}),
          base::Seconds(3600), false)));
}

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponseEncryptedToken) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidTokenResponseEnrcypted);
  EXPECT_THAT(
      ParseMintTokenResponse(json),
      testing::Optional(HasMintTokenResult(
          "at1", std::set<std::string>({"http://scope1", "http://scope2"}),
          base::Seconds(3600), true)));
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  RemoteConsentResolutionData resolution_data;
  ASSERT_TRUE(ParseRemoteConsentResponse(json, &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_EmptyCookies) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  json.FindListByDottedPath("resolutionData.browserCookies")->clear();
  RemoteConsentResolutionData resolution_data;
  EXPECT_TRUE(ParseRemoteConsentResponse(json, &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  expected_resolution_data.cookies.clear();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoCookies) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(json.RemoveByDottedPath("resolutionData.browserCookies"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_TRUE(ParseRemoteConsentResponse(json, &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  expected_resolution_data.cookies.clear();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoResolutionData) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(json.Remove("resolutionData"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoUrl) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(json.RemoveByDottedPath("resolutionData.resolutionUrl"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadUrl) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(
      json.SetByDottedPath("resolutionData.resolutionUrl", "not-a-url"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoApproach) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(json.RemoveByDottedPath("resolutionData.resolutionApproach"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadApproach) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(
      json.SetByDottedPath("resolutionData.resolutionApproach", "badApproach"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest,
       ParseRemoteConsentResponse_BadCookie_MissingRequiredField) {
  static const char* kRequiredFields[] = {"name", "value", "domain"};
  for (const auto* required_field : kRequiredFields) {
    base::Value::Dict json =
        base::test::ParseJsonDict(kValidRemoteConsentResponse);
    base::Value::List* cookies =
        json.FindListByDottedPath("resolutionData.browserCookies");
    ASSERT_TRUE(cookies);
    EXPECT_TRUE((*cookies)[0].GetDict().Remove(required_field));
    RemoteConsentResolutionData resolution_data;
    EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
    EXPECT_TRUE(resolution_data.url.is_empty());
    EXPECT_TRUE(resolution_data.cookies.empty());
  }
}

TEST_F(OAuth2MintTokenFlowTest,
       ParseRemoteConsentResponse_MissingCookieOptionalField) {
  static const char* kOptionalFields[] = {"path", "maxAgeSeconds", "isSecure",
                                          "isHttpOnly", "sameSite"};
  for (const auto* optional_field : kOptionalFields) {
    base::Value::Dict json =
        base::test::ParseJsonDict(kValidRemoteConsentResponse);
    base::Value::List* cookies =
        json.FindListByDottedPath("resolutionData.browserCookies");
    ASSERT_TRUE(cookies);
    EXPECT_TRUE((*cookies)[0].GetDict().Remove(optional_field));
    RemoteConsentResolutionData resolution_data;
    EXPECT_TRUE(ParseRemoteConsentResponse(json, &resolution_data));
    RemoteConsentResolutionData expected_resolution_data =
        CreateRemoteConsentResolutionData();
    EXPECT_EQ(resolution_data, expected_resolution_data);
  }
}

TEST_F(OAuth2MintTokenFlowTest,
       ParseRemoteConsentResponse_BadCookie_BadMaxAge) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  base::Value::List* cookies =
      json.FindListByDottedPath("resolutionData.browserCookies");
  ASSERT_TRUE(cookies);
  (*cookies)[0].GetDict().Set("maxAgeSeconds", "not-a-number");
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadCookieList) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  json.FindListByDottedPath("resolutionData.browserCookies")->Append(42);
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_NoBody) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseJsonFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kOkUnexpectedFormat, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_BadJson) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>("foo"));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseJsonFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kOkUnexpectedFormat, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_NoAccessToken) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kTokenResponseNoAccessToken));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseMintTokenFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kOkUnexpectedFormat, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_GoodToken) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  std::set<std::string> granted_scopes = {"http://scope1", "http://scope2"};
  EXPECT_CALL(delegate_,
              OnMintTokenSuccess(HasMintTokenResult(
                  "at1", granted_scopes, base::Seconds(3600), false)));
  ProcessApiCallSuccess(head_200_.get(),
                        std::make_unique<std::string>(kValidTokenResponse));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kMintTokenSuccess, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kOk, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_GoodRemoteConsent) {
  CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
  RemoteConsentResolutionData resolution_data =
      CreateRemoteConsentResolutionData();
  EXPECT_CALL(delegate_, OnRemoteConsentSuccess(Eq(ByRef(resolution_data))));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kValidRemoteConsentResponse));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kRemoteConsentSuccess, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kConsentRequired, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_RemoteConsentNoCookies) {
  constexpr std::string_view kValidRemoteConsentResponseNoCookies = R"(
      {
        "issueAdvice": "remoteConsent",
        "resolutionData": {
          "resolutionApproach": "resolveInBrowser",
          "resolutionUrl": "https://admin.google.com/ServiceNotAllowed"
      }
    })";

  CreateClientFlow(/*bound_oauth_token=*/std::string());
  RemoteConsentResolutionData resolution_data;
  resolution_data.url = GURL("https://admin.google.com/ServiceNotAllowed");
  EXPECT_CALL(delegate_, OnRemoteConsentSuccess(Eq(ByRef(resolution_data))));
  ProcessApiCallSuccess(
      head_200_.get(),
      std::make_unique<std::string>(kValidRemoteConsentResponseNoCookies));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kRemoteConsentSuccess, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kConsentRequired, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_RemoteConsentFailure) {
  CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kInvalidRemoteConsentResponse));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseRemoteConsentFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kOkUnexpectedFormat, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_TokenBindingChallenge) {
  const std::string kChallenge = "SIGN_ME";
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromTokenBindingChallenge(kChallenge);
  EXPECT_CALL(delegate_, OnMintTokenFailure(expected_error));
  network::mojom::URLResponseHeadPtr head(
      network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED));
  head->headers->SetHeader("X-Chrome-Auth-Token-Binding-Challenge", kChallenge);
  ProcessApiCallFailure(net::OK, head.get(), nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kChallengeResponseRequiredFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kTokenBindingChallenge,
                                       1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_NullDelegate) {
  network::mojom::URLResponseHead head;
  CreateFlow(nullptr, OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE, false, "",
             GaiaId(), "");
  ProcessApiCallFailure(net::ERR_FAILED, &head, nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
  histogram_tester_.ExpectTotalCount(kOAuth2MintTokenResponseHistogram, 0);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_NonNullDelegate) {
  network::mojom::URLResponseHead head;
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallFailure(net::ERR_FAILED, &head, nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
  histogram_tester_.ExpectTotalCount(kOAuth2MintTokenResponseHistogram, 0);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_NullHead) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallFailure(net::ERR_FAILED, nullptr, nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
  histogram_tester_.ExpectTotalCount(kOAuth2MintTokenResponseHistogram, 0);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_NoGrantedScopes) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  std::set<std::string> granted_scopes = {"http://scope1", "http://scope2"};
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kTokenResponseNoGrantedScopes));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseMintTokenFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       OAuth2Response::kOkUnexpectedFormat, 1);
}

class OAuth2MintTokenFlowApiCallFailureParamTest
    : public OAuth2MintTokenFlowTest,
      public testing::WithParamInterface<MintTokenFailureTestParam> {};

TEST_P(OAuth2MintTokenFlowApiCallFailureParamTest, Test) {
  CreateClientFlow(/*bound_oauth_token=*/std::string());
  EXPECT_CALL(delegate_, OnMintTokenFailure(GetParam().expected_error));
  network::mojom::URLResponseHeadPtr head(
      network::CreateURLResponseHead(GetParam().http_response_code));
  ProcessApiCallFailure(
      net::OK, head.get(),
      std::make_unique<std::string>(GetParam().response_body));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
  histogram_tester_.ExpectUniqueSample(kOAuth2MintTokenResponseHistogram,
                                       GetParam().expected_oauth2_response, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         OAuth2MintTokenFlowApiCallFailureParamTest,
                         testing::ValuesIn(GetMintTokenFailureTestParams()),
                         [](const auto& info) { return info.param.test_name; });
