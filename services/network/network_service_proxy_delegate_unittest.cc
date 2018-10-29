// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"
#include "base/test/scoped_task_environment.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kHttpUrl[] = "http://example.com";
constexpr char kLocalhost[] = "http://localhost";
constexpr char kHttpsUrl[] = "https://example.com";
constexpr char kWebsocketUrl[] = "ws://example.com";

}  // namespace

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
        network::mojom::CustomProxyConfig::New(), mojo::MakeRequest(&client_));
    SetConfig(std::move(config));
    return delegate;
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, nullptr);
  }

  void SetConfig(mojom::CustomProxyConfigPtr config) {
    client_->OnCustomProxyConfigUpdated(std::move(config));
    scoped_task_environment_.RunUntilIdle();
  }

 private:
  mojom::CustomProxyConfigClientPtr client_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(NetworkServiceProxyDelegateTest, AddsHeadersBeforeCache) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kHttpUrl));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  std::string value;
  EXPECT_TRUE(headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
}

TEST_F(NetworkServiceProxyDelegateTest,
       DoesNotAddHeadersBeforeCacheForLocalhost) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=proxy");
  config->pre_cache_headers.SetHeader("foo", "bar");
  auto delegate = CreateDelegate(std::move(config));

  net::HttpRequestHeaders headers;
  auto request = CreateRequest(GURL(kLocalhost));
  delegate->OnBeforeStartTransaction(request.get(), &headers);

  EXPECT_TRUE(headers.IsEmpty());
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

  std::string value;
  EXPECT_TRUE(headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
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

  std::string value;
  EXPECT_TRUE(headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "value");
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

  std::string value;
  EXPECT_TRUE(headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
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

  std::string value;
  EXPECT_TRUE(headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
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

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyWebsocketScheme) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
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

TEST_F(NetworkServiceProxyDelegateTest, InitialConfigUsedForProxy) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  mojom::CustomProxyConfigClientPtr client;
  auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
      std::move(config), mojo::MakeRequest(&client));

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
