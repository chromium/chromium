// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/config_fetcher.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::StrictMock;
using ::testing::WithArg;

class ConfigFetcherTest : public RenderViewHostImplTestHarness {
 protected:
  ConfigFetcherTest() = default;
  ~ConfigFetcherTest() override = default;

  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ConfigFetcherTest, FailedToFetchWellKnown) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting([&loop](std::vector<ConfigFetcher::FetchResult>
                                             result) {
        EXPECT_EQ(result.size(), 1ul);
        EXPECT_TRUE(result[0].error);
        EXPECT_EQ(result[0].error->result,
                  blink::mojom::FederatedRequestResult::kWellKnownHttpNotFound);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, FailedToFetchWellKnownButNoEnforcement) {
  feature_list_.InitAndEnableFeature(
      features::kFedCmWithoutWellKnownEnforcement);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
  fetcher.Start({{GURL("https://idp.example/fedcm.json"),
                  /*force_skip_well_known_enforcement=*/false}},
                /*icon_ideal_size=*/0,
                /*icon_minimum_size=*/0,
                base::BindLambdaForTesting(
                    [&loop](std::vector<ConfigFetcher::FetchResult> result) {
                      EXPECT_EQ(result.size(), 1ul);
                      EXPECT_FALSE(result[0].error);
                      EXPECT_TRUE(result[0].wellknown.provider_urls.empty());
                      EXPECT_EQ(result[0].endpoints.token,
                                GURL("https://idp.example/token.php"));
                      loop.Quit();
                    }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, FailedToFetchConfig) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 404 for the fetch of the config file.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(
                result[0].error->result,
                blink::mojom::FederatedRequestResult::kConfigHttpNotFound);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, SucceedsToFetchConfigButInvalidResponse) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting([&loop](std::vector<ConfigFetcher::FetchResult>
                                             result) {
        EXPECT_EQ(result.size(), 1ul);
        EXPECT_TRUE(result[0].error);
        EXPECT_EQ(result[0].error->result,
                  blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, SuccessfullAndValidResponse) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
  fetcher.Start({{GURL("https://idp.example/fedcm.json"),
                  /*force_skip_well_known_enforcement=*/false}},
                /*icon_ideal_size=*/0,
                /*icon_minimum_size=*/0,
                base::BindLambdaForTesting(
                    [&loop](std::vector<ConfigFetcher::FetchResult> result) {
                      EXPECT_EQ(result.size(), 1ul);
                      EXPECT_FALSE(result[0].error);
                      loop.Quit();
                    }));

  loop.Run();
}

TEST_F(ConfigFetcherTest,
       TooManyProvidersInWellKnownLeadsToErrorWellKnownTooBig) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_TRUE(result[0].error);
            EXPECT_EQ(result[0].error->result,
                      blink::mojom::FederatedRequestResult::kWellKnownTooBig);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, ProvidersUrlsIgnoredWhenAccountEndpointsMatch) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
  fetcher.Start({{GURL("https://idp.example/fedcm.json"),
                  /*force_skip_well_known_enforcement=*/false}},
                /*icon_ideal_size=*/0,
                /*icon_minimum_size=*/0,
                base::BindLambdaForTesting(
                    [&loop](std::vector<ConfigFetcher::FetchResult> result) {
                      EXPECT_EQ(result.size(), 1ul);
                      EXPECT_FALSE(result[0].error);
                      loop.Quit();
                    }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, ProvidersUrlsCanbeEmptyWhenAccountEndpointsMatch) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  // Returns a 200 but with an empty and invalid response.
  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
  fetcher.Start({{GURL("https://idp.example/fedcm.json"),
                  /*force_skip_well_known_enforcement=*/false}},
                /*icon_ideal_size=*/0,
                /*icon_minimum_size=*/0,
                base::BindLambdaForTesting(
                    [&loop](std::vector<ConfigFetcher::FetchResult> result) {
                      EXPECT_EQ(result.size(), 1ul);
                      EXPECT_FALSE(result[0].error);
                      EXPECT_TRUE(result[0].wellknown.provider_urls.empty());
                      loop.Quit();
                    }));

  loop.Run();
}

TEST_F(ConfigFetcherTest, ValidFetchResult) {
  // The most basic valid fetch result is one where:
  // (a) both the well-known and the config files were loaded successfully
  // (b) there is an accounts and token endpoint in the config file
  // (c) the well-known file contains the configURL
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/sign-in");
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_FALSE(result.error);
}

TEST_F(ConfigFetcherTest, InvalidMissingAcccountsEndpoint) {
  ConfigFetcher::FetchResult result;
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest, InvalidCrossOriginAcccountsEndpoint) {
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://cross-origin.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest, InvalidMissingTokenEndpoint) {
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest, InvalidCrossOriginTokenEndpoint) {
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://cross-origin.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest, InvalidCrossOriginSigninUrl) {
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://cross-origin.example/sign-in");
  result.metadata = metadata;
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest, InvalidConfigUrlNotInProviders) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://another-idp.example/fedcm.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigNotInWellKnown);
}

TEST_F(ConfigFetcherTest, InvalidConfigUrlNotInWellKnown) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/another-file.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigNotInWellKnown);
}

