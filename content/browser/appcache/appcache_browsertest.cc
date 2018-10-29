// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
namespace content {

// This class currently enables the network service feature, which allows us to
// test the AppCache code in that mode.
class AppCacheNetworkServiceBrowserTest : public ContentBrowserTest {
 public:
  AppCacheNetworkServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kNetworkService);
  }

  ~AppCacheNetworkServiceBrowserTest() override {}

  // Handler to count the number of requests.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    request_count_++;
    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  // Call this to reset the request_count_.
  void Clear() {
    request_count_ = 0;
  }

  int request_count() const { return request_count_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Tracks the number of requests.
  int request_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppCacheNetworkServiceBrowserTest);
};

// The network service process launch DCHECK's on Android. The bug
// here http://crbug.com/748764. It looks like unsandboxed utility
// processes are not supported on Android.
#if !defined(OS_ANDROID)
// This test validates that navigating to a TLD which has an AppCache
// associated with it and then navigating to another TLD within that
// host clears the previously registered factory. We verify this by
// validating that request count for the last navigation.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       VerifySubresourceFactoryClearedOnNewNavigation) {
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server(
      new net::EmbeddedTestServer());

  embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&AppCacheNetworkServiceBrowserTest::HandleRequest,
                          base::Unretained(this)));

  base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
  embedded_test_server->AddDefaultHandlers(content_test_data);

  ASSERT_TRUE(embedded_test_server->Start());

  GURL main_url =
      embedded_test_server->GetURL("/appcache/simple_page_with_manifest.html");

  base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");

  // Load the main page twice. The second navigation should have AppCache
  // initialized for the page.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(main_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  Clear();
  GURL page_no_manifest =
      embedded_test_server->GetURL("/appcache/simple_page_no_manifest.html");

  EXPECT_TRUE(NavigateToURL(shell(), page_no_manifest));
  // We expect two requests for simple_page_no_manifest.html. The request
  // for the main page and the logo.
  EXPECT_GT(request_count(), 1);
  EXPECT_EQ(page_no_manifest, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}
#endif

}  // namespace content
