// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"

#include <optional>
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
#include "net/proxy_resolution/proxy_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kHttpUrl[] = "http://example.com";
constexpr char kLocalhost[] = "http://localhost";
constexpr char kHttpsUrl[] = "https://example.com";
constexpr char kWebsocketUrl[] = "ws://example.com";

}  // namespace

MATCHER_P2(Contain,
           expected_name,
           expected_value,
           std::string("headers ") + (negation ? "don't " : "") + "contain '" +
               expected_name + ": " + expected_value + "'") {
  return arg.GetHeader(expected_name) == expected_value;
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
    std::unique_ptr<TestCustomProxyConnectionObserver> observer =
        std::make_unique<TestCustomProxyConnectionObserver>();
    observer_ = observer.get();

    mojo::PendingRemote<mojom::CustomProxyConnectionObserver> observer_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(observer), observer_remote.InitWithNewPipeAndPassReceiver());

    auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
        network::mojom::CustomProxyConfig::New(),
        client_.BindNewPipeAndPassReceiver(), std::move(observer_remote));
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
      nullptr, client.BindNewPipeAndPassReceiver(), mojo::NullRemote());

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

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDoesNotMergeDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), net::NetworkAnonymizationKey(),
                           "GET", net::ProxyRetryInfoMap(), &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyDoesNotMergeWhenOverrideExistingConfigFlagIsEnabled) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
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

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyNothingMatching) {
  auto config = mojom::CustomProxyConfig::New();
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost),
                           net::NetworkAnonymizationKey::CreateCrossSite(
                               net::SchemefulSite(GURL("http://top.com"))),
                           "GET", net::ProxyRetryInfoMap(), &result);
  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.is_for_ip_protection());
}

TEST_F(NetworkServiceProxyDelegateTest, InitialConfigUsedForProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  mojo::Remote<mojom::CustomProxyConfigClient> client;
  auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
      std::move(config), client.BindNewPipeAndPassReceiver(),
      mojo::NullRemote());

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

}  // namespace network
