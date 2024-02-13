// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_proxy_delegate.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/ip_protection/ip_protection_config_cache_impl.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection/ip_protection_token_cache_manager.h"
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kHttpsUrl[] = "https://example.com";
constexpr char kHttpUrl[] = "http://example.com";
constexpr char kLocalhost[] = "http://localhost";

constexpr char kEligibilityHistogram[] =
    "NetworkService.IpProtection.RequestIsEligibleForProtection";
constexpr char kAreAuthTokensAvailableHistogram[] =
    "NetworkService.IpProtection.AreAuthTokensAvailable";
constexpr char kIsProxyListAvailableHistogram[] =
    "NetworkService.IpProtection.IsProxyListAvailable";
constexpr char kAvailabilityHistogram[] =
    "NetworkService.IpProtection.ProtectionIsAvailableForRequest";

class MockIpProtectionConfigCache : public IpProtectionConfigCache {
 public:
  bool AreAuthTokensAvailable() override { return auth_token_.has_value(); }
  void InvalidateTryAgainAfterTime() override {
    if (on_invalidate_try_again_after_time_) {
      std::move(on_invalidate_try_again_after_time_).Run();
    }
  }
  std::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken(
      size_t chain_index) override {
    return std::move(auth_token_);
  }

  // Set the auth token that will be returned from the next call to
  // `GetAuthToken()`.
  void SetNextAuthToken(
      std::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token) {
    auth_token_ = std::move(auth_token);
  }

  void SetIpProtectionProxyListManagerForTesting(
      std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager)
      override {
    NOTREACHED_NORETURN();
  }

  IpProtectionTokenCacheManager* GetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer proxy_layer) override {
    NOTREACHED_NORETURN();
  }

  void SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer proxy_layer,
      std::unique_ptr<IpProtectionTokenCacheManager> ipp_token_cache_manager)
      override {
    NOTREACHED_NORETURN();
  }

  std::vector<net::ProxyChain> GetProxyChainList() override {
    return *proxy_list_;
  }

  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

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

  void SetOnInvalidateTryAgainAfterTime(
      base::OnceClosure on_invalidate_try_again_after_time) {
    on_invalidate_try_again_after_time_ =
        std::move(on_invalidate_try_again_after_time);
  }

 private:
  std::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token_;
  std::optional<std::vector<net::ProxyChain>> proxy_list_;
  std::vector<net::ProxyChain> proxy_chain_list_;
  base::OnceClosure on_force_refresh_proxy_list_;
  base::OnceClosure on_invalidate_try_again_after_time_;
};

}  // namespace

MATCHER_P2(Contain,
           expected_name,
           expected_value,
           std::string("headers ") + (negation ? "don't " : "") + "contain '" +
               expected_name + ": " + expected_value + "'") {
  std::string value;
  return arg.GetHeader(expected_name, &value) && value == expected_value;
}

struct HeadersReceived {
  net::ProxyChain proxy_chain;
  uint64_t chain_index;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
};

class TestCustomProxyConnectionObserver
    : public mojom::CustomProxyConnectionObserver {
 public:
  TestCustomProxyConnectionObserver() = default;
  ~TestCustomProxyConnectionObserver() override = default;

  const std::optional<std::pair<net::ProxyChain, int>>& FallbackArgs() const {
    return fallback_;
  }

  const std::optional<HeadersReceived>& HeadersReceivedArgs() const {
    return headers_received_;
  }

  // mojom::CustomProxyConnectionObserver:
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override {
    fallback_ = std::make_pair(bad_chain, net_error);
  }
  void OnTunnelHeadersReceived(const net::ProxyChain& proxy_chain,
                               uint64_t chain_index,
                               const scoped_refptr<net::HttpResponseHeaders>&
                                   response_headers) override {
    headers_received_ =
        HeadersReceived{proxy_chain, chain_index, response_headers};
  }

 private:
  std::optional<std::pair<net::ProxyChain, int>> fallback_;
  std::optional<HeadersReceived> headers_received_;
};

