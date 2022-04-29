// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(https://crbug.com/1295770) Add tests that query a test Do53 server.

class DohHttpsProtocolUpgradeBrowserTest : public content::ContentBrowserTest {
 public:
  DohHttpsProtocolUpgradeBrowserTest() {
    features_.InitAndEnableFeatureWithParameters(
        net::features::kUseDnsHttpsSvcb,
        {{"UseDnsHttpsSvcbHttpUpgrade", "true"}});
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
    absl::optional<net::DnsOverHttpsConfig> doh_config =
        net::DnsOverHttpsConfig::FromString(doh_server_->GetTemplate());
    ASSERT_TRUE(doh_config.has_value());

    // When the network service runs out-of-process, use `BrowserTestBase`
    // methods to poke the DNS configuration.
    if (content::IsOutOfProcessNetworkService()) {
      SetTestDohConfig(net::SecureDnsMode::kSecure,
                       std::move(doh_config).value());
      SetReplaceSystemDnsConfig();
      return;
    }
    // When the network service runs in-process, we can talk to it directly.
    // Ideally, we would use Mojo to communicate with the network process
    // regardless of where it's running, but for reasons I do not understand,
    // Mojo messages seem to cause a deadlock when the network service is
    // in-process.
    //
    // TODO(https://crbug.com/1295732) Rely on `BrowserTestBase` to pass this
    // info to the network service via Mojo, regardless of where the network
    // service is running.
    base::RunLoop run_loop;
    content::GetNetworkTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&] {
          network::NetworkService* network_service =
              network::NetworkService::GetNetworkServiceForTesting();
          ASSERT_TRUE(network_service);
          network_service->SetTestDohConfigForTesting(
              net::SecureDnsMode::kSecure, doh_config.value());
          network_service->ReplaceSystemDnsConfigForTesting();
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  std::unique_ptr<net::TestDohServer> doh_server_;

  // Note that kHttpsOnlyDomain is covered by
  // net::EmbeddedTestServer::CERT_TEST_NAMES.
  static constexpr base::StringPiece kHttpsOnlyDomain = "a.test";
  static constexpr base::StringPiece kRegularDomain = "http-ok.example";

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
  const int num_queries_served = doh_server_->QueriesServed();
  EXPECT_TRUE(num_queries_served == 4 || num_queries_served == 6)
      << "Unexpected number of queries served: " << num_queries_served;
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
  const int num_queries_served = doh_server_->QueriesServed();
  EXPECT_TRUE(num_queries_served == 2 || num_queries_served == 3)
      << "Unexpected number of queries served: " << num_queries_served;
}
