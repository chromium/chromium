// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/accounts_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::WithArg;

class AccountsFetcherTest : public RenderViewHostImplTestHarness {
 protected:
  AccountsFetcherTest() = default;
  ~AccountsFetcherTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(GURL("https://rp.example"));
    api_permission_delegate_ =
        std::make_unique<NiceMock<MockApiPermissionDelegate>>();
    permission_delegate_ = std::make_unique<NiceMock<MockPermissionDelegate>>();
    metrics_ = std::make_unique<Metrics>(ukm::kInvalidSourceId);
  }

  void ExpectSuccessResult(const std::vector<AccountsFetcher::Result>& results,
                           const GURL& expected_config_url,
                           const GURL& expected_accounts_endpoint,
                           const GURL& expected_token_endpoint) {
    ASSERT_EQ(results.size(), 1ul);
    const auto& result = results[0];
    EXPECT_EQ(result.idp_config_url, expected_config_url);
    EXPECT_FALSE(result.error);
    EXPECT_FALSE(result.accounts_fetched_time.is_null());
    ASSERT_TRUE(result.idp_info);
    EXPECT_EQ(result.idp_info->endpoints.accounts, expected_accounts_endpoint);
    EXPECT_EQ(result.idp_info->endpoints.token, expected_token_endpoint);
    ASSERT_TRUE(result.accounts);
    EXPECT_EQ(result.accounts->accounts.size(), 1ul);
  }

  std::unique_ptr<NiceMock<MockApiPermissionDelegate>> api_permission_delegate_;
  std::unique_ptr<NiceMock<MockPermissionDelegate>> permission_delegate_;
  std::unique_ptr<Metrics> metrics_;
};

