// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_config_cache_impl.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/ip_protection/ip_protection_geo_utils.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

constexpr char kEmptyTokenCacheHistogram[] =
    "NetworkService.IpProtection.EmptyTokenCache";

constexpr char kMountainViewGeoId[] = "US,US-CA,MOUNTAIN VIEW";
constexpr char kSunnyvaleGeoId[] = "US,US-CA,SUNNYVALE";

constexpr bool kEnableTokenCacheByGeo = true;
constexpr bool kDisableTokenCacheByGeo = false;

class MockIpProtectionTokenCacheManager : public IpProtectionTokenCacheManager {
 public:
  bool IsAuthTokenAvailable() override {
    return IsAuthTokenAvailable(current_geo_id_);
  }

  bool IsAuthTokenAvailable(const std::string& geo_id) override {
    return auth_tokens_.contains(geo_id);
  }

  void InvalidateTryAgainAfterTime() override {}

  std::string CurrentGeo() const override { return current_geo_id_; }

  void SetCurrentGeo(const std::string& geo_id) override {
    current_geo_id_ = geo_id;
  }

  std::optional<BlindSignedAuthToken> GetAuthToken() override {
    return GetAuthToken(current_geo_id_);
  }

  std::optional<BlindSignedAuthToken> GetAuthToken(
      const std::string& geo_id) override {
    if (!auth_tokens_.contains(geo_id)) {
      return std::nullopt;
    }

    return auth_tokens_.extract(geo_id).mapped();
  }

  void SetAuthToken(BlindSignedAuthToken auth_token) {
    auth_tokens_[network::GetGeoIdFromGeoHint(auth_token.geo_hint)] =
        auth_token;
  }

 private:
  std::map<std::string, BlindSignedAuthToken> auth_tokens_;
  std::optional<BlindSignedAuthToken> auth_token_;
  std::string current_geo_id_;
};

class MockIpProtectionProxyListManager : public IpProtectionProxyListManager {
 public:
  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  const std::vector<net::ProxyChain>& ProxyList() override {
    return *proxy_list_;
  }

  const std::string& CurrentGeo() override { return geo_id_; }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      if (!geo_id_to_change_on_refresh_.empty()) {
        geo_id_ = geo_id_to_change_on_refresh_;
      }
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<net::ProxyChain> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  // Set the geo id returned from `CurrentGeo()`.
  void RefreshProxyListForGeoChange() override {
    if (on_force_refresh_proxy_list_) {
      if (!geo_id_to_change_on_refresh_.empty()) {
        geo_id_ = geo_id_to_change_on_refresh_;
      }
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list,
      std::string geo_id = "") {
    geo_id_to_change_on_refresh_ = geo_id;
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

  void SetCurrentGeo(const std::string& geo_id) { geo_id_ = geo_id; }

 private:
  std::optional<std::vector<net::ProxyChain>> proxy_list_;
  std::string geo_id_;
  std::string geo_id_to_change_on_refresh_;
  base::OnceClosure on_force_refresh_proxy_list_;
};

class IpProtectionConfigCacheImplTest : public testing::Test {
 protected:
  IpProtectionConfigCacheImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    SetTokenCachingByGeoParam(kEnableTokenCacheByGeo);
    ipp_config_cache_ = std::make_unique<IpProtectionConfigCacheImpl>(nullptr);
  }

  // Shortcut to create a ProxyChain from hostnames.
  net::ProxyChain MakeChain(std::vector<std::string> hostnames) {
    std::vector<net::ProxyServer> servers;
    for (auto& hostname : hostnames) {
      servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
    }
    return net::ProxyChain::ForIpProtection(servers);
  }

  void SetTokenCachingByGeoParam(bool should_enable_feature) {
    // Set token caching by geo param value.
    scoped_feature_list_.Reset();
    std::map<std::string, std::string> parameters;
    parameters[net::features::kIpPrivacyCacheTokensByGeo.name] =
        should_enable_feature ? "true" : "false";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy, std::move(parameters));
  }

  base::HistogramTester histogram_tester_;