class IpProtectionProxyDelegateTest : public testing::Test {
 public:
  IpProtectionProxyDelegateTest() = default;

  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
    scoped_feature_list_.InitWithFeatures(
        {net::features::kEnableIpProtectionProxy,
         network::features::kMaskedDomainList},
        {});
  }

 protected:
  std::unique_ptr<IpProtectionProxyDelegate> CreateDelegate(
      NetworkServiceProxyAllowList* network_service_proxy_allow_list,
      std::unique_ptr<IpProtectionConfigCache> ipp_config_cache) {
    return std::make_unique<IpProtectionProxyDelegate>(
        network_service_proxy_allow_list, std::move(ipp_config_cache),
        /*is_ip_protection_enabled=*/true);
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Shortcut to create a ProxyChain from hostnames.
  net::ProxyChain MakeChain(std::vector<std::string> hostnames,
                            int chain_id = 0) {
    std::vector<net::ProxyServer> servers;
    for (auto& hostname : hostnames) {
      servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
    }
    return net::ProxyChain::ForIpProtection(servers, chain_id);
  }

  mojom::BlindSignedAuthTokenPtr MakeAuthToken(std::string content) {
    auto token = mojom::BlindSignedAuthToken::New();
    token->token = std::move(content);
    return token;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<net::URLRequestContext> context_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(IpProtectionProxyDelegateTest, AddsTokenToTunnelRequest) {
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(/*first_party_map=*/{});
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::HttpRequestHeaders headers;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", std::nullopt)});
  delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain, /*chain_index=*/0,
                                  &headers);

  EXPECT_THAT(headers, Contain("Authorization", "Bearer: a-token"));
}

TEST_F(IpProtectionProxyDelegateTest, AddsPskToTunnelRequest) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyProxyBPsk.name] = "seekrit";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(/*first_party_map=*/{});
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetProxyList({MakeChain({"proxya", "proxyb"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::HttpRequestHeaders headers;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", std::nullopt)});
  delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain, /*chain_index=*/0,
                                  &headers);
  EXPECT_THAT(headers, testing::Not(Contain("Proxy-Authorization",
                                            "Preshared seekrit")));

  delegate->OnBeforeTunnelRequest(ip_protection_proxy_chain, /*chain_index=*/1,
                                  &headers);
  EXPECT_THAT(headers, Contain("Proxy-Authorization", "Preshared seekrit"));
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyDeprioritizesBadProxies) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList(
      {MakeChain({"proxya"}), MakeChain({"backup"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info = retry_map[net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt)})];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::Days(2);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", std::move(retry_map), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyChain(
      net::ProxyChain::ForIpProtection({net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, "backup", std::nullopt)}));
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyAllProxiesBad) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"proxya"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info = retry_map[net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", std::nullopt)})];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::Days(2);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", std::move(retry_map), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyNetworkServiceProxyAllowListMatch) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList(
      {MakeChain({"ippro-1", "ippro-2"}), MakeChain({"ippro-2", "ippro-2"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  // Verify that the IP Protection proxy list is correctly merged with the
  // existing proxy list.
  result.UsePacString("PROXY bar; DIRECT; PROXY weird");
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));

  const net::ProxyServer kProxyServer1{net::ProxyServer::SCHEME_HTTPS,
                                       net::HostPortPair("ippro-1", 443)};
  const net::ProxyServer kProxyServer2{net::ProxyServer::SCHEME_HTTPS,
                                       net::HostPortPair("ippro-2", 443)};
  const net::ProxyChain kIpProtectionChain1 =
      net::ProxyChain::ForIpProtection({kProxyServer1, kProxyServer2});
  const net::ProxyChain kIpProtectionChain2 =
      net::ProxyChain::ForIpProtection({kProxyServer2, kProxyServer2});

  expected_proxy_list.AddProxyChain(std::move(kIpProtectionChain1));
  expected_proxy_list.AddProxyChain(std::move(kIpProtectionChain2));
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY weird"));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_FALSE(result.is_for_ip_protection());

  // After a fallback, the first IP Protection proxy chain should be used.
  EXPECT_TRUE(result.Fallback(net::ERR_PROXY_CONNECTION_FAILED,
                              net::NetLogWithSource()));
  EXPECT_TRUE(result.is_for_ip_protection());

  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyNetworkServiceProxyAllowListMatch_DirectOnly) {
  std::map<std::string, std::string> parameters;
  parameters[net::features::kIpPrivacyDirectOnly.name] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"foo"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection({});
  expected_proxy_list.AddProxyChain(std::move(ip_protection_proxy_chain));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(
    IpProtectionProxyDelegateTest,
    OnResolveProxyNetworkServiceProxyAllowListDoesNotMatch_FirstPartyException) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {"top.com"};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList(
      {MakeChain({"ippro-1"}), MakeChain({"ippro-2"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kIneligible, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_NoAuthToken) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetProxyList({MakeChain({"proxy"})});
  // No token is added to the cache, so the result will be direct.
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, false, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_NoProxyList) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  // No proxy list is added to the cache, so the result will be direct.
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, false,
                                       1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, false, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnResolveProxy_IpProtectionDisabled) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"proxy"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  delegate->SetIpProtectionEnabled(false);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

// When URLs do not match the allow list, the result is direct and not flagged
// as for IP protection.
TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyIpProtectionNoMatch) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["not.example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList(
      {MakeChain({"ippro-1"}), MakeChain({"ippro-2"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kIneligible, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

// If the allowlist is empty, this suggests it hasn't yet been populated and
// thus we don't really know whether the request is supposed to be eligible or
// not.
TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyIpProtectionNoMatch_UnpopulatedAllowList) {
  std::map<std::string, std::set<std::string>> first_party_map;
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList(
      {MakeChain({"ippro-1"}), MakeChain({"ippro-2"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kUnknown, 1);
  histogram_tester_.ExpectTotalCount(kAreAuthTokensAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kIsProxyListAvailableHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAvailabilityHistogram, 0);
}

// When the URL is HTTP and single-proxy chains are used, the result is direct
// and not flagged as for IP Protection.
// TODO(https://crbug.com/1474932): Once IP Protection chains are guaranteed to
// be multi-proxy, we can remove this test.
TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyIpProtectionSingleProxyHttpFailure) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"proxy"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
  // Whilst we're not proxying the request in this case, it is "eligible" for
  // protection in general, and would be protected under ideal circumstances.
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  // Despite IP Protection not being applied in this case, it is still
  // considered generally "available". (It is fully initialized and able to
  // service most normal requests).
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

// When the URL is HTTP and multi-proxy chains are used, the result is flagged
// as for IP protection and is not direct.
TEST_F(IpProtectionProxyDelegateTest,
       OnResolveProxyIpProtectionMultiProxyHttpSuccess) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"proxy1", "proxy2"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

// When URLs match the allow list, and a token is available, the result is
// flagged as for IP protection and is not direct.
TEST_F(IpProtectionProxyDelegateTest, OnResolveProxyIpProtectionHttpsSuccess) {
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({MakeChain({"proxy"})});
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
  histogram_tester_.ExpectUniqueSample(
      kEligibilityHistogram,
      IpProtectionProxyDelegate::ProtectionEligibility::kEligible, 1);
  histogram_tester_.ExpectUniqueSample(kAreAuthTokensAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kIsProxyListAvailableHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kAvailabilityHistogram, true, 1);
}

TEST_F(IpProtectionProxyDelegateTest, OnFallback) {
  constexpr int kChainId = 2;
  auto ip_protection_proxy_chain = net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy.com", std::nullopt)},
      kChainId);
  bool force_refresh_called = false;

  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(/*first_party_map=*/{});
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { force_refresh_called = true; }));
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));

  delegate->OnFallback(ip_protection_proxy_chain, net::ERR_FAILED);
  EXPECT_TRUE(force_refresh_called);
  histogram_tester_.ExpectBucketCount(
      "NetworkService.IpProtection.ProxyChainFallback", kChainId, 1);
}

TEST_F(IpProtectionProxyDelegateTest, MergeProxyRules) {
  net::ProxyChain chain1({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy2a.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy2b.com", 80),
  });
  net::ProxyChain chain2(net::ProxyChain::Direct());
  net::ProxyChain chain3(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "proxy1.com", 80));
  net::ProxyList existing_proxy_list;
  existing_proxy_list.AddProxyChain(chain1);
  existing_proxy_list.AddProxyChain(chain2);
  existing_proxy_list.AddProxyChain(chain3);

  net::ProxyChain custom1({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "custom-a.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "custom-b.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "custom-c.com", 80),
  });
  net::ProxyChain custom2(net::ProxyChain::Direct());
  net::ProxyList custom_proxy_list;
  custom_proxy_list.AddProxyChain(custom1);
  custom_proxy_list.AddProxyChain(custom2);

  auto result = IpProtectionProxyDelegate::MergeProxyRules(existing_proxy_list,
                                                           custom_proxy_list);

  // Custom chains replace `chain2`.
  std::vector<net::ProxyChain> expected = {
      chain1,
      custom1,
      custom2,
      chain3,
  };
  EXPECT_EQ(result.AllChains(), expected);
}

TEST_F(IpProtectionProxyDelegateTest, InvalidateTryAgainAfterTime) {
  bool invalidated = false;

  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(/*first_party_map=*/{});
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetOnInvalidateTryAgainAfterTime(
      base::BindLambdaForTesting([&]() { invalidated = true; }));
  auto delegate = CreateDelegate(&network_service_proxy_allow_list,
                                 std::move(ipp_config_cache));
  delegate->InvalidateIpProtectionConfigCacheTryAgainAfterTime();
  EXPECT_TRUE(invalidated);
}

}  // namespace network
