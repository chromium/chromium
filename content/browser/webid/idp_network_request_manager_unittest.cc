// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include <array>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using AccountList = content::IdpNetworkRequestManager::AccountList;
using ClientMetadata = content::IdpNetworkRequestManager::ClientMetadata;
using Endpoints = content::IdpNetworkRequestManager::Endpoints;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using AccountsRequestCallback =
    content::IdpNetworkRequestManager::AccountsRequestCallback;
using RevokeResponse = content::IdpNetworkRequestManager::RevokeResponse;
using LoginState = content::IdentityRequestAccount::LoginState;

namespace content {

namespace {

// Values for testing. Real minimum and ideal sizes are different.
const int kTestIdpBrandIconMinimumSize = 16;
const int kTestIdpBrandIconIdealSize = 32;

const char kTestIdpUrl[] = "https://idp.test";
const char kTestRpUrl[] = "https://rp.test";
const char kTestManifestListUrl[] = "https://idp.test/.well-known/fedcm.json";
const char kTestManifestUrl[] = "https://idp.test/fedcm.json";
const char kTestAccountsEndpoint[] = "https://idp.test/accounts_endpoint";
const char kTestTokenEndpoint[] = "https://idp.test/token_endpoint";
const char kTestClientMetadataEndpoint[] =
    "https://idp.test/client_metadata_endpoint";
const char kTestRevocationEndpoint[] = "https://idp.test/revocation_endpoint";

class IdpNetworkRequestManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<IdpNetworkRequestManager>(
        GURL(kTestIdpUrl), url::Origin::Create(GURL(kTestRpUrl)),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::mojom::ClientSecurityState::New());
  }

  void TearDown() override { manager_.reset(); }

  std::tuple<FetchStatus, std::set<std::string>>
  SendManifestListRequestAndWaitForResponse(const char* test_data) {
    GURL manifest_list_url(kTestManifestListUrl);
    test_url_loader_factory().AddResponse(manifest_list_url.spec(), test_data);

    base::RunLoop run_loop;
    FetchStatus parsed_fetch_status;
    std::set<std::string> parsed_urls;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus fetch_status, const std::set<std::string>& urls) {
          parsed_fetch_status = fetch_status;
          parsed_urls = urls;
          run_loop.Quit();
        });
    manager().FetchManifestList(std::move(callback));
    run_loop.Run();

    return {parsed_fetch_status, parsed_urls};
  }

  std::tuple<FetchStatus, IdentityProviderMetadata>
  SendManifestRequestAndWaitForResponse(const char* test_data) {
    GURL manifest_url(kTestManifestUrl);
    test_url_loader_factory().AddResponse(manifest_url.spec(), test_data);

    base::RunLoop run_loop;
    FetchStatus parsed_fetch_status;
    IdentityProviderMetadata parsed_idp_metadata;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus fetch_status, Endpoints endpoints,
            IdentityProviderMetadata idp_metadata) {
          parsed_fetch_status = fetch_status;
          parsed_idp_metadata = std::move(idp_metadata);
          run_loop.Quit();
        });
    manager().FetchManifest(kTestIdpBrandIconIdealSize,
                            kTestIdpBrandIconMinimumSize, std::move(callback));
    run_loop.Run();

    return {parsed_fetch_status, parsed_idp_metadata};
  }

  std::tuple<FetchStatus, AccountList> SendAccountsRequestAndWaitForResponse(
      const char* test_accounts,
      const char* client_id = "",
      bool send_id_and_referrer = false) {
    GURL accounts_endpoint(kTestAccountsEndpoint);
    test_url_loader_factory().AddResponse(accounts_endpoint.spec(),
                                          test_accounts);

    base::RunLoop run_loop;
    FetchStatus parsed_accounts_response;
    AccountList parsed_accounts;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus response, AccountList accounts) {
          parsed_accounts_response = response;
          parsed_accounts = accounts;
          run_loop.Quit();
        });
    manager().SendAccountsRequest(accounts_endpoint, client_id,
                                  std::move(callback));
    run_loop.Run();

    return {parsed_accounts_response, parsed_accounts};
  }

  std::string SendTokenRequestAndWaitForResponse(
      const char* account,
      const char* request,
      net::HttpStatusCode http_status = net::HTTP_OK) {
    const char response[] = R"({"id_token": "token"})";
    GURL token_endpoint(kTestTokenEndpoint);
    test_url_loader_factory().AddResponse(token_endpoint.spec(), response,
                                          http_status);

    std::string token;
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus status, const std::string& token_response) {
          token = token_response;
          run_loop.Quit();
        });
    manager().SendTokenRequest(token_endpoint, account, request,
                               std::move(callback));
    run_loop.Run();
    return token;
  }

  ClientMetadata SendClientMetadataRequestAndWaitForResponse(
      const char* client_id,
      net::HttpStatusCode http_status = net::HTTP_OK) {
    const char response[] = R"({})";
    GURL client_id_endpoint(kTestClientMetadataEndpoint);
    test_url_loader_factory().AddResponse(
        client_id_endpoint.spec() + "?client_id=" + client_id, response,
        http_status);

    ClientMetadata data;
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus status, ClientMetadata metadata) {
          data = metadata;
          run_loop.Quit();
        });
    manager().FetchClientMetadata(client_id_endpoint, client_id,
                                  std::move(callback));
    run_loop.Run();
    return data;
  }

  RevokeResponse SendRevokeRequestAndWaitForResponse(
      const char* client_id,
      const char* hint,
      net::HttpStatusCode http_status = net::HTTP_NO_CONTENT) {
    GURL revocation_endpoint(kTestRevocationEndpoint);
    test_url_loader_factory().AddResponse(revocation_endpoint.spec(), "",
                                          http_status);

    RevokeResponse status;
    base::RunLoop run_loop;
    auto callback =
        base::BindLambdaForTesting([&](RevokeResponse revoke_status) {
          status = revoke_status;
          run_loop.Quit();
        });
    manager().SendRevokeRequest(revocation_endpoint, client_id, hint,
                                std::move(callback));
    run_loop.Run();
    return status;
  }
  IdpNetworkRequestManager& manager() { return *manager_; }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdpNetworkRequestManager> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
};

