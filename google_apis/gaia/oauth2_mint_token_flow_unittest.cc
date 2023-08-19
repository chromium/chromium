// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFlow.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::StrictMock;

namespace {

const char kValidTokenResponse[] =
    "{"
    "  \"token\": \"at1\","
    "  \"issueAdvice\": \"Auto\","
    "  \"expiresIn\": \"3600\","
    "  \"grantedScopes\": \"http://scope1 http://scope2\""
    "}";

const char kTokenResponseNoGrantedScopes[] =
    "{"
    "  \"token\": \"at1\","
    "  \"issueAdvice\": \"Auto\","
    "  \"expiresIn\": \"3600\""
    "}";

const char kTokenResponseEmptyGrantedScopes[] =
    "{"
    "  \"token\": \"at1\","
    "  \"issueAdvice\": \"Auto\","
    "  \"expiresIn\": \"3600\","
    "  \"grantedScopes\": \"\""
    "}";

const char kTokenResponseNoAccessToken[] =
    "{"
    "  \"issueAdvice\": \"Auto\""
    "}";

const char kValidRemoteConsentResponse[] =
    "{"
    "  \"issueAdvice\": \"remoteConsent\","
    "  \"resolutionData\": {"
    "    \"resolutionApproach\": \"resolveInBrowser\","
    "    \"resolutionUrl\": \"https://test.com/consent?param=value\","
    "    \"browserCookies\": ["
    "      {"
    "        \"name\": \"test_name\","
    "        \"value\": \"test_value\","
    "        \"domain\": \"test.com\","
    "        \"path\": \"/\","
    "        \"maxAgeSeconds\": \"60\","
    "        \"isSecure\": false,"
    "        \"isHttpOnly\": true,"
    "        \"sameSite\": \"none\""
    "      },"
    "      {"
    "        \"name\": \"test_name2\","
    "        \"value\": \"test_value2\","
    "        \"domain\": \"test.com\""
    "      }"
    "    ]"
    "  }"
    "}";

const char kInvalidRemoteConsentResponse[] =
    "{"
    "  \"issueAdvice\": \"remoteConsent\","
    "  \"resolutionData\": {"
    "    \"resolutionApproach\": \"resolveInBrowser\""
    "  }"
    "}";

const char kTokenBindingChallengeResponse[] = R"(
    {
      "tokenBindingResponse" : {
        "retryResponse" : {
          "challenge" : "SIGN_ME"
        }
      }
    }
  )";

constexpr base::StringPiece kVersion = "test_version";
constexpr base::StringPiece kChannel = "test_channel";
constexpr base::StringPiece kScopes[] = {"http://scope1", "http://scope2"};
constexpr base::StringPiece kClientId = "client1";

static RemoteConsentResolutionData CreateRemoteConsentResolutionData() {
  RemoteConsentResolutionData resolution_data;
  resolution_data.url = GURL("https://test.com/consent?param=value");
  resolution_data.cookies.push_back(
      *net::CanonicalCookie::CreateSanitizedCookie(
          resolution_data.url, "test_name", "test_value", "test.com", "/",
          base::Time(), base::Time(), base::Time(), false, true,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_DEFAULT, false,
          absl::nullopt));
  resolution_data.cookies.push_back(
      *net::CanonicalCookie::CreateSanitizedCookie(
          resolution_data.url, "test_name2", "test_value2", "test.com", "/",
          base::Time(), base::Time(), base::Time(), false, false,
          net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_DEFAULT, false,
          absl::nullopt));
  return resolution_data;
}

class MockDelegate : public OAuth2MintTokenFlow::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  MOCK_METHOD3(OnMintTokenSuccess,
               void(const std::string& access_token,
                    const std::set<std::string>& granted_scopes,
                    int time_to_live));
  MOCK_METHOD1(OnRemoteConsentSuccess,
               void(const RemoteConsentResolutionData& resolution_data));
  MOCK_METHOD1(OnMintTokenFailure,
               void(const GoogleServiceAuthError& error));
};

class MockMintTokenFlow : public OAuth2MintTokenFlow {
 public:
  explicit MockMintTokenFlow(MockDelegate* delegate,
                             OAuth2MintTokenFlow::Parameters parameters)
      : OAuth2MintTokenFlow(delegate, std::move(parameters)) {}
  ~MockMintTokenFlow() override = default;

  MOCK_METHOD0(CreateAccessTokenFetcher,
               std::unique_ptr<OAuth2AccessTokenFetcher>());

