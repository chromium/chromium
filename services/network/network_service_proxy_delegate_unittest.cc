// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection_token_cache_manager.h"
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
namespace {

constexpr char kHttpUrl[] = "http://example.com";
constexpr char kLocalhost[] = "http://localhost";
constexpr char kHttpsUrl[] = "https://example.com";
constexpr char kWebsocketUrl[] = "ws://example.com";

class MockIpProtectionConfigCache : public IpProtectionConfigCache {
 public:
  bool AreAuthTokensAvailable() override { return auth_token_.has_value(); }
  void InvalidateTryAgainAfterTime() override {}
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken(
      size_t chain_index) override {
    return std::move(auth_token_);
  }

  // Set the auth token that will be returned from the next call to
  // `GetAuthToken()`.
  void SetNextAuthToken(
      absl::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token) {
    auth_token_ = std::move(auth_token);
  }

  void SetUp() override { NOTREACHED_NORETURN(); }

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
    return proxy_chain_list_;
  }

  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<std::vector<std::string>> proxy_list) {
    proxy_list_ = std::move(proxy_list);
    proxy_chain_list_.clear();
    for (const auto& proxy_chain_hostnames : *proxy_list_) {
      std::vector<net::ProxyServer> proxy_servers;
      for (const auto& proxy : proxy_chain_hostnames) {
        net::ProxyServer proxy_server = net::ProxyServer::FromSchemeHostAndPort(
            net::ProxyServer::SCHEME_HTTPS, proxy, absl::nullopt);
        proxy_servers.push_back(std::move(proxy_server));
      }
      proxy_chain_list_.emplace_back(std::move(proxy_servers));
    }
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list) {
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

 private:
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token_;
  absl::optional<std::vector<std::vector<std::string>>> proxy_list_;
  std::vector<net::ProxyChain> proxy_chain_list_;
  base::OnceClosure on_force_refresh_proxy_list_;
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

  const absl::optional<std::pair<net::ProxyChain, int>>& FallbackArgs() const {
    return fallback_;
  }

  const absl::optional<HeadersReceived>& HeadersReceivedArgs() const {
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
  absl::optional<std::pair<net::ProxyChain, int>> fallback_;
  absl::optional<HeadersReceived> headers_received_;
};

class NetworkServiceProxyDelegateTest : public testing::Test {
 public:
  NetworkServiceProxyDelegateTest() = default;

  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
    scoped_feature_list_.InitWithFeatures(
        {net::features::kEnableIpProtectionProxy,
         network::features::kMaskedDomainList},
        {});
  }

 protected:
  std::unique_ptr<NetworkServiceProxyDelegate> CreateDelegate(
      mojom::CustomProxyConfigPtr config) {
    return CreateDelegate(std::move(config), nullptr);
  }

  std::unique_ptr<NetworkServiceProxyDelegate> CreateDelegate(
      mojom::CustomProxyConfigPtr config,
      NetworkServiceProxyAllowList* network_service_proxy_allow_list) {
    std::unique_ptr<TestCustomProxyConnectionObserver> observer =
        std::make_unique<TestCustomProxyConnectionObserver>();
    observer_ = observer.get();

    mojo::PendingRemote<mojom::CustomProxyConnectionObserver> observer_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(observer), observer_remote.InitWithNewPipeAndPassReceiver());

    auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
        network::mojom::CustomProxyConfig::New(),
        client_.BindNewPipeAndPassReceiver(), std::move(observer_remote),
        network_service_proxy_allow_list);
    SetConfig(std::move(config));
    return delegate;
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void SetConfig(mojom::CustomProxyConfigPtr config) {
    base::RunLoop loop;
    client_->OnCustomProxyConfigUpdated(std::move(config), loop.QuitClosure());
    loop.Run();
  }

  mojom::BlindSignedAuthTokenPtr MakeAuthToken(std::string content) {
    auto token = mojom::BlindSignedAuthToken::New();
    token->token = std::move(content);
    return token;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestCustomProxyConnectionObserver* TestObserver() const { return observer_; }

 private:
  mojo::Remote<mojom::CustomProxyConfigClient> client_;
  // Owned by the proxy delegate returned by |CreateDelegate|.
  raw_ptr<TestCustomProxyConnectionObserver> observer_ = nullptr;
  std::unique_ptr<net::URLRequestContext> context_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkServiceProxyDelegateTest, NullConfigDoesNotCrash) {
  mojo::Remote<mojom::CustomProxyConfigClient> client;
  auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
      nullptr, client.BindNewPipeAndPassReceiver(), mojo::NullRemote(),
      nullptr);

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
}

