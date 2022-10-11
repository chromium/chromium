// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/proxy_lookup_client_impl.h"

#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class TestNetworkContext : public network::TestNetworkContext {
 public:
  TestNetworkContext(bool call_on_proxy_lookup_complete,
                     absl::optional<net::ProxyInfo> proxy_info)
      : call_on_proxy_lookup_complete_(call_on_proxy_lookup_complete),
        proxy_info_(proxy_info) {}

  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          pending_proxy_lookup_client) override {
    mojo::Remote<network::mojom::ProxyLookupClient> proxy_lookup_client(
        std::move(pending_proxy_lookup_client));
    if (call_on_proxy_lookup_complete_)
      proxy_lookup_client->OnProxyLookupComplete(net::OK, proxy_info_);
  }

 private:
  // If false, then simulates the mojo pipe disconnecting before the result of
  // the proxy lookup is sent.
  bool call_on_proxy_lookup_complete_;
  absl::optional<net::ProxyInfo> proxy_info_;
};

class ProxyLookupClientImplTest : public ::testing::Test {};

TEST_F(ProxyLookupClientImplTest, NoProxyInfo) {
  BrowserTaskEnvironment task_environment;

  TestNetworkContext network_context(/*call_on_proxy_lookup_complete_=*/true,
                                     absl::nullopt);

  base::RunLoop run_loop;
  GURL test_url("example.com");
  net::SchemefulSite site(test_url);
  net::NetworkAnonymizationKey network_anonymization_key(site, site);
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client =
      std::make_unique<ProxyLookupClientImpl>(
          test_url, network_anonymization_key,
          base::BindOnce(
              [](base::RunLoop* run_loop, bool has_proxy) {
                EXPECT_FALSE(has_proxy);
                run_loop->Quit();
              },
              &run_loop),
          &network_context);
  run_loop.Run();
}

TEST_F(ProxyLookupClientImplTest, OnlyDirect) {
  BrowserTaskEnvironment task_environment;

  // A proxy info of DIRECT means that no proxy is used.
  net::ProxyInfo direct_proxy_info;
  direct_proxy_info.UseDirect();
  TestNetworkContext network_context(/*call_on_proxy_lookup_complete_=*/true,
                                     direct_proxy_info);

  base::RunLoop run_loop;
  GURL test_url("example.com");
  net::SchemefulSite site(test_url);
  net::NetworkAnonymizationKey network_anonymization_key(site, site);
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client =
      std::make_unique<ProxyLookupClientImpl>(
          test_url, network_anonymization_key,
          base::BindOnce(
              [](base::RunLoop* run_loop, bool has_proxy) {
                EXPECT_FALSE(has_proxy);
                run_loop->Quit();
              },
              &run_loop),
          &network_context);
  run_loop.Run();
}

TEST_F(ProxyLookupClientImplTest, Proxy) {
  BrowserTaskEnvironment task_environment;

  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("proxy.com");
  TestNetworkContext network_context(/*call_on_proxy_lookup_complete_=*/true,
                                     proxy_info);

  base::RunLoop run_loop;
  GURL test_url("example.com");
  net::SchemefulSite site(test_url);
  net::NetworkAnonymizationKey network_anonymization_key(site, site);
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client =
      std::make_unique<ProxyLookupClientImpl>(
          test_url, network_anonymization_key,
          base::BindOnce(
              [](base::RunLoop* run_loop, bool has_proxy) {
                EXPECT_TRUE(has_proxy);
                run_loop->Quit();
              },
              &run_loop),
          &network_context);
  run_loop.Run();
}

TEST_F(ProxyLookupClientImplTest, Disconnect) {
  BrowserTaskEnvironment task_environment;

  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("proxy.com");
  TestNetworkContext network_context(/*call_on_proxy_lookup_complete_=*/false,
                                     proxy_info);

  base::RunLoop run_loop;
  GURL test_url("example.com");
  net::SchemefulSite site(test_url);
  net::NetworkAnonymizationKey network_anonymization_key(site, site);
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client =
      std::make_unique<ProxyLookupClientImpl>(
          test_url, network_anonymization_key,
          base::BindOnce(
              [](base::RunLoop* run_loop, bool has_proxy) {
                // If the mojo pipe disconnects, then result should be false.
                EXPECT_FALSE(has_proxy);
                run_loop->Quit();
              },
              &run_loop),
          &network_context);
  run_loop.Run();
}

}  // namespace
}  // namespace content