// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_provider_fetcher.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using ::testing::_;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace content {

class FederatedProviderFetcherTest : public RenderViewHostImplTestHarness {
 protected:
  FederatedProviderFetcherTest() = default;
  ~FederatedProviderFetcherTest() override = default;

  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FederatedProviderFetcherTest, FailedToFetchWellKnown) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.token = GURL("https://idp.example/token.php");
            endpoints.accounts = GURL("https://idp.example/accounts.php");

            IdentityProviderMetadata metadata;
            metadata.idp_login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    endpoints, metadata);
          }));

  // Returns a 404 for the fetch of the well-known file.
  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            std::move(callback).Run(
                {ParseStatus::kHttpNotFoundError, net::HTTP_NOT_FOUND},
                well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get a kWellKnownHttpNotFound.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(result[0].error->result,
                      blink::mojom::FederatedAuthRequestResult::
                          kWellKnownHttpNotFound);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest, FailedToFetchWellKnownButNoEnforcement) {
  feature_list_.InitAndEnableFeature(
      features::kFedCmWithoutWellKnownEnforcement);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.token = GURL("https://idp.example/token.php");
            endpoints.accounts = GURL("https://idp.example/accounts.php");

            IdentityProviderMetadata metadata;
            metadata.idp_login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    endpoints, metadata);
          }));

  // Returns a 404 for the fetch of the well-known file.
  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            std::move(callback).Run(
                {ParseStatus::kHttpNotFoundError, net::HTTP_NOT_FOUND},
                well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get no error in the result.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            EXPECT_TRUE(result[0].wellknown.provider_urls.empty());
            EXPECT_EQ(result[0].endpoints.token,
                      GURL("https://idp.example/token.php"));
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest, FailedToFetchConfig) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 404 for the fetch of the config file.
  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            std::move(callback).Run(
                {ParseStatus::kHttpNotFoundError, net::HTTP_NOT_FOUND},
                /*endpoints=*/{}, /*metadata=*/{});
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {GURL("https://idp.example/fedcm.json")};
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get a kConfigHttpNotFound.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(
                result[0].error->result,
                blink::mojom::FederatedAuthRequestResult::kConfigHttpNotFound);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest, SucceedsToFetchConfigButInvalidResponse) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    /*endpoints=*/{}, /*metadata=*/{});
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {GURL("https://idp.example/fedcm.json")};
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get a kConfigHttpNotFound.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(result[0].error->result,
                      blink::mojom::FederatedAuthRequestResult::
                          kConfigInvalidResponse);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest, SuccessfullAndValidResponse) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.token = GURL("https://idp.example/token.php");
            endpoints.accounts = GURL("https://idp.example/accounts.php");

            IdentityProviderMetadata metadata;
            metadata.idp_login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    endpoints, metadata);
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {GURL("https://idp.example/fedcm.json")};
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get a kConfigHttpNotFound.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest,
       TooManyProvidersInWellKnownLeadsToErrorWellKnownTooBig) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.token = GURL("https://idp.example/token.php");
            endpoints.accounts = GURL("https://idp.example/accounts.php");

            IdentityProviderMetadata metadata;
            metadata.idp_login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    endpoints, metadata);
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {
                GURL("https://idp.example/fedcm.json"),
                GURL("https://idp.example/one-too-many.json")};
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get a kConfigHttpNotFound.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(
                result[0].error->result,
                blink::mojom::FederatedAuthRequestResult::kWellKnownTooBig);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest,
       ProvidersUrlsIgnoredWhenAuthZIsEnabledAndAccountEndpointsMatch) {
  // When the AuthZ feature is enabled, the well-known file can have more than
  // one provider_url.
  feature_list_.InitAndEnableFeature(features::kFedCmAuthz);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig(_, _, _, _, _))
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.token = GURL("https://idp.example/token.php");
            endpoints.accounts = GURL("https://idp.example/accounts.php");

            IdentityProviderMetadata metadata;
            metadata.idp_login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    endpoints, metadata);
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {GURL("https://idp.example/fedcm.json")};
            well_known.accounts = GURL("https://idp.example/accounts.php");
            well_known.login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get a kConfigHttpNotFound.
  fetcher.Start(
      {GURL("https://idp.example/fedcm.json")}, blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FederatedProviderFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FederatedProviderFetcherTest, ValidFetchResult) {
  // The most basic valid fetch result is one where:
  // (a) both the well-known and the config files were loaded successfully
  // (b) there is an accounts and token endpoint in the config file
  // (c) the well-known file contains the configURL
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_FALSE(result.error);
}

