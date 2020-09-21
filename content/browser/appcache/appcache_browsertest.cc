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
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
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
  AppCacheNetworkServiceBrowserTest() = default;

  ~AppCacheNetworkServiceBrowserTest() override = default;

 private:

  DISALLOW_COPY_AND_ASSIGN(AppCacheNetworkServiceBrowserTest);
};

// This test validates that navigating to a TLD which has an AppCache
// associated with it and then navigating to another TLD within that
// host clears the previously registered factory. We verify this by
// validating that request count for the last navigation.
// Flaky on linux-chromeos-chrome (http://crbug.com/1042385)
IN_PROC_BROWSER_TEST_F(
    AppCacheNetworkServiceBrowserTest,
    DISABLED_VerifySubresourceFactoryClearedOnNewNavigation) {
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
// Disabled due to flakiness https://crbug.com/1031090.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       DISABLED_SSLCertificateCachedCorrectly) {
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
// Timedout on all platforms.  http://crbug.com/1031089
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       DISABLED_CacheableResourcesReuse) {
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

// Client which intercepts all requests using WillCreateURLLoaderFactory() and
// logs them in intercepted_request_map().
class LoaderFactoryInterceptingBrowserClient : public TestContentBrowserClient {
 public:
  bool WillCreateURLLoaderFactory(
      BrowserContext* browser_context,
      RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      base::Optional<int64_t> navigation_id,
      base::UkmSourceId ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override {
    auto proxied_receiver = std::move(*factory_receiver);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
    *factory_receiver = target_factory_remote.InitWithNewPipeAndPassReceiver();

    proxies_.push_back(std::make_unique<PassThroughURLLoaderFactory>(
        std::move(proxied_receiver), std::move(target_factory_remote),
        request_initiator, this));

    return true;
  }

  const std::map<GURL, url::Origin>& intercepted_request_map() const {
    return intercepted_request_map_;
  }

 private:
  class PassThroughURLLoaderFactory : public network::mojom::URLLoaderFactory {
   public:
    PassThroughURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
        mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
        const url::Origin& request_initiator,
        LoaderFactoryInterceptingBrowserClient* client)
        : target_factory_(std::move(target_factory)),
          request_initiator_(request_initiator),
          client_(client) {
      proxy_receivers_.Add(this, std::move(receiver));
    }

    void CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      client_->intercepted_request_map_[request.url] = request_initiator_;
      target_factory_->CreateLoaderAndStart(
          std::move(loader_receiver), routing_id, request_id, options, request,
          std::move(client), traffic_annotation);
    }

    void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                   loader_receiver) override {
      proxy_receivers_.Add(this, std::move(loader_receiver));
    }

   private:
    mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
    url::Origin request_initiator_;
    LoaderFactoryInterceptingBrowserClient* client_;
    mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  };

  std::map<GURL, url::Origin> intercepted_request_map_;
  std::vector<std::unique_ptr<PassThroughURLLoaderFactory>> proxies_;
};

// Timeout waiting for "AppCache updated" title (http://crbug.com/1080708).
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       DISABLED_AppCacheRequestsAreProxied) {
  LoaderFactoryInterceptingBrowserClient browser_client;
  ContentBrowserClient* original_client =
      SetBrowserClientForTesting(&browser_client);
  net::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());

  ASSERT_TRUE(embedded_test_server.Start());

  GURL main_url =
      embedded_test_server.GetURL("/appcache/simple_page_with_manifest.html");

  base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  const auto& request_map = browser_client.intercepted_request_map();
  EXPECT_EQ(request_map.count(main_url), 1u);

  GURL subresource_url = embedded_test_server.GetURL("/appcache/logo.png");
  EXPECT_EQ(request_map.count(subresource_url), 1u);
  EXPECT_EQ(request_map.find(subresource_url)->second,
            url::Origin::Create(embedded_test_server.base_url()));

  GURL subresource_url2 = embedded_test_server.GetURL("/appcache/logo2.png");
  EXPECT_EQ(request_map.count(subresource_url2), 0u);

  const char kScript[] = R"(
      new Promise(function (resolve, reject) {
          var img = document.createElement('img');
          img.src = '/appcache/logo2.png';
          img.onload = _ => resolve('IMG LOADED');
          img.onerror = reject;
      })
  )";
  EXPECT_EQ("IMG LOADED", EvalJs(shell(), kScript));

  EXPECT_EQ(request_map.count(subresource_url2), 1u);
  EXPECT_EQ(request_map.find(subresource_url2)->second,
            url::Origin::Create(embedded_test_server.base_url()));

  SetBrowserClientForTesting(original_client);
}

}  // namespace content
