// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  std::string value;
  return arg.GetHeader(expected_name, &value) && value == expected_value;
}

class TestCustomProxyConnectionObserver
    : public mojom::CustomProxyConnectionObserver {
 public:
  TestCustomProxyConnectionObserver() = default;
  ~TestCustomProxyConnectionObserver() override = default;

  const absl::optional<std::pair<net::ProxyServer, int>>& FallbackArgs() const {
    return fallback_;
  }

  const absl::optional<
      std::pair<net::ProxyServer, scoped_refptr<net::HttpResponseHeaders>>>&
  HeadersReceivedArgs() const {
    return headers_received_;
  }

  // mojom::CustomProxyConnectionObserver:
  void OnFallback(const net::ProxyServer& bad_proxy, int net_error) override {
    fallback_ = std::make_pair(bad_proxy, net_error);
  }
  void OnTunnelHeadersReceived(const net::ProxyServer& proxy_server,
                               const scoped_refptr<net::HttpResponseHeaders>&
                                   response_headers) override {
    headers_received_ = std::make_pair(proxy_server, response_headers);
  }

 private:
  absl::optional<std::pair<net::ProxyServer, int>> fallback_;
  absl::optional<
      std::pair<net::ProxyServer, scoped_refptr<net::HttpResponseHeaders>>>
      headers_received_;
};

class NetworkServiceProxyDelegateTest : public testing::Test {
 public:
  NetworkServiceProxyDelegateTest() {}

  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
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

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestCustomProxyConnectionObserver* TestObserver() const { return observer_; }

 private:
  mojo::Remote<mojom::CustomProxyConfigClient> client_;
  // Owned by the proxy delegate returned by |CreateDelegate|.
  raw_ptr<TestCustomProxyConnectionObserver> observer_ = nullptr;
  std::unique_ptr<net::URLRequestContext> context_;
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
  auto proxy_server = net::PacResultElementToProxyServer("HTTPS proxy");
  delegate->OnBeforeTunnelRequest(proxy_server, &headers);

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
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
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
      net::PacResultElementToProxyServer("HTTPS foo"));
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
      net::PacResultElementToProxyServer("HTTPS foo"));
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
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyEmptyConfig) {
  auto delegate = CreateDelegate(mojom::CustomProxyConfig::New());

  net::ProxyInfo result;
  result.UseDirect();
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  EXPECT_TRUE(result.is_direct());
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
      net::PacResultElementToProxyServer("PROXY foo"));
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
      net::PacResultElementToProxyServer("PROXY bar"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
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
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyMergesDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  config->should_replace_direct = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));

  // Resolve proxy for HTTPS URL and check that proxy list is not modified since
  // the config rules specify http
  net::ProxyInfo result_https;
  result_https.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpsUrl), "GET", net::ProxyRetryInfoMap(),
                           &result_https);

  net::ProxyList expected_proxy_list_https;
  expected_proxy_list_https.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list_https.AddProxyServer(
      net::PacResultElementToProxyServer("DIRECT"));

  EXPECT_TRUE(result_https.proxy_list().Equals(expected_proxy_list_https));
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyMergesConfigsThatIncludeDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo, direct://");
  config->should_replace_direct = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("DIRECT"));

  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest, OnResolveProxyDoesNotMergeDirect) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
  config->should_replace_direct = false;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; DIRECT");
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("DIRECT"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest,
       OnResolveProxyDoesNotMergeWhenDirectIsNotSet) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https=foo");
  config->should_replace_direct = true;
  auto delegate = CreateDelegate(std::move(config));

  net::ProxyInfo result;
  result.UsePacString("PROXY bar; PROXY baz");
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY baz"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
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
  delegate->OnResolveProxy(GURL(kHttpsUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
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
  info.bad_until = base::TimeTicks::Now() + base::Days(2);
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", retry_map, &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY bar"));
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
  info.bad_until = base::TimeTicks::Now() + base::Days(2);
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", retry_map, &result);

  EXPECT_TRUE(result.is_direct());
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
  delegate->OnResolveProxy(GURL(kHttpUrl), "GET", net::ProxyRetryInfoMap(),
                           &result);

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foo"));
  EXPECT_TRUE(result.proxy_list().Equals(expected_proxy_list));
}

TEST_F(NetworkServiceProxyDelegateTest, OnFallbackObserved) {
  net::ProxyServer proxy(net::ProxyServer::SCHEME_HTTP,
                         net::HostPortPair("proxy.com", 80));

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(TestObserver()->FallbackArgs());
  delegate->OnFallback(proxy, net::ERR_FAILED);
  RunUntilIdle();
  ASSERT_TRUE(TestObserver()->FallbackArgs());
  EXPECT_EQ(TestObserver()->FallbackArgs()->first, proxy);
  EXPECT_EQ(TestObserver()->FallbackArgs()->second, net::ERR_FAILED);
}

TEST_F(NetworkServiceProxyDelegateTest, OnTunnelHeadersReceivedObserved) {
  net::ProxyServer proxy(net::ProxyServer::SCHEME_HTTP,
                         net::HostPortPair("proxy.com", 80));
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 200\nHello: World\n\n");

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("http=foo");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(TestObserver()->HeadersReceivedArgs());
  EXPECT_EQ(net::OK, delegate->OnTunnelHeadersReceived(proxy, *headers));
  RunUntilIdle();
  ASSERT_TRUE(TestObserver()->HeadersReceivedArgs());
  EXPECT_EQ(TestObserver()->HeadersReceivedArgs()->first, proxy);
  // Compare raw header strings since the headers pointer is copied.
  EXPECT_EQ(TestObserver()->HeadersReceivedArgs()->second->raw_headers(),
            headers->raw_headers());
}

}  // namespace network