TEST_F(NetworkServiceProxyDelegateTest, AddsHeadersToTunnelRequest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  delegate->OnBeforeTunnelRequest(proxy_chain, /*chain_index=*/0, &headers);

  EXPECT_THAT(headers, Contain("connect", "baz"));
}

TEST_F(NetworkServiceProxyDelegateTest, AddsTokenToTunnelRequest) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();
  auto delegate = CreateDelegate(std::move(config));

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"proxya", "proxyb"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::HttpRequestHeaders headers;
  auto proxy_chain = net::ProxyChain(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", absl::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", absl::nullopt)});
  delegate->OnBeforeTunnelRequest(proxy_chain, /*chain_index=*/0, &headers);

  EXPECT_THAT(headers, Contain("Authorization", "Bearer: a-token"));
}

TEST_F(NetworkServiceProxyDelegateTest, AddsPskToTunnelRequest) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyProxyBPsk"] = "seekrit";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));

  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();
  auto delegate = CreateDelegate(std::move(config));

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetProxyList({{"proxya", "proxyb"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::HttpRequestHeaders headers;
  auto proxy_chain = net::ProxyChain(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxya", absl::nullopt),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxyb", absl::nullopt)});
  delegate->OnBeforeTunnelRequest(proxy_chain, /*chain_index=*/0, &headers);
  EXPECT_THAT(headers, testing::Not(Contain("Proxy-Authorization",
                                            "Preshared seekrit")));

  delegate->OnBeforeTunnelRequest(proxy_chain, /*chain_index=*/1, &headers);
  EXPECT_THAT(headers, Contain("Proxy-Authorization", "Preshared seekrit"));
}

TEST_F(NetworkServiceProxyDelegateTest, NoTokenIfNotIpProtection) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::HttpRequestHeaders headers;
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  delegate->OnBeforeTunnelRequest(proxy_chain, /*chain_index=*/0, &headers);

  std::string value;
  EXPECT_FALSE(headers.GetHeader("Authorization", &value));
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyDiscardsInvalidProxyServers) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"foo:80"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessHttpProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessHttpsUrl) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessWebSocketUrl) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kWebsocketUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyNoRuleForHttpsUrl) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyLocalhost) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyEmptyConfig) {
  auto delegate = CreateDelegate(mojom::CustomProxyConfig::New());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyNonIdempotentMethod) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "POST", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyNonIdempotentMethodAllowed) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->allow_non_idempotent_methods = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "POST", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyBypassForWebSocketScheme) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->rules.bypass_rules.AddRuleFromString(GURL(kWebsocketUrl).scheme() +
                                               "://*");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kWebsocketUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDoesNotOverrideExisting) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->should_override_existing_config = false;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyOverridesExisting) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->should_override_existing_config = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyMergesDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->should_replace_direct = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());

  // Resolve proxy for HTTPS URL and check that proxy list is not modified since
  // the config rules specify http
  net::ProxyInfo result_https;
  result_https.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpsUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result_https);

  net::ProxyList expected_proxy_list_https;
  expected_proxy_list_https.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list_https.AddProxyServer(
      net::PacResultElementToProxyServer("DIRECT"));

  EXPECT_TRUE(result_https.proxy_list().Equals(expected_proxy_list_https));
  EXPECT_FALSE(result_https.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyMergesConfigsThatIncludeDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo, direct://");
  config->should_replace_direct = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("DIRECT"));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDoesNotMergeDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
  config->should_replace_direct = false;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("DIRECT"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyDoesNotMergeWhenDirectIsNotSet) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
  config->should_replace_direct = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; PROXY baz");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY baz"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyDoesNotMergeWhenOverrideExistingConfigFlagIsEnabled) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
  config->should_replace_direct = true;
  config->should_override_existing_config = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpsUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDeprioritizesBadProxies) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo,bar");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info =
      retry_map[ProxyUriToProxyChain("foo:80", net::ProxyServer::SCHEME_HTTP)];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::Days(2);
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", retry_map, &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyAllProxiesBad) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info =
      retry_map[ProxyUriToProxyChain("foo:80", net::ProxyServer::SCHEME_HTTP)];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::Days(2);
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", retry_map, &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyNetworkServiceProxyAllowListMatch) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"ippro-1"}, {"ippro-2"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

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
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS ippro-1"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS ippro-2"));
  expected_proxy_list.AddProxyServer(net::ProxyServer::Direct());
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY weird"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_TRUE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyNetworkServiceProxyAllowListMatch_DirectOnly) {
  std::map<std::string, std::string> parameters;
  parameters["IpPrivacyDirectOnly"] = "true";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy, std::move(parameters));
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"foo"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  // Proxy server is not added.
  expected_proxy_list.AddProxyServer(net::ProxyServer::Direct());
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list))
      << "Got: " << result.proxy_list().ToDebugString();
  EXPECT_TRUE(result.is_for_ip_protection());
}

