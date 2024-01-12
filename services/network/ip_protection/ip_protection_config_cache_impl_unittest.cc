// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_config_cache_impl.h"

#include <deque>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager_impl.h"
#include "services/network/public/mojom/network_context.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

constexpr char kAreAuthTokensAvailableHistogram[] =
    "NetworkService.IpProtection.AreAuthTokensAvailable";
constexpr char kEmptyTokenCacheHistogram[] =
    "NetworkService.IpProtection.EmptyTokenCache";

class MockIpProtectionTokenCacheManager : public IpProtectionTokenCacheManager {
 public:
  bool IsAuthTokenAvailable() override { return auth_token_.has_value(); }

  void InvalidateTryAgainAfterTime() override {}

  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken()
      override {
    return std::move(auth_token_);
  }

  void SetAuthToken(
      absl::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token) {
    auth_token_ = std::move(auth_token);
  }

 private:
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token_;
};

class MockIpProtectionProxyListManager : public IpProtectionProxyListManager {
 public:
  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  const std::vector<std::vector<std::string>>& ProxyList() override {
    return *proxy_list_;
  }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<std::vector<std::string>> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list) {
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

 private:
  absl::optional<std::vector<std::vector<std::string>>> proxy_list_;
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
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 1);
  histogram_tester_.ExpectBucketCount(kAreAuthTokensAvailableHistogram, true,
                                      1);
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
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 1);
  histogram_tester_.ExpectBucketCount(kAreAuthTokensAvailableHistogram, true,
                                      1);
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
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 1);
  histogram_tester_.ExpectBucketCount(kAreAuthTokensAvailableHistogram, false,
                                      1);
  histogram_tester_.ExpectTotalCount(kEmptyTokenCacheHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      kEmptyTokenCacheHistogram, mojom::IpProtectionProxyLayer::kProxyB, 1);
}

TEST_F(IpProtectionConfigCacheImplTest,
       AreAuthTokensAvailable_NoProxiesConfigured) {
  ASSERT_FALSE(ipp_config_cache_->AreAuthTokensAvailable());
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 1);
  histogram_tester_.ExpectBucketCount(kAreAuthTokensAvailableHistogram, false,
                                      1);
}

// Proxy list manager returns currently cached proxy hostnames.
TEST_F(IpProtectionConfigCacheImplTest, GetProxyListFromManager) {
  std::string proxy = "a-proxy";
  auto ip_protection_proxy_chain =
      net::ProxyChain(net::ProxyServer::FromSchemeHostAndPort(
                          net::ProxyServer::SCHEME_HTTPS, proxy, absl::nullopt))
          .ForIpProtection();
  const std::vector<net::ProxyChain> proxy_chain_list = {
      std::move(ip_protection_proxy_chain)};
  auto ipp_proxy_list_manager_ =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager_->SetProxyList({{proxy}});
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager_));

  ASSERT_TRUE(ipp_config_cache_->IsProxyListAvailable());
  EXPECT_EQ(ipp_config_cache_->GetProxyChainList(), proxy_chain_list);
}

// Token cache manager supports both hostnames and host:port for proxy servers
// in list.
TEST_F(IpProtectionConfigCacheImplTest, GetProxyChainList) {
  const struct {
    const net::ProxyServer::Scheme scheme;
    const char* const host;
    const std::optional<uint16_t> port;
    const char* const proxy;
  } tests[] = {
      {net::ProxyServer::SCHEME_HTTPS, "a-proxy", 443, "a-proxy:443"},
      {net::ProxyServer::SCHEME_HTTPS, "b-proxy", 443, "b-proxy:443"},

      // No ports.
      {net::ProxyServer::SCHEME_HTTPS, "a-proxy", absl::nullopt, "a-proxy"},
      {net::ProxyServer::SCHEME_HTTPS, "b-proxy", absl::nullopt, "b-proxy"},

      // Non-standard port.
      {net::ProxyServer::SCHEME_HTTPS, "a-proxy", 10, "a-proxy:10"},
      {net::ProxyServer::SCHEME_HTTPS, "b-proxy", 0, "b-proxy:0"},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    auto ip_protection_proxy_chain =
        net::ProxyChain(net::ProxyServer::FromSchemeHostAndPort(
                            tests[i].scheme, tests[i].host, tests[i].port))
            .ForIpProtection();
    const std::vector<net::ProxyChain> proxy_chain_list = {
        std::move(ip_protection_proxy_chain)};
    auto ipp_proxy_list_manager_ =
        std::make_unique<MockIpProtectionProxyListManager>();
    ipp_proxy_list_manager_->SetProxyList({{tests[i].proxy}});
    ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
        std::move(ipp_proxy_list_manager_));

    ASSERT_TRUE(ipp_config_cache_->IsProxyListAvailable());
    EXPECT_EQ(ipp_config_cache_->GetProxyChainList(), proxy_chain_list);
  }
}

}  // namespace network