TEST_F(ConfigFetcherTest, InvalidWellKnownTooManyProviders) {
  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/fedcm.json"),
      GURL("https://idp.example/another-one.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kWellKnownTooBig);
}

TEST_F(ConfigFetcherTest, SkippingTheChecksWithTheWellKnownFlag) {
  feature_list_.InitAndEnableFeature(
      features::kFedCmWithoutWellKnownEnforcement);

  IdentityProviderMetadata metadata;
  metadata.idp_login_url = GURL("https://idp.example/idp_login_url.php");

  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {
      GURL("https://idp.example/another-file.json")};
  result.identity_provider_config_url = GURL("https://idp.example/fedcm.json");
  result.metadata = std::move(metadata);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(ConfigFetcherTest, InvalidWellKnownWithoutSignInUrl) {
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.accounts = GURL("https://idp.example/accounts");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(ConfigFetcherTest, InvalidWellKnownWithoutAccountsEndpoint) {
  ConfigFetcher::FetchResult result;
  result.endpoints.accounts = GURL("https://idp.example/accounts");
  result.endpoints.token = GURL("https://idp.example/token");
  result.wellknown.provider_urls = {GURL("https://idp.example/fedcm.json")};
  result.wellknown.login_url = GURL("https://idp.example/signin");
  result.identity_provider_config_url =
      GURL("https://idp.example/another-file.json");

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(ConfigFetcherTest, ValidWellKnownWithMatchingAccountsAndSignInUrl) {
  ConfigFetcher::FetchResult result;
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
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(ConfigFetcherTest,
       FailResultEvenIfConfigUrlMatchesWhenAccountsEndpointIsAvailable) {
  // In this test, we verify that when the accounts endpoint is available and
  // not matching the one provided in the configURL, we do to allow falling
  // back to checking based on the fact that the provider_url contains the
  // configURL.
  ConfigFetcher::FetchResult result;
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
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  // EXPECT_EQ(result.error->result, blink::mojom::FederatedRequestResult::
  //                                     kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest,
       SuccessResultEvenWithEmptyAccountsEndpointWithLightweightFedCm) {
  // Validate that when LightweightFedCM is enabled, it's permissible to have an
  // empty accounts_endpoint set.
  feature_list_.InitAndEnableFeature(features::kFedCmLightweightMode);
  ConfigFetcher::FetchResult result;
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
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(ConfigFetcherTest, ProvidersUrlsCanbeEmptyWhenLightweightIsEnabled) {
  // Validate that when LightweightFedCM is enabled,
  // it's permissible to have an empty accounts_endpoint set and
  // no provider_config_urls, so long as the accounts url is empty in both the
  // wellknown and config.
  feature_list_.InitAndEnableFeature(features::kFedCmLightweightMode);
  ConfigFetcher::FetchResult result;
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
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_FALSE(result.error);
}

TEST_F(ConfigFetcherTest,
       FailureResultWithMismatchingAccountsEndpointWithLightweightFedCm) {
  // Validate that when LightweightFedCM is enabled, it's still an error to have
  // a non-same-origin accounts endpoint.
  feature_list_.InitAndEnableFeature(features::kFedCmLightweightMode);
  ConfigFetcher::FetchResult result;
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
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(ConfigFetcherTest,
       FailureResultWithEmptyAccountsEndpointWithoutLightweightFedCm) {
  // Validate that when LightweightFedCM is disabled, it's still an error to not
  // define an accounts endpoint.
  ConfigFetcher::FetchResult result;
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
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
}

TEST_F(ConfigFetcherTest, InvalidEmptyConfig) {
  ConfigFetcher::FetchResult result;

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigInvalidResponse);
}

TEST_F(ConfigFetcherTest, InvalidNetworkError) {
  ConfigFetcher::FetchResult result;
  result.error = ConfigFetcher::FetchError(
      blink::mojom::FederatedRequestResult::kConfigHttpNotFound,
      RequestIdTokenStatus::kConfigHttpNotFound,
      /*additional_console_error_message=*/std::nullopt);

  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  fetcher.ValidateAndMaybeSetError(result);

  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.error->result,
            blink::mojom::FederatedRequestResult::kConfigHttpNotFound);
}

TEST_F(ConfigFetcherTest, RegisteredIdpSkipsWellKnownCheck) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdPRegistration);
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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

  // Asserts that we get success despite well-known failing, and that there are
  // no identity provider links, since they come from the well-known file.
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/true}},
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            EXPECT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            ASSERT_TRUE(result[0].metadata);
            EXPECT_TRUE(result[0].metadata->account_management_url.is_empty());
            EXPECT_TRUE(result[0].metadata->privacy_policy_url.is_empty());
            EXPECT_TRUE(result[0].metadata->terms_of_service_url.is_empty());
            loop.Quit();
          }));

  loop.Run();
}

