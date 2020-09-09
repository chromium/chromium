// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

// Test suite covering the interaction between browser bookmarks and
// `Sec-Fetch-*` headers that can't be covered by Web Platform Tests (yet).
// See https://mikewest.github.io/sec-metadata/#directly-user-initiated and
// https://github.com/web-platform-tests/wpt/issues/16019.
class SecFetchBrowserTest : public ContentBrowserTest {
 public:
  SecFetchBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server_.AddDefaultHandlers(GetTestDataFilePath());
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_test_server_.Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  void NavigateForHeader(const std::string& header) {
    std::string url = "/echoheader?";
    ASSERT_TRUE(
        NavigateToURL(shell(), https_test_server_.GetURL(url + header)));

    NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                         ui::PAGE_TRANSITION_TYPED));
  }

  GURL GetUrl(const std::string& path_and_query) {
    return https_test_server_.GetURL(path_and_query);
  }

  GURL GetSecFetchUrl() { return GetUrl("/echoheader?sec-fetch-site"); }

  std::string GetContent() {
    return EvalJs(shell(), "document.body.innerText").ExtractString();
  }

 private:
  net::EmbeddedTestServer https_test_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SecFetchBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SecFetchBrowserTest, TypedNavigation) {
  {
    // Sec-Fetch-Dest: document
    NavigateForHeader("Sec-Fetch-Dest");
    EXPECT_EQ("document", GetContent());
  }

  {
    // Sec-Fetch-Mode: navigate
    NavigateForHeader("Sec-Fetch-Mode");
    EXPECT_EQ("navigate", GetContent());
  }

  {
    // Sec-Fetch-Site: none
    NavigateForHeader("Sec-Fetch-Site");
    EXPECT_EQ("none", GetContent());
  }

  {
    // Sec-Fetch-User: ?1
    NavigateForHeader("Sec-Fetch-User");
    EXPECT_EQ("?1", GetContent());
  }
}

// Verify that cross-port navigations are treated as same-site by
// Sec-Fetch-Site.
IN_PROC_BROWSER_TEST_F(SecFetchBrowserTest, CrossPortNavigation) {
  net::EmbeddedTestServer server2(net::EmbeddedTestServer::TYPE_HTTPS);
  server2.AddDefaultHandlers(GetTestDataFilePath());
  server2.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(server2.Start());

  GURL initial_url = server2.GetURL("/title1.html");
  GURL final_url = GetSecFetchUrl();
  EXPECT_EQ(initial_url.scheme(), final_url.scheme());
  EXPECT_EQ(initial_url.host(), final_url.host());
  EXPECT_NE(initial_url.port(), final_url.port());

  // Navigate to (paraphrasing): https://foo.com:port1/...
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Navigate to (paraphrasing): https://foo.com:port2/...  when the navigation
  // is initiated from (paraphrasing): https://foo.com:port1/...
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    ASSERT_TRUE(ExecJs(shell(), JsReplace("location = $1", final_url)));
    nav_observer.Wait();
  }

  // Verify that https://foo.com:port1 is treated as same-site wrt
  // https://foo.com:port2.
  EXPECT_EQ("same-site", GetContent());
}

// This test verifies presence of a correct ("replayed") Sec-Fetch-Site HTTP
// request header in a history/back navigation.
//
// This is a regression test for https://crbug.com/946503.
//
// This test is slightly redundant with
// wpt/fetch/metadata/history.tentative.https.sub.html
// but it tests history navigations that are browser-initiated
// (e.g. as-if they were initiated by Chrome UI, not by javascript).
IN_PROC_BROWSER_TEST_F(SecFetchBrowserTest, BackNavigation) {
  // Start the test at |initial_url|.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url(GetUrl("/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Renderer-initiated navigation to same-origin |main_url|.
  GURL main_url(GetSecFetchUrl());
  EXPECT_EQ(url::Origin::Create(initial_url), url::Origin::Create(main_url));
  {
    TestNavigationObserver nav_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(ExecJs(shell(), JsReplace("window.location = $1", main_url)));
    nav_observer.Wait();
    EXPECT_EQ("same-origin", GetContent());
  }

  // Renderer-initiated navigation to |cross_origin_url|.
  {
    GURL cross_origin_url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_NE(url::Origin::Create(main_url),
              url::Origin::Create(cross_origin_url));
    TestNavigationObserver nav_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(
        ExecJs(shell(), JsReplace("window.location = $1", cross_origin_url)));
    nav_observer.Wait();
  }

  // Go back and verify that `Sec-Fetch-Site: same-origin` is again sent to the
  // server.
  {
    TestNavigationObserver nav_observer(shell()->web_contents(), 1);
    shell()->web_contents()->GetController().GoBack();
    nav_observer.Wait();
    EXPECT_EQ(main_url, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ("same-origin", GetContent());
  }
}

// This test verifies presence of a correct ("replayed") Sec-Fetch-Site HTTP
// request header in a history/reload navigation.
//
// This is a regression test for https://crbug.com/946503.
IN_PROC_BROWSER_TEST_F(SecFetchBrowserTest, ReloadNavigation) {
  // Start the test at |initial_url|.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url(GetUrl("/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Renderer-initiated navigation to same-origin |main_url|.
  GURL main_url(GetSecFetchUrl());
  EXPECT_EQ(url::Origin::Create(initial_url), url::Origin::Create(main_url));
  {
    TestNavigationObserver nav_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(ExecJs(shell(), JsReplace("window.location = $1", main_url)));
    nav_observer.Wait();
    EXPECT_EQ("same-origin", GetContent());
  }

  // Reload and verify that `Sec-Fetch-Site: same-origin` is again sent to the
  // server.
  {
    TestNavigationObserver nav_observer(shell()->web_contents(), 1);
    shell()->web_contents()->GetController().Reload(ReloadType::BYPASSING_CACHE,
                                                    true);
    nav_observer.Wait();
    EXPECT_EQ(main_url, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ("same-origin", GetContent());
  }
}

}  // namespace content
