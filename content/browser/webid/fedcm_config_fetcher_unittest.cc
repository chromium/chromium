// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_config_fetcher.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/public/common/content_features.h"
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

class FedCmConfigFetcherTest : public RenderViewHostImplTestHarness {
 protected:
  FedCmConfigFetcherTest() = default;
  ~FedCmConfigFetcherTest() override = default;

  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FedCmConfigFetcherTest, FailedToFetchWellKnown) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
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
  EXPECT_CALL(*network_manager, FetchWellKnown)
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
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(result[0].error->result,
                      blink::mojom::FederatedAuthRequestResult::
                          kWellKnownHttpNotFound);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest, FailedToFetchWellKnownButNoEnforcement) {
  feature_list_.InitAndEnableFeature(
      features::kFedCmWithoutWellKnownEnforcement);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
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
  EXPECT_CALL(*network_manager, FetchWellKnown)
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
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            EXPECT_TRUE(result[0].wellknown.provider_urls.empty());
            EXPECT_EQ(result[0].endpoints.token,
                      GURL("https://idp.example/token.php"));
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest, FailedToFetchConfig) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 404 for the fetch of the config file.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            std::move(callback).Run(
                {ParseStatus::kHttpNotFoundError, net::HTTP_NOT_FOUND},
                /*endpoints=*/{}, /*metadata=*/{});
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown)
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
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(
                result[0].error->result,
                blink::mojom::FederatedAuthRequestResult::kConfigHttpNotFound);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest, SucceedsToFetchConfigButInvalidResponse) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<4>(
          [](IdpNetworkRequestManager::FetchConfigCallback callback) {
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    /*endpoints=*/{}, /*metadata=*/{});
          }));

  EXPECT_CALL(*network_manager, FetchWellKnown)
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
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(result[0].error->result,
                      blink::mojom::FederatedAuthRequestResult::
                          kConfigInvalidResponse);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest, SuccessfullAndValidResponse) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
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

  EXPECT_CALL(*network_manager, FetchWellKnown)
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
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest,
       TooManyProvidersInWellKnownLeadsToErrorWellKnownTooBig) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
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

  EXPECT_CALL(*network_manager, FetchWellKnown)
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
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(
                result[0].error->result,
                blink::mojom::FederatedAuthRequestResult::kWellKnownTooBig);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest, ProvidersUrlsIgnoredWhenAccountEndpointsMatch) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
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

  EXPECT_CALL(*network_manager, FetchWellKnown)
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

  // Asserts that we get no error in the result.
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest,
       ProvidersUrlsCanbeEmptyWhenAccountEndpointsMatch) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
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

  EXPECT_CALL(*network_manager, FetchWellKnown)
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = GURL("https://idp.example/accounts.php");
            well_known.login_url =
                GURL("https://idp.example/idp_login_url.php");
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get no error in the result.
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            EXPECT_TRUE(result[0].wellknown.provider_urls.empty());
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(FedCmConfigFetcherTest, ValidFetchResult) {
  // The most basic valid fetch result is one where:
  // (a) both the well-known and the config files were loaded successfully
  // (b) there is an accounts and token endpoint in the config file
  // (c) the well-known file contains the configURL
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_FALSE(result.error);
}

TEST_F(FedCmConfigFetcherTest, InvalidMissingAcccountsEndpoint) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest, InvalidCrossOriginAcccountsEndpoint) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://cross-origin.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest, InvalidMissingTokenEndpoint) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest, InvalidCrossOriginTokenEndpoint) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://cross-origin.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest, InvalidCrossOriginSigninUrl) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://cross-origin.example/sign-in");
  result.metadata = metadata;
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest, InvalidConfigUrlNotInProviders) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://another-idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigNotInWellKnown);
}

TEST_F(FedCmConfigFetcherTest, InvalidConfigUrlNotInWellKnown) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/another-file.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigNotInWellKnown);
}

TEST_F(FedCmConfigFetcherTest, InvalidWellKnownTooManyProviders) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/fedcm.json"),
      GURL("https://idp.example/another-one.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kWellKnownTooBig);
}