  // TODO(abhipatel): Reorder scoped_feature_list_ to be
  // initialized before task_environment_
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // The IpProtectionConfigCache being tested.
  std::unique_ptr<IpProtectionConfigCacheImpl> ipp_config_cache_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(IpProtectionConfigCacheImplTest,
       AreAuthTokensAvailable_NoProxiesConfigured) {
  // A proxy list is available. This should ensure that the only reason tokens
  // are not available is due to a lack of token cache managers.
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo(kMountainViewGeoId);

  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  ASSERT_FALSE(ipp_config_cache_->AreAuthTokensAvailable());
}

TEST_F(IpProtectionConfigCacheImplTest,
       AuthTokensNotAvailableIfProxyListIsNotAvailable) {
  // `IpProtectionProxyListManager` has not been set up. This means even if
  // tokens are available, `AreAuthTokensAvailable()` should return false.
  BlindSignedAuthToken exp_token;
  exp_token.token = "a-token";
  exp_token.geo_hint =
      network::GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value();
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetAuthToken(std::move(exp_token));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  ASSERT_FALSE(ipp_config_cache_->AreAuthTokensAvailable());
  // Neither calls will return a token since there is no proxy list available.
  ASSERT_FALSE(ipp_config_cache_->GetAuthToken(0).has_value());
  ASSERT_FALSE(ipp_config_cache_->GetAuthToken(1).has_value());
}

// Token cache manager returns available token for proxyA.
TEST_F(IpProtectionConfigCacheImplTest, GetAuthTokenFromManagerForProxyA) {
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo(kMountainViewGeoId);

  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "a-token",
      .geo_hint =
          network::GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value()});

  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  ASSERT_TRUE(ipp_config_cache_->AreAuthTokensAvailable());
  ASSERT_FALSE(
      ipp_config_cache_->GetAuthToken(1).has_value());  // ProxyB has no tokens.
  ASSERT_TRUE(ipp_config_cache_->GetAuthToken(0));
}

// Token cache manager returns available token for proxyB.
TEST_F(IpProtectionConfigCacheImplTest, GetAuthTokenFromManagerForProxyB) {
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo(kMountainViewGeoId);

  BlindSignedAuthToken exp_token;
  exp_token.token = "b-token";
  exp_token.geo_hint =
      network::GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value();
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetAuthToken(std::move(exp_token));

  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyB, std::move(ipp_token_cache_manager));

  ASSERT_TRUE(ipp_config_cache_->AreAuthTokensAvailable());
  ASSERT_FALSE(
      ipp_config_cache_->GetAuthToken(0).has_value());  // ProxyA has no tokens.
  ASSERT_TRUE(ipp_config_cache_->GetAuthToken(1));
}

// If a required token is missing from one of the token caches, the availability
// is set to false.
TEST_F(IpProtectionConfigCacheImplTest,
       AreAuthTokensAvailable_OneTokenCacheIsEmpty) {
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo(kMountainViewGeoId);

  BlindSignedAuthToken exp_token;
  exp_token.token = "a-token";
  exp_token.geo_hint =
      network::GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value();
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetAuthToken(std::move(exp_token));

  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyB,
      std::make_unique<MockIpProtectionTokenCacheManager>());

  ASSERT_FALSE(ipp_config_cache_->AreAuthTokensAvailable());
  histogram_tester_.ExpectTotalCount(kEmptyTokenCacheHistogram, 1);
  histogram_tester_.ExpectBucketCount(kEmptyTokenCacheHistogram,
                                      IpProtectionProxyLayer::kProxyB, 1);
}

// GetAuthToken for where proxy list manager's geo is different than the current
// geo of the config cache.
TEST_F(IpProtectionConfigCacheImplTest, GetAuthTokenForOldGeo) {
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo(kMountainViewGeoId);

  // The token cache manager will contain a token from the "old" mountain view
  // geo and a new sunnyvale geo.
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "a-token",
      .geo_hint =
          network::GetGeoHintFromGeoIdForTesting(kMountainViewGeoId).value()});
  ipp_token_cache_manager->SetAuthToken(BlindSignedAuthToken{
      .token = "a-token",
      .geo_hint =
          network::GetGeoHintFromGeoIdForTesting(kSunnyvaleGeoId).value()});

  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  // The following calls will be based on the proxy list manager's geo (Mountain
  // View).
  ASSERT_TRUE(ipp_config_cache_->AreAuthTokensAvailable());
  std::optional<BlindSignedAuthToken> token =
      ipp_config_cache_->GetAuthToken(0);
  ASSERT_TRUE(token);
  ASSERT_EQ(token->geo_hint,
            network::GetGeoHintFromGeoIdForTesting(kMountainViewGeoId));
}

