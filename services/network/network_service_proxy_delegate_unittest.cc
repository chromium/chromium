// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"

#include <string>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kHttpUrl[] = "http://example.com";
constexpr char kLocalhost[] = "http://localhost";
constexpr char kHttpsUrl[] = "https://example.com";
constexpr char kWebsocketUrl[] = "ws://example.com";
constexpr char kBypassUrl[] = "http://bypass.com";

}  // namespace

MATCHER_P2(Contain,
           expected_name,
           expected_value,
           std::string("headers ") + (negation ? "don't " : "") + "contain '" +
               expected_name + ": " + expected_value + "'") {
  std::string value;
  return arg.GetHeader(expected_name, &value) && value == expected_value;
}

class NetworkServiceProxyDelegateTest : public testing::Test {
 public:
  NetworkServiceProxyDelegateTest() {}

  void SetUp() override {
    context_ = std::make_unique<net::TestURLRequestContext>(true);
    context_->Init();
  }

 protected:
  std::unique_ptr<NetworkServiceProxyDelegate> CreateDelegate(
      mojom::CustomProxyConfigPtr config) {
    auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
        network::mojom::CustomProxyConfig::New(),
        client_.BindNewPipeAndPassReceiver());
    SetConfig(std::move(config));
    return delegate;
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void SetConfig(mojom::CustomProxyConfigPtr config) {
    client_->OnCustomProxyConfigUpdated(std::move(config));
    task_environment_.RunUntilIdle();
  }

 private:
  mojo::Remote<mojom::CustomProxyConfigClient> client_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkServiceProxyDelegateTest, NullConfigDoesNotCrash) {
  mojo::Remote<mojom::CustomProxyConfigClient> client;
  auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
      nullptr, client.BindNewPipeAndPassReceiver());

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);
}

TEST_F(NetworkServiceProxyDelegateTest, AddsHeadersBeforeCache) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  EXPECT_THAT(headers, Contain("foo", "bar"));
}

TEST_F(NetworkServiceProxyDelegateTest,
       DoesNotAddHeadersBeforeCacheWithEmptyConfig) {
  auto config = mojom::CustomProxyConfig::New();
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest, DoesNotAddHeadersBeforeCacheForHttps) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpsUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest,
       DoesNotAddHeadersBeforeCacheForWebSocket) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kWebsocketUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest, AddsHeadersAfterCache) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->post_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UsePacString("PROXY proxy");
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_THAT(headers, Contain("foo", "bar"));
}

TEST_F(NetworkServiceProxyDelegateTest,
       DoesNotAddHeadersAfterCacheForProxyNotInConfig) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->post_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UsePacString("PROXY other");
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest, DoesNotAddHeadersAfterCacheForDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->post_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UseDirect();
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest, DoesNotAddHeadersAfterCacheForHttps) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->post_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpsUrl));
  net::ProxyInfo info;
  info.UsePacString("PROXY proxy");
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest, DoesNotAddHeadersIfProxyIsBypassed) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->rules.bypass_rules.AddRuleFromString(GURL(kBypassUrl).host());
  config->pre_cache_headers.SetHeader("pre", "cache");
  config->post_cache_headers.SetHeader("post", "cache");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kBypassUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  net::ProxyInfo info;
  info.UseDirect();
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest,
       RemovesPreCacheHeadersWhenProxyNotInConfig) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UseDirect();
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest,
       DoesNotRemoveHeaderForHttpsIfAlreadyExists) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bad");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "value");
  auto request = CreateRequest(GURL(kHttpsUrl));
  net::ProxyInfo info;
  info.UseDirect();
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_THAT(headers, Contain("foo", "value"));
}

TEST_F(NetworkServiceProxyDelegateTest, KeepsPreCacheHeadersWhenProxyInConfig) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UsePacString("PROXY proxy");
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_THAT(headers, Contain("foo", "bar"));
}

TEST_F(NetworkServiceProxyDelegateTest, KeepsHeadersWhenConfigUpdated) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(config->Clone());

  // Update config with new proxy.
  config->rules.ParseFromString("http=other");
  SetConfig(std::move(config));

  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UsePacString("PROXY proxy");
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_THAT(headers, Contain("foo", "bar"));
}

TEST_F(NetworkServiceProxyDelegateTest,
       RemovesPreCacheHeadersWhenConfigUpdatedToBeEmpty) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(config->Clone());

  // Update config with empty proxy rules.
  config->rules = net::ProxyConfig::ProxyRules();
  SetConfig(std::move(config));

  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  auto request = CreateRequest(GURL(kHttpUrl));
  net::ProxyInfo info;
  info.UseDirect();
  delegate->OnBeforeSendHeaders(request.get(), info, &headers);

  EXPECT_TRUE(headers.IsEmpty());
}

TEST_F(NetworkServiceProxyDelegateTest, AddsHeadersToTunnelRequest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->pre_cache_headers.SetHeader("pre_cache", "foo");
  config->post_cache_headers.SetHeader("post_cache", "bar");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto proxy_server = net::ProxyServer::FromPacString("HTTPS proxy");
  delegate->OnBeforeHttp1TunnelRequest(proxy_server, &headers);

  EXPECT_FALSE(headers.HasHeader("pre_cache"));
  EXPECT_FALSE(headers.HasHeader("post_cache"));
  EXPECT_THAT(headers, Contain("connect", "baz"));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessHttpProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  // HTTP proxies are not used as alternative QUIC proxies.
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessHttpsProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=https://foo");
  config->assume_https_proxies_support_quic = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("HTTPS foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_EQ(result.alternative_proxy(),
            net::ProxyServer::FromPacString("QUIC foo"));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessHttpsProxyNoQuic) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=https://foo");
  config->assume_https_proxies_support_quic = false;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessHttpsUrl) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("HTTPS foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxySuccessWebSocketUrl) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kWebsocketUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("HTTPS foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyNoRuleForHttpsUrl) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpsUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyLocalhost) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kLocalhost), "GET", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyEmptyConfig) {
  auto delegate = CreateDelegate(mojom::CustomProxyConfig::New());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyNonIdempotentMethod) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "POST", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyNonIdempotentMethodAllowed) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->allow_non_idempotent_methods = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "POST", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
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
  delegate->OnResolveProxy(GURL(kWebsocketUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_TRUE(result.is_direct());
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDoesNotOverrideExisting) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->should_override_existing_config = false;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar");
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("PROXY bar"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyOverridesExisting) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->should_override_existing_config = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar");
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
  EXPECT_FALSE(result.alternative_proxy().is_valid());
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDeprioritizesBadProxies) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo,bar");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info = retry_map["foo:80"];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::TimeDelta::FromDays(2);
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", retry_map, &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("PROXY bar"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyAllProxiesBad) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UseDirect();
  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo& info = retry_map["foo:80"];
  info.try_while_bad = false;
  info.bad_until = base::TimeTicks::Now() + base::TimeDelta::FromDays(2);
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", retry_map, &result);

  EXPECT_TRUE(result.is_direct());
}

TEST_F(NetworkServiceProxyDelegateTest, InitialConfigUsedForProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  mojo::Remote<mojom::CustomProxyConfigClient> client;
  auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
      std::move(config), client.BindNewPipeAndPassReceiver());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::ProxyServer::FromPacString("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

}  // namespace network