TEST_F(
    NetworkServiceProxyDelegateTest,
    OnResolveProxyNetworkServiceProxyAllowListDoesNotMatch_FirstPartyException) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {"top.com"};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"ippro-1"}, {"ippro-2"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxy_NoConfigCache) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxy_NoAuthToken) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetProxyList({{"proxy"}});
  // No token is added to the cache, so the result will be direct.
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxy_NoProxyList) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);
  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  // No proxy list is added to the cache, so the result will be direct.
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxy_AllowListDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList});
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"proxy"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(
    NetworkServiceProxyDelegateTest,
    OnResolveProxyNetworkServiceProxyAllowListDoesNotMatch_ResourceNotAllowed) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();

  std::map<std::string, std::set<std::string>> first_party_map;
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);

  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"ippro-1"}, {"ippro-2"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

// When a `config` does not look like an IP Protection `CustomProxyConfig`, the
// result is direct and not flagged as for IP Protection.
TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyIpProtectionDisabledByConfig) {
  auto config = mojom::CustomProxyConfig::New();
  auto delegate = CreateDelegate(std::move(config));

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"ippro-1"}, {"ippro-2"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

// When a `config` does look like an IP Protection `CustomProxyConfig`, but the
// URLs do not match the allow list, the result is direct and not flagged as for
// IP protection.
TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyIpProtectionNoMatch) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();
  std::map<std::string, std::set<std::string>> first_party_map;
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"ippro-1"}, {"ippro-2"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

// When a `config` does look like an IP Protection `CustomProxyConfig`, but
// the URL is HTTP instead of HTTPS, the result is direct and not flagged as for
// IP Protection.
// TODO(https://crbug.com/1474932): Support proxying HTTP URLs by using
// CONNECT requests (i.e. tunnelling) instead of using the old-style proxy GET
// requests from the last proxy in the chain.
TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyIpProtectionHttpFailure) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"proxy"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