TEST_F(FederatedProviderFetcherTest, InvalidMissingAcccountsEndpoint) {
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidCrossOriginAcccountsEndpoint) {
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://cross-origin.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidMissingTokenEndpoint) {
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidCrossOriginTokenEndpoint) {
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://cross-origin.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidCrossOriginSigninUrl) {
  feature_list_.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://cross-origin.example/sign-in");
  result.metadata = metadata;
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidConfigUrlNotInProviders) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://another-idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigNotInWellKnown);
}

TEST_F(FederatedProviderFetcherTest, InvalidConfigUrlNotInWellKnown) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/another-file.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigNotInWellKnown);
}

TEST_F(FederatedProviderFetcherTest, InvalidWellKnownTooManyProviders) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/fedcm.json"),
      GURL("https://idp.example/another-one.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kWellKnownTooBig);
}

TEST_F(FederatedProviderFetcherTest, SkippingTheChecksWithTheWellKnownFlag) {
  feature_list_.InitAndEnableFeature(
      features::kFedCmWithoutWellKnownEnforcement);

  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/another-file.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(FederatedProviderFetcherTest, InvalidWellKnownWithoutSignInUrl) {
  feature_list_.InitAndEnableFeature(features::kFedCmAuthz);
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.accounts = GURL("https://idp.example/accounts");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(FederatedProviderFetcherTest, InvalidWellKnownWithoutAccountsEndpoint) {
  feature_list_.InitAndEnableFeature(features::kFedCmAuthz);
  FederatedProviderFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.login_url = GURL("https://idp.example/signin");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(FederatedProviderFetcherTest,
       ValidWellKnownWithMatchingAccountsAndSignInUrl) {
  feature_list_.InitAndEnableFeature(features::kFedCmAuthz);
  FederatedProviderFetcher::FetchResult result;
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  result.metadata = metadata;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.accounts = GURL("https://idp.example/accounts");
  result.wellknown.login_url = GURL("https://idp.example/sign-in");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(FederatedProviderFetcherTest,
       FailResultEvenIfConfigUrlMatchesWhenAccountsEndpointIsAvailable) {
  // In this test, we verify that when the accounts endpoint is available and
  // not matching the one provided in the configURL, we do to allow falling
  // back to checking based on the fact that the provider_url contains the
  // configURL.
  feature_list_.InitAndEnableFeature(features::kFedCmAuthz);
  FederatedProviderFetcher::FetchResult result;
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  result.metadata = metadata;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.accounts = GURL("https://idp.example/another-accounts");
  result.wellknown.login_url = GURL("https://idp.example/sign-in");

  // Even if the configURL is present in the provider_urls, the presence of
  // the accounts_endpoint disqualifies this configuration.
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  // EXPECT_EQ(result.error->result, blink::mojom::FederatedAuthRequestResult::
  //                                     kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidEmptyConfig) {
  FederatedProviderFetcher::FetchResult result;

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FederatedProviderFetcherTest, InvalidNetworkError) {
  FederatedProviderFetcher::FetchResult result;
  result.error = FederatedProviderFetcher::FetchError(
      blink::mojom::FederatedAuthRequestResult::kConfigHttpNotFound,
      FedCmRequestIdTokenStatus::kConfigHttpNotFound,
      /*additional_console_error_message=*/std::nullopt);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FederatedProviderFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigHttpNotFound);
}

}  // namespace content