TEST_F(FedCmConfigFetcherTest, SkippingTheChecksWithTheWellKnownFlag) {
  feature_list_.InitAndEnableFeature(
      features::kFedCmWithoutWellKnownEnforcement);

  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/another-file.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(FedCmConfigFetcherTest, InvalidWellKnownWithoutSignInUrl) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.accounts = GURL("https://idp.example/accounts");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(FedCmConfigFetcherTest, InvalidWellKnownWithoutAccountsEndpoint) {
  FedCmConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.login_url = GURL("https://idp.example/signin");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(FedCmConfigFetcherTest, ValidWellKnownWithMatchingAccountsAndSignInUrl) {
  FedCmConfigFetcher::FetchResult result;
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
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(FedCmConfigFetcherTest,
       FailResultEvenIfConfigUrlMatchesWhenAccountsEndpointIsAvailable) {
  // In this test, we verify that when the accounts endpoint is available and
  // not matching the one provided in the configURL, we do to allow falling
  // back to checking based on the fact that the provider_url contains the
  // configURL.
  FedCmConfigFetcher::FetchResult result;
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
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  // EXPECT_EQ(result.error->result, blink::mojom::FederatedAuthRequestResult::
  //                                     kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest,
       SuccessResultEvenWithEmptyAccountsEndpointWithLightweightFedCm) {
  // Validate that when LightweightFedCM is enabled, it's permissible to have an
  // empty accounts_endpoint set.
  feature_list_.InitAndEnableFeature(features::kFedCmLightweightMode);
  FedCmConfigFetcher::FetchResult result;
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  result.metadata = metadata;
  result.endpoints.accounts = GURL();
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.login_url = GURL("https://idp.example/sign-in");
  result.wellknown.accounts = GURL();

  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(FedCmConfigFetcherTest,
       ProvidersUrlsCanbeEmptyWhenLightweightIsEnabled) {
  // Validate that when LightweightFedCM is enabled,
  // it's permissible to have an empty accounts_endpoint set and
  // no provider_config_urls, so long as the accounts url is empty in both the
  // wellknown and config.
  feature_list_.InitAndEnableFeature(features::kFedCmLightweightMode);
  FedCmConfigFetcher::FetchResult result;
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  result.metadata = metadata;
  result.endpoints.accounts = GURL();
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.login_url = GURL("https://idp.example/sign-in");
  result.wellknown.accounts = GURL();

  result.wellknown.provider_urls = {};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(FedCmConfigFetcherTest,
       FailureResultWithMismatchingAccountsEndpointWithLightweightFedCm) {
  // Validate that when LightweightFedCM is enabled, it's still an error to have
  // a non-same-origin accounts endpoint.
  feature_list_.InitAndEnableFeature(features::kFedCmLightweightMode);
  FedCmConfigFetcher::FetchResult result;
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  result.metadata = metadata;
  result.endpoints.accounts = GURL("https://not-the-idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.login_url = GURL();
  result.wellknown.accounts = GURL();

  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(FedCmConfigFetcherTest,
       FailureResultWithEmptyAccountsEndpointWithoutLightweightFedCm) {
  // Validate that when LightweightFedCM is disabled, it's still an error to not
  // define an accounts endpoint.
  FedCmConfigFetcher::FetchResult result;
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  result.metadata = metadata;
  result.endpoints.accounts = GURL();
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.login_url = GURL();
  result.wellknown.accounts = GURL();

  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(FedCmConfigFetcherTest, InvalidEmptyConfig) {
  FedCmConfigFetcher::FetchResult result;

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigInvalidResponse);
}

TEST_F(FedCmConfigFetcherTest, InvalidNetworkError) {
  FedCmConfigFetcher::FetchResult result;
  result.error = FedCmConfigFetcher::FetchError(
      blink::mojom::FederatedAuthRequestResult::kConfigHttpNotFound,
      FedCmRequestIdTokenStatus::kConfigHttpNotFound,
      /*additional_console_error_message=*/std::nullopt);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedAuthRequestResult::kConfigHttpNotFound);
}

TEST_F(FedCmConfigFetcherTest, RegisteredIdpSkipsWellKnownCheck) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdPRegistration);
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  FedCmConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
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
  EXPECT_CALL(*network_manager, FetchWellKnown)
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            std::move(callback).Run(
                {ParseStatus::kHttpNotFoundError, net::HTTP_NOT_FOUND},
                well_known);
          }));

  base::RunLoop loop;

  // Asserts that we get success despite well-known failing.
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/true}},
      blink::mojom::RpMode::kPassive,
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<FedCmConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            loop.Quit();
          }));

  loop.Run();
}

}  // namespace content