TEST_F(IdpNetworkRequestManagerTest, ParseAccountEmpty) {
  const auto* test_empty_account_json = R"({
  "accounts" : []
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_empty_account_json);

  EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountSingle) {
  const auto* test_single_account_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_single_account_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMultiple) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    },
    {
      "id" : "5678",
      "email": "sam@idp.test",
      "name": "Sam G. Test",
      "given_name": "Sam",
      "picture": "https://idp.test/profile/2"
    }
  ]
  })";
  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ(2UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].id);
  EXPECT_EQ("5678", accounts[1].id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountOptionalFields) {
  // given_name and picture fields are optional
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ("1234", accounts[0].id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountRequiredFields) {
  {
    const auto* test_accounts_missing_account_id_json = R"({"accounts" : [{
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }]})";
    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(
            test_accounts_missing_account_id_json);

    EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_email_json = R"({"accounts" : [{
      "id" : "1234",
      "name": "Ken R. Example"
    }]})";
    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_email_json);

    EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_name_json = R"({"accounts" : [{
      "id" : "1234",
      "email": "ken@idp.test"
    }]})";
    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_name_json);

    EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountPictureUrl) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "picture": "https://idp.test/profile/1234"
    },
    {
      "id" : "567",
      "email": "sam@idp.test",
      "name": "Sam R. Example",
      "picture": "invalid_url"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_TRUE(accounts[0].picture.is_valid());
  EXPECT_EQ(GURL("https://idp.test/profile/1234"), accounts[0].picture);
  EXPECT_FALSE(accounts[1].picture.is_valid());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountUnicode) {
  auto TestAccountWithKeyValue = [](const std::string& key,
                                    const std::string& value) {
    const auto* json = R"({
     "accounts" : [
        {
          "id" : "1234",
          "email": "ken@idp.test",
          "%s": "%s"
        }
      ]
    })";
    return base::StringPrintf(json, key.c_str(), value.c_str());
  };

  std::array<std::string, 3> test_values{"ascii", "🦖", "مجید"};

  for (auto& test_value : test_values) {
    const auto& accounts_json = TestAccountWithKeyValue("name", test_value);

    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(accounts_json.c_str());

    EXPECT_EQ(1UL, accounts.size());
    EXPECT_EQ(test_value, accounts[0].name);
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountInvalid) {
  const auto* test_invalid_account_json = "{}";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMalformed) {
  const auto* test_invalid_account_json = "malformed_json";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseManifestList) {
  FetchStatus fetch_status;
  std::set<std::string> urls;

  std::tie(fetch_status, urls) = SendManifestListRequestAndWaitForResponse(R"({
  "provider_urls": ["https://idp.test/fedcm.json"]
  })");
  EXPECT_EQ(FetchStatus::kSuccess, fetch_status);
  EXPECT_EQ(std::set<std::string>{kTestManifestUrl}, urls);

  // Value not a list
  std::tie(fetch_status, urls) = SendManifestListRequestAndWaitForResponse(R"({
  "provider_urls": "https://idp.test/fedcm.json"
  })");
  EXPECT_EQ(FetchStatus::kInvalidResponseError, fetch_status);

  // Toplevel not a dictionary
  std::tie(fetch_status, urls) = SendManifestListRequestAndWaitForResponse(R"(
  ["https://idp.test/fedcm.json"]
  )");
  EXPECT_EQ(FetchStatus::kInvalidResponseError, fetch_status);

  // Incorrect key
  std::tie(fetch_status, urls) = SendManifestListRequestAndWaitForResponse(R"({
  "providers": ["https://idp.test/fedcm.json"]
  })");
  EXPECT_EQ(FetchStatus::kInvalidResponseError, fetch_status);

  // Array entry not a string
  std::tie(fetch_status, urls) = SendManifestListRequestAndWaitForResponse(R"({
  "provider_urls": [1]
  })");
  EXPECT_EQ(FetchStatus::kInvalidResponseError, fetch_status);
}

