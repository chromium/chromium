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
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
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

static RemoteConsentResolutionData CreateRemoteConsentResolutionData() {
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
    return CreateFlow(&delegate_, mode, false, "", "", "");
  }

  void CreateFlowWithEnableGranularPermissions(
      const bool enable_granular_permissions) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE,
                      enable_granular_permissions, "", "", "");
  }

  void CreateFlowWithDeviceId(const std::string& device_id) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE, false,
                      device_id, "", "");
  }

  void CreateFlowWithSelectedUserId(const std::string& selected_user_id) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE, false,
                      "", selected_user_id, "");
  }

  void CreateFlowWithConsentResult(const std::string& consent_result) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE,
                      false, "", "", consent_result);
  }

  void CreateFlow(MockDelegate* delegate,
                  OAuth2MintTokenFlow::Mode mode,
                  const bool enable_granular_permissions,
                  const std::string& device_id,
                  const std::string& selected_user_id,
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
  CreateFlowWithSelectedUserId("user_id1");
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
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_BadJson) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>("foo"));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseJsonFailure, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_NoAccessToken) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kTokenResponseNoAccessToken));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseMintTokenFailure, 1);
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
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_RemoteConsentFailure) {
  CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kInvalidRemoteConsentResponse));
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kParseRemoteConsentFailure, 1);
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
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_NullDelegate) {
  network::mojom::URLResponseHead head;
  CreateFlow(nullptr, OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE, false, "",
             "", "");
  ProcessApiCallFailure(net::ERR_FAILED, &head, nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_NonNullDelegate) {
  network::mojom::URLResponseHead head;
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallFailure(net::ERR_FAILED, &head, nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure_NullHead) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  EXPECT_CALL(delegate_, OnMintTokenFailure(_));
  ProcessApiCallFailure(net::ERR_FAILED, nullptr, nullptr);
  histogram_tester_.ExpectUniqueSample(
      kOAuth2MintTokenApiCallResultHistogram,
      OAuth2MintTokenApiCallResult::kApiCallFailure, 1);
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
}