  // Moves the method to the public section to make it available for tests.
  std::string CreateApiCallBody() override {
    return OAuth2MintTokenFlow::CreateApiCallBody();
  }
};

}  // namespace

class OAuth2MintTokenFlowTest : public testing::Test {
 public:
  OAuth2MintTokenFlowTest()
      : head_200_(network::CreateURLResponseHead(net::HTTP_OK)) {}
  ~OAuth2MintTokenFlowTest() override {}

 protected:
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
    const base::StringPiece kExtensionId = "ext1";
    flow_ = std::make_unique<MockMintTokenFlow>(
        delegate,
        OAuth2MintTokenFlow::Parameters::CreateForExtensionFlow(
            kExtensionId, kClientId, kScopes, mode, enable_granular_permissions,
            kVersion, kChannel, device_id, selected_user_id, consent_result));
  }

  void CreateClientFlow() {
    const base::StringPiece kDeviceId = "test_device_id";
    flow_ = std::make_unique<MockMintTokenFlow>(
        &delegate_, OAuth2MintTokenFlow::Parameters::CreateForClientFlow(
                        kClientId, kScopes, kVersion, kChannel, kDeviceId));
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
  CreateClientFlow();
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

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponse) {
  {  // Access token missing.
    base::Value::Dict json =
        base::test::ParseJsonDict(kTokenResponseNoAccessToken);
    std::string access_token;
    std::set<std::string> granted_scopes;
    int time_to_live;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseMintTokenResponse(
        json, &access_token, &granted_scopes, &time_to_live));
    EXPECT_TRUE(access_token.empty());
  }
  {  // Granted scopes parameter is there but is empty.
    base::Value::Dict json =
        base::test::ParseJsonDict(kTokenResponseEmptyGrantedScopes);
    std::string access_token;
    std::set<std::string> granted_scopes;
    int time_to_live;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseMintTokenResponse(
        json, &access_token, &granted_scopes, &time_to_live));
    EXPECT_TRUE(granted_scopes.empty());
  }
  {  // Granted scopes parameter is missing.
    base::Value::Dict json =
        base::test::ParseJsonDict(kTokenResponseNoGrantedScopes);
    std::string access_token;
    std::set<std::string> granted_scopes;
    int time_to_live;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseMintTokenResponse(
        json, &access_token, &granted_scopes, &time_to_live));
    EXPECT_TRUE(granted_scopes.empty());
  }
  {  // All good.
    base::Value::Dict json = base::test::ParseJsonDict(kValidTokenResponse);
    std::string access_token;
    std::set<std::string> granted_scopes;
    int time_to_live;
    EXPECT_TRUE(OAuth2MintTokenFlow::ParseMintTokenResponse(
        json, &access_token, &granted_scopes, &time_to_live));
    EXPECT_EQ("at1", access_token);
    EXPECT_EQ(3600, time_to_live);
    EXPECT_EQ(std::set<std::string>({"http://scope1", "http://scope2"}),
              granted_scopes);
  }
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  RemoteConsentResolutionData resolution_data;
  ASSERT_TRUE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_EmptyCookies) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  json.FindListByDottedPath("resolutionData.browserCookies")->clear();
  RemoteConsentResolutionData resolution_data;
  EXPECT_TRUE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
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
  EXPECT_TRUE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
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
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoUrl) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(json.RemoveByDottedPath("resolutionData.resolutionUrl"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadUrl) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(
      json.SetByDottedPath("resolutionData.resolutionUrl", "not-a-url"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoApproach) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(json.RemoveByDottedPath("resolutionData.resolutionApproach"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadApproach) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  EXPECT_TRUE(
      json.SetByDottedPath("resolutionData.resolutionApproach", "badApproach"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
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
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
        json, &resolution_data));
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
    EXPECT_TRUE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
        json, &resolution_data));
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
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadCookieList) {
  base::Value::Dict json =
      base::test::ParseJsonDict(kValidRemoteConsentResponse);
  json.FindListByDottedPath("resolutionData.browserCookies")->Append(42);
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(
      OAuth2MintTokenFlow::ParseRemoteConsentResponse(json, &resolution_data));
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
  EXPECT_CALL(delegate_, OnMintTokenSuccess("at1", granted_scopes, 3600));
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

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess_TokenBindingChallenge) {
  CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromTokenBindingChallenge("SIGN_ME");
  EXPECT_CALL(delegate_, OnMintTokenFailure(expected_error));
  ProcessApiCallSuccess(head_200_.get(), std::make_unique<std::string>(
                                             kTokenBindingChallengeResponse));
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
