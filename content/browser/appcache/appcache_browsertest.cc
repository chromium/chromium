// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

// This class currently enables the network service feature, which allows us to
// test the AppCache code in that mode.
class AppCacheNetworkServiceBrowserTest : public ContentBrowserTest {
 public:
  AppCacheNetworkServiceBrowserTest() {}

  ~AppCacheNetworkServiceBrowserTest() override = default;

 private:

  DISALLOW_COPY_AND_ASSIGN(AppCacheNetworkServiceBrowserTest);
};

// This test validates that navigating to a TLD which has an AppCache
// associated with it and then navigating to another TLD within that
// host clears the previously registered factory. We verify this by
// validating that request count for the last navigation.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       VerifySubresourceFactoryClearedOnNewNavigation) {
  net::EmbeddedTestServer embedded_test_server;

  int request_count = 0;
  embedded_test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        ++request_count;
        return nullptr;
      }));

  embedded_test_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());

  ASSERT_TRUE(embedded_test_server.Start());

  GURL main_url =
      embedded_test_server.GetURL("/appcache/simple_page_with_manifest.html");

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

  request_count = 0;
  GURL page_no_manifest =
      embedded_test_server.GetURL("/appcache/simple_page_no_manifest.html");

  EXPECT_TRUE(NavigateToURL(shell(), page_no_manifest));
  // We expect two requests for simple_page_no_manifest.html. The request
  // for the main page and the logo.
  EXPECT_GT(request_count, 1);
  EXPECT_EQ(page_no_manifest, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// Regression test for crbug.com/937761.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       SSLCertificateCachedCorrectly) {
  net::EmbeddedTestServer embedded_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  embedded_test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                    net::SSLServerConfig());
  embedded_test_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(embedded_test_server.Start());

  GURL main_url =
      embedded_test_server.GetURL("/appcache/simple_page_with_manifest.html");
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

  content::NavigationEntry* const entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_FALSE(net::IsCertStatusError(entry->GetSSL().cert_status));
  EXPECT_TRUE(entry->GetSSL().certificate);
}

// Regression test for crbug.com/968179.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       CacheableResourcesReuse) {
  net::EmbeddedTestServer embedded_test_server;

  std::string manifest_nonce = "# Version 1";
  int resource_request_count = 0;
  embedded_test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().path() != "/appcache/cache_reuse.manifest") {
          ++resource_request_count;
          return nullptr;
        }

        // Return a dynamically generated manifest, to trigger AppCache updates.
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_content_type("text/cache-manifest");
        http_response->set_content(base::StrCat({
            "CACHE MANIFEST\n",
            manifest_nonce,
            "\n/appcache/cache_reuse.html\n",
        }));
        return http_response;
      }));

  embedded_test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                    net::SSLServerConfig());
  embedded_test_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(embedded_test_server.Start());

  GURL main_url = embedded_test_server.GetURL("/appcache/cache_reuse.html");

  // First navigation populates AppCache.
  {
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    base::string16 expected_title = base::ASCIIToUTF16("AppCache primed");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Flush the HTTP cache so cache_reuse.html won't be served from there.
  base::RunLoop run_loop;
  content::StoragePartition* storage_partition = shell()
                                                     ->web_contents()
                                                     ->GetMainFrame()
                                                     ->GetProcess()
                                                     ->GetStoragePartition();
  storage_partition->GetNetworkContext()->ClearHttpCache(
      base::Time(), base::Time::Max(), nullptr, run_loop.QuitClosure());
  run_loop.Run();

  // Second navigation triggers an AppCache update.
  resource_request_count = 0;
  manifest_nonce = "# Version 2";
  {
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // The AppCache update should only reload the manifest.
  EXPECT_EQ(0, resource_request_count);
}

}  // namespace content
