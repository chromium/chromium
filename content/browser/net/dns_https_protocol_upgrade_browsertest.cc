// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
// Note that kHttpsOnlyDomain is covered by
// net::EmbeddedTestServer::CERT_TEST_NAMES.
const char kHttpsOnlyDomain[] = "a.test";
const char kRegularDomain[] = "http-ok.example";
}  // namespace

class DnsHttpsProtocolUpgradeBrowserTest : public content::ContentBrowserTest {
 public:
  DnsHttpsProtocolUpgradeBrowserTest() {
    features_.InitAndEnableFeatureWithParameters(
        net::features::kUseDnsHttpsSvcb,
        {{"UseDnsHttpsSvcbHttpUpgrade", "true"}});
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddSimulatedHTTPSServiceFormRecord(kHttpsOnlyDomain);
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(DnsHttpsProtocolUpgradeBrowserTest,
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

  EXPECT_TRUE(content::NavigateToURL(shell(), /*url=*/http_url,
                                     /*expected_commit_url=*/https_url));
}

IN_PROC_BROWSER_TEST_F(DnsHttpsProtocolUpgradeBrowserTest, NoProtocolUpgrade) {
  RegisterDefaultHandlers(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL http_url =
      embedded_test_server()->GetURL(kRegularDomain, "/defaultresponse");
  EXPECT_TRUE(http_url.SchemeIs(url::kHttpScheme));

  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  shell()->LoadURL(http_url);
  navigation_observer.WaitForNavigationFinished();

  EXPECT_TRUE(content::NavigateToURL(shell(), http_url));
}