// When a `config` does look like an IP Protection `CustomProxyConfig` and
// the URLs match the allow list, and a token is available, the result is
// flagged as for IP protection and is not direct.
TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyIpProtectionHttpsSuccess) {
  auto config =
      NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig();
  std::map<std::string, std::set<std::string>> first_party_map;
  first_party_map["example.com"] = {};
  auto network_service_proxy_allow_list =
      NetworkServiceProxyAllowList::CreateForTesting(first_party_map);
  auto delegate =
      CreateDelegate(std::move(config), &network_service_proxy_allow_list);

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetNextAuthToken(MakeAuthToken("Bearer: a-token"));
  ipp_config_cache->SetProxyList({{"proxy"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("https://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_TRUE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, InitialConfigUsedForProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  mojo::Remote<mojom::CustomProxyConfigClient> client;
  auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
      std::move(config), client.BindNewPipeAndPassReceiver(),
      mojo::NullRemote(), nullptr);

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, OnFallbackObserved) {
  net::ProxyChain proxy_chain(net::ProxyServer::SCHEME_HTTP,
                              net::HostPortPair("proxy.com", 80));

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(TestObserver()->FallbackArgs());
  delegate->OnFallback(proxy_chain, net::ERR_FAILED);
  RunUntilIdle();
  ASSERT_TRUE(TestObserver()->FallbackArgs());
  EXPECT_EQ(TestObserver()->FallbackArgs()->first, proxy_chain);
  EXPECT_EQ(TestObserver()->FallbackArgs()->second, net::ERR_FAILED);
}

TEST_F(NetworkServiceProxyDelegateTest, OnFallback_IpProtection) {
  auto proxy_chain = net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "proxy.com", absl::nullopt);
  bool force_refresh_called = false;

  auto config = mojom::CustomProxyConfig::New();
  auto delegate = CreateDelegate(std::move(config));

  auto ipp_config_cache = std::make_unique<MockIpProtectionConfigCache>();
  ipp_config_cache->SetOnRequestRefreshProxyList(
      base::BindLambdaForTesting([&]() { force_refresh_called = true; }));
  ipp_config_cache->SetProxyList({{"proxy.com"}});
  delegate->SetIpProtectionConfigCache(std::move(ipp_config_cache));

  delegate->OnFallback(proxy_chain, net::ERR_FAILED);
  EXPECT_TRUE(force_refresh_called);
}

TEST_F(NetworkServiceProxyDelegateTest, OnTunnelHeadersReceivedObserved) {
  net::ProxyChain proxy_chain({
      net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                       net::HostPortPair("proxy1.com", 80)),
      net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                       net::HostPortPair("proxy2.com", 80)),
      net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                       net::HostPortPair("proxy3.com", 80)),
  });
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 200\nHello: World\n\n");

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(TestObserver()->HeadersReceivedArgs());
  EXPECT_EQ(net::OK, delegate->OnTunnelHeadersReceived(
                         proxy_chain, /*chain_index=*/2, *headers));
  RunUntilIdle();
  ASSERT_TRUE(TestObserver()->HeadersReceivedArgs());
  EXPECT_EQ(TestObserver()->HeadersReceivedArgs()->proxy_chain, proxy_chain);
  EXPECT_EQ(TestObserver()->HeadersReceivedArgs()->chain_index, 2UL);
  // Compare raw header strings since the headers pointer is copied.
  EXPECT_EQ(
      TestObserver()->HeadersReceivedArgs()->response_headers->raw_headers(),
      headers->raw_headers());
}

TEST_F(NetworkServiceProxyDelegateTest, MergeProxyRules) {
  net::ProxyChain chain1({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy2a.com", 80),
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy2b.com", 80),
  });
  net::ProxyChain chain2(net::ProxyServer::Direct());
  net::ProxyChain chain3({
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              "proxy1.com", 80),
  });
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
  net::ProxyChain custom2(net::ProxyServer::Direct());
  net::ProxyList custom_proxy_list;
  custom_proxy_list.AddProxyChain(custom1);
  custom_proxy_list.AddProxyChain(custom2);

  auto config = mojom::CustomProxyConfig::New();
  auto delegate = CreateDelegate(std::move(config));

  auto result =
      delegate->MergeProxyRules(existing_proxy_list, custom_proxy_list);

  // Custom chains replace `chain2`.
  std::vector<net::ProxyChain> expected = {
      chain1,
      custom1,
      custom2,
      chain3,
  };
  EXPECT_EQ(result.AllChains(), expected);
}

}  // namespace network
