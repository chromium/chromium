// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

class PrerenderBrowserTest : public ContentBrowserTest {
 public:
  PrerenderBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
  }
  ~PrerenderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ssl_server_.RegisterRequestMonitor(base::BindRepeating(
        &PrerenderBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    ASSERT_TRUE(ssl_server_.Start());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    request_count_by_path_[request.GetURL().PathForRequest()]++;
  }

  PrerenderHostRegistry& GetPrerenderHostRegistry() {
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    return *storage_partition->GetPrerenderHostRegistry();
  }

  GURL GetUrl(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

  int GetRequestCount(const GURL& url) {
    return request_count_by_path_[url.PathForRequest()];
  }

 private:
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  // Counts of requests sent to the server. Keyed by path (not by full URL)
  // because the host part of the requests is translated ("a.test" to
  // "127.0.0.1") before the server handles them.
  std::map<std::string, int> request_count_by_path_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LinkRelPrerender) {
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Start watching new web contents to be created for prerendering.
  content::TestNavigationObserver navigation_observer(kPrerenderingUrl);
  navigation_observer.StartWatchingNewWebContents();

  // Navigate to a page that initiates prerendering for `kPrerenderingUrl`.
  ASSERT_TRUE(
      NavigateToURL(shell(), GetUrl("/prerender/single_prerender.html")));

  // Wait until the completion of prerendering.
  navigation_observer.Wait();

  // Check if a request is issued for `kPrerenderingUrl`.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Check if PrerenderHost for `kPrerenderingUrl` is registered in
  // PrerenderHostRegistry.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrl(kPrerenderingUrl), nullptr);

  // TODO(https://crbug.com/1132746): Test navigation to the prerendered page.
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LinkRelPrerender_Multiple) {
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // Start watching new web contents to be created for prerendering.
  content::TestNavigationObserver navigation_observer1(kPrerenderingUrl1);
  content::TestNavigationObserver navigation_observer2(kPrerenderingUrl2);
  navigation_observer1.StartWatchingNewWebContents();
  navigation_observer2.StartWatchingNewWebContents();

  // Navigate to a page that initiates prerendering for `kPrerenderingUrl1` and
  // `kPrerenderingUrl2`.
  ASSERT_TRUE(
      NavigateToURL(shell(), GetUrl("/prerender/multiple_prerenders.html")));

  // Wait until the completion of prerendering.
  navigation_observer1.Wait();
  navigation_observer2.Wait();

  // Check if requests are issued for `kPrerenderingUrl1` and
  // `kPrerenderingUrl2`.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);

  // Check if PrerenderHosts for `kPrerenderingUrl1` and `kPrerenderingUrl2` are
  // registered in PrerenderHostRegistry.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrl(kPrerenderingUrl1), nullptr);
  EXPECT_NE(registry.FindHostByUrl(kPrerenderingUrl2), nullptr);

  // TODO(https://crbug.com/1132746): Test navigation to one of the prerendered
  // pages.
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LinkRelPrerender_Duplicate) {
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // Start watching new web contents to be created for prerendering.
  content::TestNavigationObserver navigation_observer1(kPrerenderingUrl1);
  content::TestNavigationObserver navigation_observer2(kPrerenderingUrl2);
  navigation_observer1.StartWatchingNewWebContents();
  navigation_observer2.StartWatchingNewWebContents();

  // Navigate to a page that initiates prerendering for `kPrerenderingUrl1`
  // twice. The second prerendering request should be ignored.
  ASSERT_TRUE(
      NavigateToURL(shell(), GetUrl("/prerender/duplicate_prerenders.html")));

  // Wait until the completion of prerendering.
  navigation_observer1.Wait();
  navigation_observer2.Wait();

  // Check if requests are issued once per prerendering URL.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);

  // Check if PrerenderHosts for `kPrerenderingUrl1` and `kPrerenderingUrl2` are
  // registered in PrerenderHostRegistry.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrl(kPrerenderingUrl1), nullptr);
  EXPECT_NE(registry.FindHostByUrl(kPrerenderingUrl2), nullptr);

  // TODO(https://crbug.com/1132746): Test navigation to the prerendered page.
}

// TODO(https://crbug.com/1132746): Test canceling prerendering.

// TODO(https://crbug.com/1132746): Test prerendering for 404 page, redirection,
// auth error, cross origin, etc.

}  // namespace
}  // namespace content