// Proxy list manager returns currently cached proxy hostnames.
TEST_F(IpProtectionConfigCacheImplTest, GetProxyListFromManager) {
  std::string proxy = "a-proxy";
  auto ip_protection_proxy_chain =
      net::ProxyChain::ForIpProtection({net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, proxy, std::nullopt)});
  const std::vector<net::ProxyChain> proxy_chain_list = {
      std::move(ip_protection_proxy_chain)};
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({proxy})});
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  ASSERT_TRUE(ipp_config_cache_->IsProxyListAvailable());
  EXPECT_EQ(ipp_config_cache_->GetProxyChainList(), proxy_chain_list);
}

// When QUIC proxies are enabled, the proxy list has both QUIC and HTTPS
// proxies, and falls back properly when a QUIC proxy fails.
TEST_F(IpProtectionConfigCacheImplTest, GetProxyListFromManagerWithQuic) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyUseQuicProxies.name] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier =
      net::NetworkChangeNotifier::CreateMockIfNeeded();

  ipp_config_cache_ = std::make_unique<IpProtectionConfigCacheImpl>(nullptr);

  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy1", "b-proxy1"}),
                                        MakeChain({"a-proxy2", "b-proxy2"})});
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  const std::vector<net::ProxyChain> proxy_chain_list_with_quic = {
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "a-proxy1", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "b-proxy1", std::nullopt),
      }),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "a-proxy1", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "b-proxy1", std::nullopt),
      }),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "a-proxy2", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_QUIC,
                                                  "b-proxy2", std::nullopt),
      })};

  const std::vector<net::ProxyChain> proxy_chain_list_without_quic = {
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "a-proxy1", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "b-proxy1", std::nullopt),
      }),
      net::ProxyChain::ForIpProtection({
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "a-proxy2", std::nullopt),
          net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "b-proxy2", std::nullopt),
      })};
  ASSERT_TRUE(ipp_config_cache_->IsProxyListAvailable());
  EXPECT_EQ(ipp_config_cache_->GetProxyChainList(), proxy_chain_list_with_quic);

  ipp_config_cache_->QuicProxiesFailed();

  EXPECT_EQ(ipp_config_cache_->GetProxyChainList(),
            proxy_chain_list_without_quic);

  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ipp_config_cache_->GetProxyChainList(), proxy_chain_list_with_quic);
}

// When the network changes, a new proxy list is requested.
TEST_F(IpProtectionConfigCacheImplTest, RefreshProxyListOnNetworkChange) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyUseQuicProxies.name] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier =
      net::NetworkChangeNotifier::CreateMockIfNeeded();

  ipp_config_cache_ = std::make_unique<IpProtectionConfigCacheImpl>(nullptr);

  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  bool refresh_requested = false;
  ipp_proxy_list_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(refresh_requested);
}

// Simulates a geo change detected in the IppProxyListManager.
TEST_F(IpProtectionConfigCacheImplTest,
       GeoChangeObservedInIppProxyListManager) {
  // Set up IppProxyListManager to have a "new" geo.
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  bool refresh_requested = false;
  std::string new_geo_signal = "US,US-NY,NEW YORK CITY";
  ipp_proxy_list_manager->SetCurrentGeo(new_geo_signal);
  ipp_proxy_list_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});

  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  // Set up `IppTokenCacheManager` to have an "old geo"
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetCurrentGeo("US,US-MA,BOSTON");
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  // Simulate that the new geo signal in the Proxy List Manager resulted in a
  // call to observe a geo change.
  ipp_config_cache_->GeoObserved(new_geo_signal);

  EXPECT_EQ(ipp_config_cache_->GetIpProtectionProxyListManagerForTesting()
                ->CurrentGeo(),
            new_geo_signal);
  EXPECT_EQ(ipp_config_cache_
                ->GetIpProtectionTokenCacheManagerForTesting(
                    IpProtectionProxyLayer::kProxyA)
                ->CurrentGeo(),
            new_geo_signal);

  // Since the new geo matches the geo of the proxy list manager, it should not
  // refresh the proxy list.
  EXPECT_FALSE(refresh_requested);
}