// Asserts that when IDP info is cached, AccountsFetcher skips ConfigFetcher
// (well-known and config fetch) and directly requests accounts from the IDP.
TEST_F(AccountsFetcherTest, CachedIdpBypassesConfigFetcher) {
  const GURL kIdpConfigUrl("https://idp.example/fedcm.json");
  const GURL kAccountsEndpoint("https://idp.example/accounts.json");
  const GURL kTokenEndpoint("https://idp.example/token.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();

  // FetchWellKnown and FetchConfig should NOT be called because IDP is cached.
  EXPECT_CALL(*network_manager, FetchWellKnown).Times(0);
  EXPECT_CALL(*network_manager, FetchConfig).Times(0);

  IdpNetworkRequestManager::AccountsRequestCallback accounts_callback;

  // SendAccountsRequest SHOULD be called.
  EXPECT_CALL(*network_manager, SendAccountsRequest)
      .WillOnce(
          WithArg<2>([&accounts_callback](
                         IdpNetworkRequestManager::AccountsRequestCallback cb) {
            accounts_callback = std::move(cb);
            return true;
          }));

  base::RunLoop loop;
  AccountsFetcher fetcher(
      *main_rfh(), network_manager.get(), api_permission_delegate_.get(),
      permission_delegate_.get(),
      AccountsFetcher::FedCmFetchingParams(
          blink::mojom::RpMode::kPassive, /*icon_ideal_size=*/0,
          /*icon_minimum_size=*/0,
          password_manager::CredentialMediationRequirement::kOptional),
      base::BindLambdaForTesting(
          [&](base::TimeTicks well_known_and_config_fetched_time,
              std::vector<AccountsFetcher::Result> results) {
            EXPECT_TRUE(well_known_and_config_fetched_time.is_null());
            ExpectSuccessResult(results, kIdpConfigUrl, kAccountsEndpoint,
                                kTokenEndpoint);
            loop.Quit();
          }));

  // Build cached IdentityProviderInfo.
  auto config = blink::mojom::IdentityProviderConfig::New();
  config->config_url = kIdpConfigUrl;

  auto options = blink::mojom::IdentityProviderRequestOptions::New(
      std::move(config), "nonce", /*login_hint=*/"", /*domain_hint=*/"",
      /*fields=*/std::nullopt, /*params_json=*/std::nullopt,
      /*format=*/std::nullopt);

  IdpNetworkRequestManager::Endpoints endpoints;
  endpoints.accounts = kAccountsEndpoint;
  endpoints.token = kTokenEndpoint;

  auto idp_info = std::make_unique<IdentityProviderInfo>(
      options, endpoints, IdentityProviderMetadata(),
      blink::mojom::RpContext::kSignIn, blink::mojom::RpMode::kPassive,
      /*format=*/std::nullopt);

  std::vector<std::unique_ptr<IdentityProviderInfo>> cached_idp_infos;
  cached_idp_infos.push_back(std::move(idp_info));

  base::flat_map<GURL, AccountsFetcher::IdentityProviderGetInfo>
      token_request_get_infos;
  token_request_get_infos.emplace(
      kIdpConfigUrl,
      AccountsFetcher::IdentityProviderGetInfo(
          options.Clone(), blink::mojom::RpContext::kSignIn,
          blink::mojom::RpMode::kPassive, /*format=*/std::nullopt));

  fetcher.FetchAccountsForIdps(
      std::move(cached_idp_infos), token_request_get_infos, metrics_.get(),
      url::Origin::Create(GURL("https://rp.example")), base::DoNothing());

  // Provide 1 valid account in the response.
  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.accounts.push_back(
      base::MakeRefCounted<IdentityRequestAccount>(
          "123", "user@example.com", "User Name", "user@example.com",
          "User Name", "User", GURL(), /*phone=*/"", /*username=*/"",
          /*potentially_approved_site_hashes=*/std::vector<std::string>(),
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>()));

  // Invoke accounts response callback asynchronously.
  ASSERT_TRUE(accounts_callback);
  std::move(accounts_callback)
      .Run({ParseStatus::kSuccess, net::HTTP_OK}, std::move(accounts_response));

  loop.Run();
}

// Asserts that when no IDP info is cached, AccountsFetcher fetches well-known
// and config files via ConfigFetcher before requesting accounts from the IDP.
TEST_F(AccountsFetcherTest, UncachedIdpFetchesConfigAndAccounts) {
  const GURL kIdpConfigUrl("https://idp.example/fedcm.json");
  const GURL kAccountsEndpoint("https://idp.example/accounts.json");
  const GURL kTokenEndpoint("https://idp.example/token.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();

  // FetchConfig should be called for uncached IDP.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
          [kAccountsEndpoint, kTokenEndpoint](
              IdpNetworkRequestManager::FetchConfigCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.token = kTokenEndpoint;
            endpoints.accounts = kAccountsEndpoint;

            IdentityProviderMetadata metadata;
            metadata.idp_login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    endpoints, metadata);
          }));

  // FetchWellKnown should be called for uncached IDP.
  EXPECT_CALL(*network_manager, FetchWellKnown)
      .WillOnce(WithArg<1>(
          [kIdpConfigUrl](
              IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {kIdpConfigUrl};
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  IdpNetworkRequestManager::AccountsRequestCallback accounts_callback;

  // SendAccountsRequest SHOULD be called after ConfigFetcher finishes.
  EXPECT_CALL(*network_manager, SendAccountsRequest)
      .WillOnce(
          WithArg<2>([&accounts_callback](
                         IdpNetworkRequestManager::AccountsRequestCallback cb) {
            accounts_callback = std::move(cb);
            return true;
          }));

  base::RunLoop loop;
  AccountsFetcher fetcher(
      *main_rfh(), network_manager.get(), api_permission_delegate_.get(),
      permission_delegate_.get(),
      AccountsFetcher::FedCmFetchingParams(
          blink::mojom::RpMode::kPassive, /*icon_ideal_size=*/0,
          /*icon_minimum_size=*/0,
          password_manager::CredentialMediationRequirement::kOptional),
      base::BindLambdaForTesting(
          [&](base::TimeTicks well_known_and_config_fetched_time,
              std::vector<AccountsFetcher::Result> results) {
            EXPECT_FALSE(well_known_and_config_fetched_time.is_null());
            ExpectSuccessResult(results, kIdpConfigUrl, kAccountsEndpoint,
                                kTokenEndpoint);
            loop.Quit();
          }));

  auto config = blink::mojom::IdentityProviderConfig::New();
  config->config_url = kIdpConfigUrl;

  auto options = blink::mojom::IdentityProviderRequestOptions::New(
      std::move(config), "nonce", /*login_hint=*/"", /*domain_hint=*/"",
      /*fields=*/std::nullopt, /*params_json=*/std::nullopt,
      /*format=*/std::nullopt);

  ConfigFetcher::FetchRequest fetch_req(
      kIdpConfigUrl, /*force_skip_well_known_enforcement=*/false);

  base::flat_map<GURL, AccountsFetcher::IdentityProviderGetInfo>
      token_request_get_infos;
  token_request_get_infos.emplace(
      kIdpConfigUrl,
      AccountsFetcher::IdentityProviderGetInfo(
          options.Clone(), blink::mojom::RpContext::kSignIn,
          blink::mojom::RpMode::kPassive, /*format=*/std::nullopt));

  fetcher.FetchEndpointsForIdps(
      {fetch_req}, token_request_get_infos, metrics_.get(),
      url::Origin::Create(GURL("https://rp.example")), base::DoNothing());

  // Provide 1 valid account in the response.
  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.accounts.push_back(
      base::MakeRefCounted<IdentityRequestAccount>(
          "123", "user@example.com", "User Name", "user@example.com",
          "User Name", "User", GURL(), /*phone=*/"", /*username=*/"",
          /*potentially_approved_site_hashes=*/std::vector<std::string>(),
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>()));

  // Invoke accounts response callback asynchronously.
  ASSERT_TRUE(accounts_callback);
  std::move(accounts_callback)
      .Run({ParseStatus::kSuccess, net::HTTP_OK}, std::move(accounts_response));

  loop.Run();
}

}  // namespace content::webid
