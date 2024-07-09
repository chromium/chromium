// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_config_cache_impl.h"

#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_change_notifier.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager_impl.h"
#include "services/network/public/mojom/network_context.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

constexpr char kEmptyTokenCacheHistogram[] =
    "NetworkService.IpProtection.EmptyTokenCache";

class MockIpProtectionTokenCacheManager : public IpProtectionTokenCacheManager {
 public:
  bool IsAuthTokenAvailable() override { return auth_token_.has_value(); }

  void InvalidateTryAgainAfterTime() override {}

  std::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken()
      override {
    return std::move(auth_token_);
  }

  void SetAuthToken(
      std::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token) {
    auth_token_ = std::move(auth_token);
  }

 private:
  std::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token_;
};

class MockIpProtectionProxyListManager : public IpProtectionProxyListManager {
 public:
  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  const std::vector<net::ProxyChain>& ProxyList() override {
    return *proxy_list_;
  }

  const std::string& GeoId() override { return geo_id_; }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<net::ProxyChain> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list) {
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

 private:
  std::optional<std::vector<net::ProxyChain>> proxy_list_;
  std::string geo_id_;
  base::OnceClosure on_force_refresh_proxy_list_;
};

}  // namespace

class IpProtectionConfigCacheImplTest : public testing::Test {
 protected:
  IpProtectionConfigCacheImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        ipp_config_cache_(
            std::make_unique<IpProtectionConfigCacheImpl>(mojo::NullRemote())) {
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

  base::HistogramTester histogram_tester_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // The IpProtectionConfigCache being tested.
  std::unique_ptr<IpProtectionConfigCacheImpl> ipp_config_cache_;
};

// Token cache manager returns available token for proxyA.
TEST_F(IpProtectionConfigCacheImplTest, GetAuthTokenFromManagerForProxyA) {
  auto exp_token = mojom::BlindSignedAuthToken::New();
  exp_token->token = "a-token";
  auto ipp_token_cache_manager_ =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager_->SetAuthToken(std::move(exp_token));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer::kProxyA,
      std::move(ipp_token_cache_manager_));

  ASSERT_TRUE(ipp_config_cache_->AreAuthTokensAvailable());
  ASSERT_FALSE(
      ipp_config_cache_->GetAuthToken(1).has_value());  // ProxyB has no tokens.
  ASSERT_TRUE(ipp_config_cache_->GetAuthToken(0));
}

// Token cache manager returns available token for proxyB.
TEST_F(IpProtectionConfigCacheImplTest, GetAuthTokenFromManagerForProxyB) {
  auto exp_token = mojom::BlindSignedAuthToken::New();
  exp_token->token = "b-token";
  auto ipp_token_cache_manager_ =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager_->SetAuthToken(std::move(exp_token));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer::kProxyB,
      std::move(ipp_token_cache_manager_));

  ASSERT_TRUE(ipp_config_cache_->AreAuthTokensAvailable());
  ASSERT_FALSE(
      ipp_config_cache_->GetAuthToken(0).has_value());  // ProxyA has no tokens.
  ASSERT_TRUE(ipp_config_cache_->GetAuthToken(1));
}

TEST_F(IpProtectionConfigCacheImplTest,
       AreAuthTokensAvailable_OneTokenCacheIsEmpty) {
  auto exp_token = mojom::BlindSignedAuthToken::New();
  exp_token->token = "a-token";
  auto ipp_token_cache_manager =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager->SetAuthToken(std::move(exp_token));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer::kProxyA,
      std::move(ipp_token_cache_manager));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer::kProxyB,
      std::make_unique<MockIpProtectionTokenCacheManager>());

  ASSERT_FALSE(ipp_config_cache_->AreAuthTokensAvailable());
  histogram_tester_.ExpectTotalCount(kEmptyTokenCacheHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kEmptyTokenCacheHistogram, mojom::IpProtectionProxyLayer::kProxyB, 1);
}

TEST_F(IpProtectionConfigCacheImplTest,
       AreAuthTokensAvailable_NoProxiesConfigured) {
  ASSERT_FALSE(ipp_config_cache_->AreAuthTokensAvailable());
}

// Proxy list manager returns currently cached proxy hostnames.
TEST_F(IpProtectionConfigCacheImplTest, GetProxyListFromManager) {
  std::string proxy = "a-proxy";
  auto ip_protection_proxy_chain =
      net::ProxyChain::ForIpProtection({net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, proxy, std::nullopt)});
  const std::vector<net::ProxyChain> proxy_chain_list = {
      std::move(ip_protection_proxy_chain)};
  auto ipp_proxy_list_manager_ =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager_->SetProxyList({MakeChain({proxy})});
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager_));

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

  ipp_config_cache_ =
      std::make_unique<IpProtectionConfigCacheImpl>(mojo::NullRemote());

  auto ipp_proxy_list_manager_ =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager_->SetProxyList({MakeChain({"a-proxy1", "b-proxy1"}),
                                         MakeChain({"a-proxy2", "b-proxy2"})});
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager_));

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

  ipp_config_cache_ =
      std::make_unique<IpProtectionConfigCacheImpl>(mojo::NullRemote());

  auto ipp_proxy_list_manager_ =
      std::make_unique<MockIpProtectionProxyListManager>();
  bool refresh_requested = false;
  ipp_proxy_list_manager_->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { refresh_requested = true; }));
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager_));

  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(refresh_requested);
}

}  // namespace network