// Current geo for all token cache managers should reflect this empty geo.
TEST_F(IpProtectionConfigCacheImplTest,
       GeoChangeObservedEmptyGeoIdInProxyListManager) {
  // Set up IppProxyListManager to have a "new" geo.
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  bool refresh_requested = false;
  std::string empty_geo_signal = "";
  ipp_proxy_list_manager->SetCurrentGeo(empty_geo_signal);
  ipp_proxy_list_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  // Set up `IppTokenCacheManager` to have an "old geo"
  std::string boston_geo_id = "US,US-MA,BOSTON";
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetCurrentGeo(boston_geo_id);
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  // Simulate the empty geo change in the proxy list manager caused a call such
  // as this.
  ipp_config_cache_->GeoObserved(empty_geo_signal);

  EXPECT_EQ(ipp_config_cache_->GetIpProtectionProxyListManagerForTesting()
                ->CurrentGeo(),
            empty_geo_signal);
  EXPECT_EQ(ipp_config_cache_
                ->GetIpProtectionTokenCacheManagerForTesting(
                    IpProtectionProxyLayer::kProxyA)
                ->CurrentGeo(),
            empty_geo_signal);

  // Since the new geo matches the geo of the proxy list manager, it should not
  // refresh the proxy list.
  EXPECT_FALSE(refresh_requested);
}

// When token caching by geo is disabled, `GeoObserved` has no impact.
TEST_F(IpProtectionConfigCacheImplTest,
       GeoObservedTokenCachingByGeoDisabledNoImpact) {
  SetTokenCachingByGeoParam(kDisableTokenCacheByGeo);

  // Reinitialize the config cache b/c the feature value needs to be set to
  // false.
  ipp_config_cache_ = std::make_unique<IpProtectionConfigCacheImpl>(nullptr);

  // Old geo used to set current geo in both the proxy list manager and token
  // cache manager.
  std::string old_geo_id = "US,US-CA,MOUNTAIN VIEW";

  // Set up `IppTokenCacheManager` to have an "old geo"
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetCurrentGeo(old_geo_id);
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  // Set up IppProxyListManager to have a "old" geo.
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  bool refresh_requested = false;
  ipp_proxy_list_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }),
      old_geo_id);
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo(old_geo_id);
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  // Simulate a new geo signal that is non-empty. In theory this should cause
  // both the token cache manager and proxy list manager to set the new geo. But
  // the disabled experiment means this is short circuited.
  ipp_config_cache_->GeoObserved("US,US-CA,SUNNYVALE");

  // Both should still contain the old geo id.
  EXPECT_EQ(ipp_config_cache_
                ->GetIpProtectionTokenCacheManagerForTesting(
                    IpProtectionProxyLayer::kProxyA)
                ->CurrentGeo(),
            old_geo_id);

  EXPECT_EQ(ipp_config_cache_->GetIpProtectionProxyListManagerForTesting()
                ->CurrentGeo(),
            old_geo_id);
}

// Simulates a geo change detected in the IppTokenCacheManager.
TEST_F(IpProtectionConfigCacheImplTest,
       GeoChangeObservedInIppTokenCacheManager) {
  // Set up `IppTokenCacheManager` to have an "new geo"
  std::string new_geo_signal = "US,US-MA,BOSTON";
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetCurrentGeo(new_geo_signal);
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer::kProxyA, std::move(ipp_token_cache_manager));

  // Set up IppProxyListManager to have a "old" geo.
  auto ipp_proxy_list_manager =
      std::make_unique<MockIpProtectionProxyListManager>();
  bool refresh_requested = false;
  ipp_proxy_list_manager->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }),
      new_geo_signal);
  ipp_proxy_list_manager->SetProxyList({MakeChain({"a-proxy"})});
  ipp_proxy_list_manager->SetCurrentGeo("US,US-NY,NEW YORK CITY");
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager));

  // Simulate that the new geo signal in the token cache manager resulted in a
  // call to observe a geo change.
  ipp_config_cache_->GeoObserved(new_geo_signal);

  EXPECT_EQ(ipp_config_cache_
                ->GetIpProtectionTokenCacheManagerForTesting(
                    IpProtectionProxyLayer::kProxyA)
                ->CurrentGeo(),
            new_geo_signal);

  EXPECT_EQ(ipp_config_cache_->GetIpProtectionProxyListManagerForTesting()
                ->CurrentGeo(),
            new_geo_signal);

  // Since the new geo matches the geo of the proxy list manager, it should not
  // refresh the proxy list.
  EXPECT_TRUE(refresh_requested);
}

}  // namespace
}  // namespace network