// Test that the "alpha" value in the "branding" JSON is ignored.
TEST_F(IdpNetworkRequestManagerTest, ParseManifestBrandingRemoveAlpha) {
  const char test_json[] = R"({
  "branding" : {
    "background_color": "#20202020"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendManifestRequestAndWaitForResponse(test_json);

  EXPECT_EQ(FetchStatus::kSuccess, fetch_status);
  EXPECT_EQ(SkColorSetARGB(0xff, 0x20, 0x20, 0x20),
            idp_metadata.brand_background_color);
}

TEST_F(IdpNetworkRequestManagerTest, ParseManifestBrandingInvalidColor) {
  const char test_json[] = R"({
  "branding" : {
    "background_color": "fake_color"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendManifestRequestAndWaitForResponse(test_json);

  EXPECT_EQ(FetchStatus::kSuccess, fetch_status);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseManifestIgnoreInsufficientContrastTextColor) {
  const char test_json[] = R"({
  "branding" : {
    "background_color": "#000000",
    "color": "#010101"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendManifestRequestAndWaitForResponse(test_json);

  EXPECT_EQ(FetchStatus::kSuccess, fetch_status);
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseManifestBrandingIgnoreCustomTextColorNoCustomBackgroundColor) {
  const char test_json[] = R"({
  "branding" : {
    "color": "blue"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendManifestRequestAndWaitForResponse(test_json);

  EXPECT_EQ(FetchStatus::kSuccess, fetch_status);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

TEST_F(IdpNetworkRequestManagerTest, ParseManifestBrandingSelectBestSize) {
  const char test_json[] = R"({
  "branding" : {
    "icons": [
      {
        "url": "https://example.com/10.png",
        "size": 10
      },
      {
        "url": "https://example.com/16.png",
        "size": 16
      },
      {
        "url": "https://example.com/39.png",
        "size": 39
      },
      {
        "url": "https://example.com/40.png",
        "size": 40
      },
      {
        "url": "https://example.com/41.png",
        "size": 41
      }
    ]
  }
  })";

  ASSERT_EQ(32, kTestIdpBrandIconIdealSize);
  // 32 / kMaskableWebIconSafeZoneRatio = 40

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendManifestRequestAndWaitForResponse(test_json);

  EXPECT_EQ(FetchStatus::kSuccess, fetch_status);
  EXPECT_EQ("https://example.com/40.png", idp_metadata.brand_icon_url.spec());
}

// Tests that we send the correct referrer for account requests.
TEST_F(IdpNetworkRequestManagerTest, AccountRequestReferrer) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestAccountsEndpoint), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_EQ(false, request.referrer.is_valid());
      });
  test_url_loader_factory().SetInterceptor(interceptor);

  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  ASSERT_TRUE(called);
  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
}

// Verifies that we correctly check the signed-in status.
TEST_F(IdpNetworkRequestManagerTest, AccountSignedInStatus) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestAccountsEndpoint), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_FALSE(request.referrer.is_valid());
      });
  test_url_loader_factory().SetInterceptor(interceptor);

  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "approved_clients": ["xxx"]
    },
    {
      "id" : "2",
      "email": "jim@idp.test",
      "name": "Jim R. Example",
      "approved_clients": []
    },
    {
      "id" : "3",
      "email": "rashida@idp.test",
      "name": "Rashida R. Example",
      "approved_clients": ["yyy"]
    },
    {
      "id" : "4",
      "email": "wei@idp.test",
      "name": "Wei R. Example"
    },
    {
      "id" : "5",
      "email": "hans@idp.test",
      "name": "Hans R. Example",
      "approved_clients": ["xxx", "yyy"]
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json, "xxx");

  EXPECT_TRUE(called);
  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  ASSERT_EQ(5ul, accounts.size());
  ASSERT_TRUE(accounts[0].login_state.has_value());
  EXPECT_EQ(LoginState::kSignIn, *accounts[0].login_state);
  ASSERT_TRUE(accounts[1].login_state.has_value());
  EXPECT_EQ(LoginState::kSignUp, *accounts[1].login_state);
  ASSERT_TRUE(accounts[2].login_state.has_value());
  EXPECT_EQ(LoginState::kSignUp, *accounts[2].login_state);
  EXPECT_FALSE(accounts[3].login_state.has_value());
  ASSERT_TRUE(accounts[4].login_state.has_value());
  EXPECT_EQ(LoginState::kSignIn, *accounts[4].login_state);
}

