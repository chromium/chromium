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
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using AccountList = content::IdpNetworkRequestManager::AccountList;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using AccountsRequestCallback =
    content::IdpNetworkRequestManager::AccountsRequestCallback;
using RevokeResponse = content::IdpNetworkRequestManager::RevokeResponse;

namespace content {

namespace {

// Values for testing. Real minimum and ideal sizes are different.
const int kTestIdpBrandIconMinimumSize = 16;
const int kTestIdpBrandIconIdealSize = 32;

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

  void SetBitmapSpecsForUrl(const std::string& url, int size, int color) {
    bitmap_specs_[GURL(url)] = std::make_pair(size, color);
  }

  std::tuple<FetchStatus, AccountList, IdentityProviderMetadata>
  SendAccountsRequestAndWaitForResponse(const char* test_accounts) {
    GURL accounts_endpoint(kTestAccountsEndpoint);
    test_url_loader_factory().AddResponse(accounts_endpoint.spec(),
                                          test_accounts);

    base::RunLoop run_loop;
    FetchStatus parsed_accounts_response;
    AccountList parsed_accounts;
    IdentityProviderMetadata parsed_idp_metadata;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus response, AccountList accounts,
            IdentityProviderMetadata idp_metadata) {
          parsed_accounts_response = response;
          parsed_accounts = accounts;
          parsed_idp_metadata = std::move(idp_metadata);
          run_loop.Quit();
        });
    manager().SendAccountsRequest(
        accounts_endpoint, kTestIdpBrandIconIdealSize,
        kTestIdpBrandIconMinimumSize,
        base::BindOnce(&IdpNetworkRequestManagerTest::DownloadBitmap,
                       base::Unretained(this)),
        std::move(callback));
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
  void DownloadBitmap(const GURL& url,
                      int ideal_icon_size,
                      WebContents::ImageDownloadCallback callback) {
    auto bitmap_specs_it = bitmap_specs_.find(url);
    CHECK(bitmap_specs_.empty() || bitmap_specs_it != bitmap_specs_.end());

    SkBitmap bitmap;
    if (bitmap_specs_it != bitmap_specs_.end()) {
      int bitmap_edge_size = bitmap_specs_it->second.first;
      int bitmap_color = bitmap_specs_it->second.second;

      bitmap.allocN32Pixels(bitmap_edge_size, bitmap_edge_size);
      bitmap.eraseColor(bitmap_color);
    }

    std::move(callback).Run(0, 200, url, {bitmap},
                            {gfx::Size(bitmap.width(), bitmap.height())});
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdpNetworkRequestManager> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  std::map<GURL, std::pair<int, SkColor>> bitmap_specs_;
};

TEST_F(IdpNetworkRequestManagerTest, ParseAccountEmpty) {
  const auto* test_empty_account_json = R"({
  "accounts" : []
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_empty_account_json);

  EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_single_account_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].account_id);
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
  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ(2UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].account_id);
  EXPECT_EQ("5678", accounts[1].account_id);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ("1234", accounts[0].account_id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountRequiredFields) {
  {
    const auto* test_accounts_missing_account_id_json = R"({"accounts" : [{
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }]})";
    FetchStatus accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(
            test_accounts_missing_account_id_json);

    EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_email_json = R"({"accounts" : [{
      "sub" : "1234",
      "name": "Ken R. Example"
    }]})";
    FetchStatus accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_email_json);

    EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_name_json = R"({"accounts" : [{
      "sub" : "1234",
      "email": "ken@idp.test"
    }]})";
    FetchStatus accounts_response;
    AccountList accounts;
    IdentityProviderMetadata idp_metadata;
    std::tie(accounts_response, accounts, idp_metadata) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_name_json);

    EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
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

    FetchStatus accounts_response;
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMalformed) {
  const auto* test_invalid_account_json = "malformed_json";

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(FetchStatus::kInvalidResponseError, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
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

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountBrandingSelectBestSize) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
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

  SetBitmapSpecsForUrl("https://example.com/10.png", 10, SK_ColorBLACK);
  SetBitmapSpecsForUrl("https://example.com/16.png", 16, SK_ColorBLACK);
  SetBitmapSpecsForUrl("https://example.com/39.png", 39, SK_ColorBLACK);
  SetBitmapSpecsForUrl("https://example.com/40.png", 40, SK_ColorBLUE);
  SetBitmapSpecsForUrl("https://example.com/41.png", 41, SK_ColorBLACK);

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
  EXPECT_FALSE(idp_metadata.brand_icon.isNull());
  EXPECT_EQ(SK_ColorBLUE, idp_metadata.brand_icon.getColor(0, 0));
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseAccountBrandingIncorrectSizeInMetadata) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ],
  "branding" : {
    "icons": [
      {
        "url": "https://example.com/icon.png",
        "size": 32
      }
    ]
  }
  })";

  SetBitmapSpecsForUrl("https://example.com/icon.png", 1, SK_ColorBLACK);

  FetchStatus accounts_response;
  AccountList accounts;
  IdentityProviderMetadata idp_metadata;
  std::tie(accounts_response, accounts, idp_metadata) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  // Downloaded brand icon should not be used because it is too small.
  EXPECT_TRUE(idp_metadata.brand_icon.isNull());

  // An invalid brand icon should not prevent sign in.
  EXPECT_EQ(FetchStatus::kSuccess, accounts_response);
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