// Tests that the identity provider's links from the well-known file are
// accepted if they are same-site, including cross-origin, with the config URL,
// and that omitted links do not log a console warning.
TEST_F(ConfigFetcherTest, IdpLinksSameSite) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
            well_known.account_management_url =
                "https://accounts.idp.example/account";
            well_known.privacy_policy_url = "https://idp.example/privacy";
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            ASSERT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            ASSERT_TRUE(result[0].metadata);
            EXPECT_EQ(result[0].metadata->account_management_url,
                      GURL("https://accounts.idp.example/account"));
            EXPECT_EQ(result[0].metadata->privacy_policy_url,
                      GURL("https://idp.example/privacy"));
            EXPECT_TRUE(result[0].metadata->terms_of_service_url.is_empty());
            loop.Quit();
          }));
  loop.Run();

  EXPECT_TRUE(
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages().empty());
}

// Tests that the identity provider's links from the well-known file are ignored
// with a console warning if they are not same-site with the config URL.
TEST_F(ConfigFetcherTest, IdpLinksNotSameSite) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
            well_known.account_management_url = "https://other.example/account";
            well_known.privacy_policy_url = "http://idp.example/privacy";
            well_known.terms_of_service_url = "javascript:void 0";
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            ASSERT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            ASSERT_TRUE(result[0].metadata);
            EXPECT_TRUE(result[0].metadata->account_management_url.is_empty());
            EXPECT_TRUE(result[0].metadata->privacy_policy_url.is_empty());
            EXPECT_TRUE(result[0].metadata->terms_of_service_url.is_empty());
            loop.Quit();
          }));
  loop.Run();

  EXPECT_THAT(
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages(),
      ElementsAre("The FedCM account_management_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL.",
                  "The FedCM privacy_policy_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL.",
                  "The FedCM terms_of_service_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL."));
}

// Tests that the identity provider's links from the well-known file are ignored
// with a console warning if they aren't HTTP(S), including privileged URLs and
// URLs whose inner origin is same-site with the config URL.
TEST_F(ConfigFetcherTest, IdpLinksNotHttp) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
            well_known.account_management_url = "chrome://settings";
            well_known.privacy_policy_url =
                "blob:https://idp.example/b9c1d0e8-3f2a-4c5d-9e7f";
            well_known.terms_of_service_url = "file:///etc/passwd";
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            ASSERT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            ASSERT_TRUE(result[0].metadata);
            EXPECT_TRUE(result[0].metadata->account_management_url.is_empty());
            EXPECT_TRUE(result[0].metadata->privacy_policy_url.is_empty());
            EXPECT_TRUE(result[0].metadata->terms_of_service_url.is_empty());
            loop.Quit();
          }));
  loop.Run();

  EXPECT_THAT(
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages(),
      ElementsAre("The FedCM account_management_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL.",
                  "The FedCM privacy_policy_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL.",
                  "The FedCM terms_of_service_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL."));
}

