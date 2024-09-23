// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/test_doh_server.h"
#include "services/network/network_service.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(crbug.com/40214469) Add tests that query a test Do53 server.

class DohHttpsProtocolUpgradeBrowserTest : public content::ContentBrowserTest {
 public:
  DohHttpsProtocolUpgradeBrowserTest() {
    features_.InitAndEnableFeatureWithParameters(
        net::features::kUseDnsHttpsSvcb,
        {// Disable timeouts.
         {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
         {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
         {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
         {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
         {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
         {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});
  }

 protected:
  void SetUp() override { content::ContentBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    static constexpr char kDohServerHostname[] = "doh-server.example";

    doh_server_ = std::make_unique<net::TestDohServer>();
    doh_server_->SetHostname(kDohServerHostname);
    ASSERT_TRUE(doh_server_->Start());

    // Add a single bootstrapping rule so we can resolve the DoH server.
    host_resolver()->AddRule(kDohServerHostname, "127.0.0.1");

    // Build a DoH config that points to our one DoH server and pass the config
    // into the network service.
    std::optional<net::DnsOverHttpsConfig> doh_config =
        net::DnsOverHttpsConfig::FromString(doh_server_->GetTemplate());
    ASSERT_TRUE(doh_config.has_value());
    SetTestDohConfig(net::SecureDnsMode::kSecure,
                     std::move(doh_config).value());
    SetReplaceSystemDnsConfig();
  }

  std::unique_ptr<net::TestDohServer> doh_server_;

  // Note that kHttpsOnlyDomain is covered by
  // net::EmbeddedTestServer::CERT_TEST_NAMES.
  static constexpr std::string_view kHttpsOnlyDomain = "a.test";
  static constexpr std::string_view kRegularDomain = "http-ok.example";

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(DohHttpsProtocolUpgradeBrowserTest,
                       HttpsProtocolUpgrade) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  const GURL https_url =
      https_server.GetURL(kHttpsOnlyDomain, "/defaultresponse");
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  const GURL http_url = https_url.ReplaceComponents(replacements);

  doh_server_->AddAddressRecord(kHttpsOnlyDomain,
                                net::IPAddress::IPv4Localhost());
  doh_server_->AddRecord(net::BuildTestHttpsServiceRecord(
      net::dns_util::GetNameForHttpsQuery(url::SchemeHostPort(https_url)),
      /*priority=*/1, /*service_name=*/".", /*params=*/{}));

  EXPECT_TRUE(content::NavigateToURL(shell(), /*url=*/http_url,
                                     /*expected_commit_url=*/https_url));

  // A, AAAA, and HTTPS for the initial resolution, and then again after the
  // protocol upgrade. Note that the AAAA query may be disabled based on IPv6
  // connectivity.
  EXPECT_THAT(doh_server_->QueriesServedForSubdomains(kHttpsOnlyDomain),
              testing::AnyOf(4, 6));
}

IN_PROC_BROWSER_TEST_F(DohHttpsProtocolUpgradeBrowserTest, NoProtocolUpgrade) {
  RegisterDefaultHandlers(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL http_url =
      embedded_test_server()->GetURL(kRegularDomain, "/defaultresponse");
  EXPECT_TRUE(http_url.SchemeIs(url::kHttpScheme));

  doh_server_->AddAddressRecord(kRegularDomain,
                                net::IPAddress::IPv4Localhost());

  EXPECT_TRUE(content::NavigateToURL(shell(), http_url));

  // A, AAAA, and HTTPS for the host resolution. These queries will not be
  // repeated because we do not expect a protocol upgrade. Note that the AAAA
  // query may be disabled based on IPv6 connectivity.
  EXPECT_THAT(doh_server_->QueriesServedForSubdomains(kRegularDomain),
              testing::AnyOf(2, 3));
}
