// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include <array>
#include <string>
#include <tuple>
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using AccountList = content::IdpNetworkRequestManager::AccountList;
using AccountsResponse = content::IdpNetworkRequestManager::AccountsResponse;
using AccountsRequestCallback =
    content::IdpNetworkRequestManager::AccountsRequestCallback;
using RevokeResponse = content::IdpNetworkRequestManager::RevokeResponse;

namespace content {

namespace {

const char kTestIdpUrl[] = "https://idp.test";
const char kTestRpUrl[] = "https://rp.test";
const char kTestAccountsEndpoint[] = "https://idp.test/accounts_endpoint";
const char kTestRevokeEndpoint[] = "https://idp.test/revoke_endpoint";

class IdpNetworkRequestManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<IdpNetworkRequestManager>(
        GURL(kTestIdpUrl), url::Origin::Create(GURL(kTestRpUrl)),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  void TearDown() override { manager_.reset(); }

  std::tuple<AccountsResponse, AccountList, IdentityProviderMetadata>
  SendAccountsRequestAndWaitForResponse(const char* test_accounts) {
    GURL accounts_endpoint(kTestAccountsEndpoint);
    test_url_loader_factory().AddResponse(accounts_endpoint.spec(),
                                          test_accounts);

    base::RunLoop run_loop;
    AccountsResponse parsed_accounts_response;
    AccountList parsed_accounts;
    IdentityProviderMetadata parsed_idp_metadata;
    auto callback = base::BindLambdaForTesting(
        [&](AccountsResponse response, AccountList accounts,
            IdentityProviderMetadata idp_metadata) {
          parsed_accounts_response = response;
          parsed_accounts = accounts;
          parsed_idp_metadata = std::move(idp_metadata);
          run_loop.Quit();
        });
    manager().SendAccountsRequest(accounts_endpoint, std::move(callback));
    run_loop.Run();

    return {parsed_accounts_response, parsed_accounts,
            std::move(parsed_idp_metadata)};
  }

  RevokeResponse SendRevokeRequestAndWaitForResponse(
      const char* client_id,
      const char* account_id,
      net::HttpStatusCode http_status = net::HTTP_NO_CONTENT) {
    GURL revoke_endpoint(kTestRevokeEndpoint);
    test_url_loader_factory().AddResponse(revoke_endpoint.spec(), "",
                                          http_status);

    RevokeResponse status;
    base::RunLoop run_loop;
    auto callback =
        base::BindLambdaForTesting([&](RevokeResponse revoke_status) {
          status = revoke_status;
          run_loop.Quit();
        });
    manager().SendRevokeRequest(revoke_endpoint, client_id, account_id,
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

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_empty_account_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountSingle) {
  const auto* test_single_account_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    }
  ]
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_single_account_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].sub);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMultiple) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    },
    {
      "sub" : "5678",
      "email": "sam@idp.test",
      "name": "Sam G. Test",
      "given_name": "Sam",
      "picture": "https://idp.test/profile/2"
    }
  ]
  })";
  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(2UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].sub);
  EXPECT_EQ("5678", accounts[1].sub);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountOptionalFields) {
  // given_name and picture fields are optional
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ("1234", accounts[0].sub);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountRequiredFields) {
  {
    const auto* test_accounts_missing_sub_json = R"({"accounts" : [{
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }]})";
    AccountsResponse accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_sub_json);

    EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_email_json = R"({"accounts" : [{
      "sub" : "1234",
      "name": "Ken R. Example"
    }]})";
    AccountsResponse accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_email_json);

    EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_name_json = R"({"accounts" : [{
      "sub" : "1234",
      "email": "ken@idp.test"
    }]})";
    AccountsResponse accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_name_json);

    EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountPictureUrl) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "picture": "https://idp.test/profile/1234"
    },
    {
      "sub" : "567",
      "email": "sam@idp.test",
      "name": "Sam R. Example",
      "picture": "invalid_url"
    }
  ]
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
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
          "sub" : "1234",
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

    AccountsResponse accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(accounts_json.c_str());

    EXPECT_EQ(1UL, accounts.size());
    EXPECT_EQ(test_value, accounts[0].name);
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountInvalid) {
  const auto* test_invalid_account_json = "{}";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(AccountsResponse::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMalformed) {
  const auto* test_invalid_account_json = "malformed_json";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(AccountsResponse::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountBranding) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
  "branding" : {
    "foreground_color": "blue",
    "background_color": "#f0e0d0"
  }
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(SK_ColorBLUE, idp_metadata.brand_text_color);
  EXPECT_EQ(SkColorSetRGB(0xf0, 0xe0, 0xd0),
            idp_metadata.brand_background_color);
}

// Test that the "alpha" value in the "branding" JSON is ignored.
TEST_F(IdpNetworkRequestManagerTest, ParseAccountBrandingRemoveAlpha) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
  "branding" : {
    "background_color": "#20202020"
  }
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(SkColorSetARGB(0xff, 0x20, 0x20, 0x20),
            idp_metadata.brand_background_color);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountBrandingInvalidColor) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
  "branding" : {
    "background_color": "fake_color"
  }
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseAccountBrandingIgnoreInsufficientContrastTextColor) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
  "branding" : {
    "background_color": "#000000",
    "foreground_color": "#010101"
  }
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseAccountBrandingIgnoreCustomTextColorNoCustomBackgroundColor) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
  "branding" : {
    "foreground_color": "blue"
  }
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

// Tests the revoke implementation.
TEST_F(IdpNetworkRequestManagerTest, Revoke) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        ASSERT_NE(request.request_body, nullptr);
        ASSERT_EQ(1ul, request.request_body->elements()->size());
        const network::DataElement& elem =
            request.request_body->elements()->at(0);
        ASSERT_EQ(network::DataElement::Tag::kBytes, elem.type());
        const network::DataElementBytes& byte_elem =
            elem.As<network::DataElementBytes>();
        EXPECT_EQ("{\"request\":{\"client_id\":\"xxx\"},\"sub\":\"yyy\"}",
                  byte_elem.AsStringPiece());
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
}  // namespace

}  // namespace content