// Tests the token request implementation.
TEST_F(IdpNetworkRequestManagerTest, TokenRequest) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestTokenEndpoint), request.url);
        EXPECT_EQ(GURL(kTestRpUrl), request.referrer);

        // Check that the request body is correct (should be "request")
        ASSERT_NE(request.request_body, nullptr);
        ASSERT_EQ(1ul, request.request_body->elements()->size());
        const network::DataElement& elem =
            request.request_body->elements()->at(0);
        ASSERT_EQ(network::DataElement::Tag::kBytes, elem.type());
        const network::DataElementBytes& byte_elem =
            elem.As<network::DataElementBytes>();
        EXPECT_EQ("request", byte_elem.AsStringPiece());
      });
  test_url_loader_factory().SetInterceptor(interceptor);
  std::string token = SendTokenRequestAndWaitForResponse("account", "request");
  ASSERT_TRUE(called);
  ASSERT_EQ("token", token);
}

// Tests the client metadata implementation.
TEST_F(IdpNetworkRequestManagerTest, ClientMetadata) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        std::string url_string =
            std::string(kTestClientMetadataEndpoint) + "?client_id=xxx";
        EXPECT_EQ(GURL(url_string), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_EQ(GURL(kTestRpUrl), request.referrer);
      });
  test_url_loader_factory().SetInterceptor(interceptor);
  ClientMetadata data = SendClientMetadataRequestAndWaitForResponse("xxx");
  ASSERT_TRUE(called);
  ASSERT_EQ("", data.privacy_policy_url);
  ASSERT_EQ("", data.terms_of_service_url);
}

// Tests the revoke implementation.
TEST_F(IdpNetworkRequestManagerTest, Revoke) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestRpUrl), request.referrer);
        // Check that the request body is correct
        ASSERT_NE(request.request_body, nullptr);
        ASSERT_EQ(1ul, request.request_body->elements()->size());
        const network::DataElement& elem =
            request.request_body->elements()->at(0);
        ASSERT_EQ(network::DataElement::Tag::kBytes, elem.type());
        const network::DataElementBytes& byte_elem =
            elem.As<network::DataElementBytes>();
        EXPECT_EQ("client_id=xxx&hint=yyy", byte_elem.AsStringPiece());
      });
  test_url_loader_factory().SetInterceptor(interceptor);
  RevokeResponse status = SendRevokeRequestAndWaitForResponse("xxx", "yyy");
  ASSERT_TRUE(called);
  ASSERT_EQ(RevokeResponse::kSuccess, status);
}

TEST_F(IdpNetworkRequestManagerTest, RevokeError) {
  RevokeResponse status =
      SendRevokeRequestAndWaitForResponse("xxx", "yyy", net::HTTP_FORBIDDEN);
  ASSERT_EQ(RevokeResponse::kError, status);
}

// Tests that we correctly records metrics regarding approved_clients.
TEST_F(IdpNetworkRequestManagerTest, RecordApprovedClientsMetrics) {
  base::HistogramTester histogram_tester;
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestAccountsEndpoint), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_FALSE(request.referrer.is_valid());
      });
  test_url_loader_factory().SetInterceptor(interceptor);

  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "approved_clients": []
    },
    {
      "id" : "2",
      "email": "jim@idp.test",
      "name": "Jim R. Example",
      "approved_clients": ["xxx"]
    },
    {
      "id" : "3",
      "email": "rashida@idp.test",
      "name": "Rashida R. Example",
      "approved_clients": ["xxx", "yyy"]
    },
    {
      "id" : "4",
      "email": "wei@idp.test",
      "name": "Wei R. Example"
    }
   ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json, "xxx");

  EXPECT_TRUE(called);
  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  ASSERT_EQ(4ul, accounts.size());

  histogram_tester.ExpectTotalCount("Blink.FedCm.ApprovedClientsExistence", 4);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsExistence", 1,
                                     3);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsExistence", 0,
                                     1);

  histogram_tester.ExpectTotalCount("Blink.FedCm.ApprovedClientsSize", 3);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsSize", 0, 1);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsSize", 1, 1);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsSize", 2, 1);
}

}  // namespace

}  // namespace content