// Tests that the identity provider's relative links from the well-known file
// are ignored with a console warning, since only absolute URLs are accepted.
TEST_F(ConfigFetcherTest, IdpLinksRelative) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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
            well_known.account_management_url = "/account";
            well_known.privacy_policy_url = "privacy";
            well_known.terms_of_service_url = "//idp.example/terms";
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            ASSERT_EQ(result.size(), 1ul);
            EXPECT_FALSE(result[0].error);
            ASSERT_TRUE(result[0].metadata);
            EXPECT_TRUE(result[0].metadata->account_management_url.is_empty());
            EXPECT_TRUE(result[0].metadata->privacy_policy_url.is_empty());
            EXPECT_TRUE(result[0].metadata->terms_of_service_url.is_empty());
            loop.Quit();
          }));
  loop.Run();

  EXPECT_THAT(
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages(),
      ElementsAre("The FedCM account_management_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL.",
                  "The FedCM privacy_policy_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL.",
                  "The FedCM terms_of_service_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL."));
}

// Tests that when the well-known check is skipped, the identity provider's
// links are kept from a well-known file that is otherwise invalid, e.g. because
// it has no provider URLs.
TEST_F(ConfigFetcherTest, IdpLinksFromInvalidWellKnownWhenCheckSkipped) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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

  // The well-known file has a link but no provider_urls, so it's invalid.
  EXPECT_CALL(*network_manager, FetchWellKnown)
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.privacy_policy_url = "https://idp.example/privacy";
            std::move(callback).Run(
                {ParseStatus::kInvalidResponseError, net::HTTP_OK}, well_known);
          }));

  base::RunLoop loop;
  fetcher.Start({{GURL("https://idp.example/fedcm.json"),
                  /*force_skip_well_known_enforcement=*/true}},
                /*icon_ideal_size=*/0,
                /*icon_minimum_size=*/0,
                base::BindLambdaForTesting(
                    [&loop](std::vector<ConfigFetcher::FetchResult> result) {
                      ASSERT_EQ(result.size(), 1ul);
                      EXPECT_FALSE(result[0].error);
                      ASSERT_TRUE(result[0].metadata);
                      EXPECT_EQ(result[0].metadata->privacy_policy_url,
                                GURL("https://idp.example/privacy"));
                      loop.Quit();
                    }));
  loop.Run();
}

// Tests that a link that isn't same-site with the config URL logs a console
// warning even if the identity provider fails validation, like the config
// file's warnings.
TEST_F(ConfigFetcherTest, IdpLinksWarningLoggedWhenValidationFails) {
  auto network_manager =
      std::make_unique<StrictMock<MockIdpNetworkRequestManager>>();
  ConfigFetcher fetcher(*main_rfh(), network_manager.get());

  EXPECT_CALL(*network_manager, FetchConfig)
      .WillOnce(WithArg<3>(
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

  // The well-known file doesn't list the config URL, so validation fails.
  EXPECT_CALL(*network_manager, FetchWellKnown)
      .WillOnce(WithArg<1>(
          [](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.provider_urls = {GURL("https://idp.example/other.json")};
            well_known.account_management_url = "https://other.example/account";
            std::move(callback).Run({ParseStatus::kSuccess, net::HTTP_OK},
                                    well_known);
          }));

  base::RunLoop loop;
  fetcher.Start(
      {{GURL("https://idp.example/fedcm.json"),
        /*force_skip_well_known_enforcement=*/false}},
      /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindLambdaForTesting(
          [&loop](std::vector<ConfigFetcher::FetchResult> result) {
            ASSERT_EQ(result.size(), 1ul);
            ASSERT_TRUE(result[0].error);
            EXPECT_EQ(
                result[0].error->result,
                blink::mojom::FederatedRequestResult::kConfigNotInWellKnown);
            loop.Quit();
          }));
  loop.Run();

  EXPECT_THAT(
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages(),
      ElementsAre("The FedCM account_management_url in the well-known file is "
                  "ignored because it isn't an absolute HTTP(S) URL that is "
                  "same-site with the config URL."));
}

}  // namespace content::webid
