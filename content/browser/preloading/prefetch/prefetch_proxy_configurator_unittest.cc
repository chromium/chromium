// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_proxy_configurator.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kApiKey[] = "APIKEY";

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  explicit TestCustomProxyConfigClient(
      mojo::PendingReceiver<network::mojom::CustomProxyConfigClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // network::mojom::CustomProxyConfigClient:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config,
      OnCustomProxyConfigUpdatedCallback callback) override {
    config_ = std::move(proxy_config);
    std::move(callback).Run();
  }

  network::mojom::CustomProxyConfigPtr config_;

 private:
  mojo::Receiver<network::mojom::CustomProxyConfigClient> receiver_;
};

class PrefetchProxyConfiguratorTest : public testing::Test {
 public:
  network::mojom::CustomProxyConfigPtr LatestProxyConfig() {
    return std::move(config_client_->config_);
  }

  GURL prefetch_proxy_url() { return GURL("https://prefetchproxy.com"); }

  void VerifyLatestProxyConfig(const GURL& proxy_url,
                               const net::HttpRequestHeaders& headers) {
    auto config = LatestProxyConfig();
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);

    EXPECT_EQ(config->connect_tunnel_headers.ToString(), headers.ToString());

    EXPECT_EQ(config->rules.proxies_for_http.size(), 0U);
    EXPECT_EQ(config->rules.proxies_for_ftp.size(), 0U);

    ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
    EXPECT_EQ(GURL(net::ProxyServerToProxyUri(
                  config->rules.proxies_for_https.First().GetProxyServer(
                      /*chain_index=*/0))),
              proxy_url);
  }

  PrefetchProxyConfigurator* configurator() {
    if (!configurator_) {
      // Lazy construct and init so that any changed field trials can be used.
      configurator_ = std::make_unique<PrefetchProxyConfigurator>(
          prefetch_proxy_url(), kApiKey);
      mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
      config_client_ = std::make_unique<TestCustomProxyConfigClient>(
          client_remote.BindNewPipeAndPassReceiver());
      base::RunLoop run_loop;
      configurator_->AddCustomProxyConfigClient(std::move(client_remote),
                                                run_loop.QuitClosure());
      configurator_->SetClockForTesting(task_environment_.GetMockClock());
      run_loop.Run();
    }
    return configurator_.get();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PrefetchProxyConfigurator> configurator_;
  std::unique_ptr<TestCustomProxyConfigClient> config_client_;
};

TEST_F(PrefetchProxyConfiguratorTest, Fallback_DoesRandomBackoff_ErrFailed) {
  base::HistogramTester histogram_tester;

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(prefetch_proxy_url()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnFallback(proxy_chain, net::ERR_FAILED);
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.Fallback.NetError",
                                      std::abs(net::ERR_FAILED), 1);

  FastForwardBy(base::Seconds(5 * 60 + 1));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyConfiguratorTest, FallbackDoesRandomBackoff_ErrOK) {
  base::HistogramTester histogram_tester;

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(prefetch_proxy_url()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnFallback(proxy_chain, net::OK);
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.Fallback.NetError",
                                      net::OK, 1);

  FastForwardBy(base::Seconds(5 * 60 + 1));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyConfiguratorTest, Fallback_DifferentProxy) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchUseContentRefactor);

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(GURL("http://foo.com")));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnFallback(proxy_chain, net::OK);
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectTotalCount("PrefetchProxy.Proxy.Fallback.NetError", 0);
}

TEST_F(PrefetchProxyConfiguratorTest, TunnelHeaders_200OK) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrefetchUseContentRefactor);

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(prefetch_proxy_url()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy_chain, 0,
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK"));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 200, 1);
}

TEST_F(PrefetchProxyConfiguratorTest, TunnelHeaders_DifferentProxy) {
  base::HistogramTester histogram_tester;

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(GURL("http://foo.com")));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy_chain, 0,
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK"));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectTotalCount("PrefetchProxy.Proxy.RespCode", 0);
}

TEST_F(PrefetchProxyConfiguratorTest, TunnelHeaders_500NoRetryAfter) {
  base::HistogramTester histogram_tester;

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(prefetch_proxy_url()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy_chain, 0,
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 500 Internal Server Error"));
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 500, 1);

  FastForwardBy(base::Seconds(5 * 60 + 1));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyConfiguratorTest, TunnelHeaders_500WithRetryAfter) {
  base::HistogramTester histogram_tester;

  net::ProxyChain proxy_chain(
      net::GetSchemeFromUriScheme(prefetch_proxy_url().scheme()),
      net::HostPortPair::FromURL(prefetch_proxy_url()));

  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());

  configurator()->OnTunnelHeadersReceived(
      proxy_chain, 0,
      base::MakeRefCounted<
          net::HttpResponseHeaders>(net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 500 Internal Server Error\r\nRetry-After: 120\r\n\r\n")));
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());
  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 500, 1);

  FastForwardBy(base::Seconds(119));
  EXPECT_FALSE(configurator()->IsPrefetchProxyAvailable());

  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(configurator()->IsPrefetchProxyAvailable());
}

TEST_F(PrefetchProxyConfiguratorTest, ServerExperimentGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPrefetchUseContentRefactor,
      {{"server_experiment_group", "test_group"}});

  base::RunLoop loop;
  configurator()->UpdateCustomProxyConfig(loop.QuitClosure());
  loop.Run();

  net::HttpRequestHeaders headers;
  headers.SetHeader("chrome-tunnel",
                    "key=" + std::string(kApiKey) + ",exp=test_group");
  VerifyLatestProxyConfig(prefetch_proxy_url(), headers);
}

}  // namespace
}  // namespace content
