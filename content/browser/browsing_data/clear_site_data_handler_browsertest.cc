// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/browser/browsing_data/shared_storage_clear_site_data_tester.h"
#include "content/browser/browsing_data/storage_bucket_clear_site_data_tester.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"

using testing::_;

namespace content {

namespace {

// Adds a key=value pair to the url's query.
void AddQuery(GURL* url, const std::string& key, const std::string& value) {
  *url = GURL(url->spec() + (url->has_query() ? "&" : "?") + key + "=" +
              base::EscapeQueryParamValue(value, false));
}

// A helper function to synchronize with JS side of the tests. JS can append
// information to the loaded website's title and C++ will wait until that
// happens.
void WaitForTitle(const Shell* shell, const char* expected_title) {
  std::u16string expected_title_16 = base::ASCIIToUTF16(expected_title);
  TitleWatcher title_watcher(shell->web_contents(), expected_title_16);
  ASSERT_EQ(expected_title_16, title_watcher.WaitAndGetTitle());
}

// A value of the Clear-Site-Data header that requests cookie deletion. Reused
// in tests that need a valid header but do not depend on its value.
static const char* kClearCookiesHeader = "\"cookies\"";

// For use with TestBrowsingDataRemoverDelegate::ExpectClearSiteDataCall.
enum class SetStorageKey { kYes, kNo };

// A helper class to observe BrowsingDataRemover deletion tasks coming from
// ClearSiteData.
class TestBrowsingDataRemoverDelegate : public MockBrowsingDataRemoverDelegate {
 public:
  // TODO(crbug.com/328043119): Remove code associated with
  // kAncestorChainBitEnabledInPartitionedCookies after it's enabled by default.
  TestBrowsingDataRemoverDelegate() {
    feature_list_.InitAndEnableFeature(
        net::features::kAncestorChainBitEnabledInPartitionedCookies);
  }
  // Sets a test expectation that a Clear-Site-Data header call from |origin|
  // (under |top_level_site|) instructing to delete |cookies|, |storage|, and
  // |cache|, will schedule the corresponding BrowsingDataRemover deletion
  // tasks. If |set_storage_key|=kYes (the default) then a storage key will be
  // set on the filter builder.
  //
  // When `override_partition_key_cross_site` is true, it indicates that the
  // expected value of the ancestor chain bit does not align with boolean value
  // that comes from comparing the origin and the top_level_site and the value
  // should indicate cross-site. This can occur on redirects as well as A->B->A
  // cases where the top_level_site is the same as the origin but are cross-site
  // because of the B in the chain.

  void ExpectClearSiteDataCall(
      const StoragePartitionConfig& storage_partition_config,
      const url::Origin& origin,
      const net::SchemefulSite& top_level_site,
      bool cookies,
      bool storage,
      bool cache,
      bool override_partition_key_cross_site = false,
      SetStorageKey set_storage_key = SetStorageKey::kYes) {
    const uint64_t kOriginTypeMask =
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
        BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    bool partition_key_cross_site =
        override_partition_key_cross_site ||
        net::SchemefulSite(origin) != top_level_site;

    if (cookies) {
      uint64_t data_type_mask =
          BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS;
      net::CookiePartitionKey::AncestorChainBit ancestor_chain_bit =
          net::CookiePartitionKey::BoolToAncestorChainBit(
              partition_key_cross_site);
      BrowsingDataFilterBuilderImpl filter_builder(
          BrowsingDataFilterBuilder::Mode::kDelete);
      filter_builder.AddRegisterableDomain(origin.host());
      filter_builder.SetStoragePartitionConfig(storage_partition_config);
      filter_builder.SetCookiePartitionKeyCollection(
          net::CookiePartitionKeyCollection::FromOptional(
              net::CookiePartitionKey::FromStorageKeyComponents(
                  top_level_site, ancestor_chain_bit, /*nonce=*/std::nullopt)));

      ExpectCall(base::Time(), base::Time::Max(), data_type_mask,
                 kOriginTypeMask, &filter_builder);
    }
    if (storage || cache) {
      uint64_t data_type_mask =
          (storage ? BrowsingDataRemover::DATA_TYPE_DOM_STORAGE |
                         BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX
                   : 0) |
          (cache ? BrowsingDataRemover::DATA_TYPE_CACHE : 0);
      data_type_mask &=
          ~BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX_INTERNAL;

      BrowsingDataFilterBuilderImpl filter_builder(
          BrowsingDataFilterBuilder::Mode::kDelete);
      filter_builder.AddOrigin(origin);
      filter_builder.SetStoragePartitionConfig(storage_partition_config);
      if (set_storage_key == SetStorageKey::kYes) {
        filter_builder.SetStorageKey(blink::StorageKey::Create(
            origin, top_level_site,
            partition_key_cross_site
                ? blink::mojom::AncestorChainBit::kCrossSite
                : blink::mojom::AncestorChainBit::kSameSite));
      }

      ExpectCall(base::Time(), base::Time::Max(), data_type_mask,
                 kOriginTypeMask, &filter_builder);
    }
  }

  // A shortcut for the above method, but with only cookies deleted, and
  // |origin|'s site is used as |top_level_site| if omitted. This is useful for
  // most tests that use |kClearCookiesHeader|.
  //
  // When `override_partition_key_cross_site` is true, it indicates that the
  // expected value of the ancestor chain bit does not align with boolean value
  // that comes from comparing the origin and the top_level_site and the value
  // should indicate cross-site. This can occur on redirects as well as A->B->A
  // cases where the top_level_site is the same as the origin but are cross-site
  // because of the B in the chain.
  void ExpectClearSiteDataCookiesCall(
      const StoragePartitionConfig& storage_partition_config,
      const url::Origin& origin,
      bool override_partition_key_cross_site = false,
      base::optional_ref<const net::SchemefulSite> top_level_site =
          base::optional_ref<const net::SchemefulSite>()) {
    ExpectClearSiteDataCall(storage_partition_config, origin,
                            top_level_site.has_value()
                                ? top_level_site.value()
                                : net::SchemefulSite(origin),
                            /*cookies=*/true,
                            /*storage=*/false,
                            /*cache=*/false, override_partition_key_cross_site);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

class ClearSiteDataHandlerBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    browsing_data_browsertest_utils::SetIgnoreCertificateErrors(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    browser_context()->GetBrowsingDataRemover()->SetEmbedderDelegate(
        &embedder_delegate_);

    // Set up HTTP and HTTPS test servers that handle all hosts.
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService())
      browsing_data_browsertest_utils::SetUpMockCertVerifier(net::OK);

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ClearSiteDataHandlerBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up HTTPS server.
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&ClearSiteDataHandlerBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  StoragePartition* storage_partition() {
    return browser_context()->GetDefaultStoragePartition();
  }

  const StoragePartitionConfig& storage_partition_config() {
    return storage_partition()->GetConfig();
  }

  // Adds a cookie for the |url|. Used in the cookie integration tests.
  void AddCookie(const GURL& url,
                 const std::optional<net::CookiePartitionKey>&
                     cookie_partition_key = std::nullopt) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();

    std::string cookie_line = "A=1";
    if (cookie_partition_key) {
      cookie_line += "; Secure; Partitioned";
    }
    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateForTesting(
            url, cookie_line, base::Time::Now(), /*server_time=*/std::nullopt,
            cookie_partition_key));

    base::RunLoop run_loop;
    cookie_manager->SetCanonicalCookie(
        *cookie, url, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(&ClearSiteDataHandlerBrowserTest::AddCookieCallback,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Retrieves the list of all cookies. Used in the cookie integration tests.
  net::CookieList GetCookies() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();
    base::RunLoop run_loop;
    net::CookieList cookie_list;
    cookie_manager->GetAllCookies(
        base::BindOnce(&ClearSiteDataHandlerBrowserTest::GetCookiesCallback,
                       run_loop.QuitClosure(), base::Unretained(&cookie_list)));
    run_loop.Run();
    return cookie_list;
  }

  void CreateCacheEntry(const GURL& url) {
    ASSERT_EQ(net::OK,
              LoadBasicRequest(storage_partition()->GetNetworkContext(), url));
  }

  bool TestCacheEntry(const GURL& url) {
    return LoadBasicRequest(storage_partition()->GetNetworkContext(), url,
                            net::LOAD_ONLY_FROM_CACHE) == net::OK;
  }

  GURL GetURLForHTTPSHost1(const std::string& relative_url) {
    return https_server_->GetURL("origin1.com", relative_url);
  }

  GURL GetURLForHTTPSHost2(const std::string& relative_url) {
    return https_server_->GetURL("origin2.com", relative_url);
  }

  TestBrowsingDataRemoverDelegate* delegate() { return &embedder_delegate_; }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  // Set a Clear-Site-Data header that |HandleRequest| will use for every
  // following request.
  void SetClearSiteDataHeader(const std::string& header) {
    base::AutoLock lock(clear_site_data_header_lock_);
    clear_site_data_header_ = header;
  }

  bool RunScriptAndGetBool(const std::string& script) {
    return EvalJs(shell()->web_contents(), script).ExtractBool();
  }

 private:
  // Handles all requests.
  //
  // Supports the following <key>=<value> query parameters in the url:
  // <key>="header"       responds with the header "Clear-Site-Data: <value>"
  // <key>="redirect"     responds with a redirect to the url <value>
  // <key>="html"         responds with a text/html content <value>
  // <key>="file"         responds with the content of file <value>
  //
  // Example: "https://localhost/?header={}&redirect=example.com" will respond
  // with headers
  // Clear-Site-Data: {}
  // Location: example.com
  //
  // Example: "https://localhost/?html=<html><head></head><body></body></html>"
  // will respond with the header
  // Content-Type: text/html
  // and content
  // <html><head></head><body></body></html>
  //
  // Example: "https://localhost/?file=file.html"
  // will respond with the header
  // Content-Type: text/html
  // and content from the file content/test/data/file.html
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());

    {
      base::AutoLock lock(clear_site_data_header_lock_);
      if (!clear_site_data_header_.empty())
        response->AddCustomHeader("Clear-Site-Data", clear_site_data_header_);
    }

    std::string value;
    if (net::GetValueForKeyInQuery(request.GetURL(), "header", &value))
      response->AddCustomHeader("Clear-Site-Data", value);

    if (net::GetValueForKeyInQuery(request.GetURL(), "redirect", &value)) {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", value);
    } else {
      response->set_code(net::HTTP_OK);
    }

    if (net::GetValueForKeyInQuery(request.GetURL(), "html", &value)) {
      response->set_content_type("text/html");
      response->set_content(value);

      // The "html" parameter is telling the server what to serve, and the XSS
      // auditor will complain if its |value| contains JS code. Disable that
      // protection.
      response->AddCustomHeader("X-XSS-Protection", "0");
    }

    if (net::GetValueForKeyInQuery(request.GetURL(),
                                   "access-control-allow-origin", &value)) {
      response->AddCustomHeader("Access-Control-Allow-Origin", value);
      response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
    }

    browsing_data_browsertest_utils::SetResponseContent(request.GetURL(),
                                                        &value, response.get());

    if (base::StartsWith(request.relative_url, "/cachetime",
                         base::CompareCase::SENSITIVE)) {
      response->set_content(
          "<html><head><title>Cache: max-age=60</title></head></html>");
      response->set_content_type("text/html");
      response->AddCustomHeader("Cache-Control", "max-age=60");
    }

    return std::move(response);
  }

  // Callback handler for AddCookie().
  static void AddCookieCallback(base::OnceClosure callback,
                                net::CookieAccessResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ASSERT_TRUE(result.status.IsInclude());
    std::move(callback).Run();
  }

  // Callback handler for GetCookies().
  static void GetCookiesCallback(base::OnceClosure callback,
                                 net::CookieList* out_cookie_list,
                                 const net::CookieList& cookie_list) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    *out_cookie_list = cookie_list;
    std::move(callback).Run();
  }

  // If this is set, |HandleRequest| will always respond with Clear-Site-Data.
  base::Lock clear_site_data_header_lock_;
  std::string clear_site_data_header_ GUARDED_BY(clear_site_data_header_lock_);

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  TestBrowsingDataRemoverDelegate embedder_delegate_;
};

// Tests that the header is recognized on the beginning, in the middle, and on
// the end of a navigation redirect chain. Each of the three parts of the chain
// may or may not send the header, so there are 8 configurations to test.

// Crashes on Win only. https://crbug.com/741189
#if BUILDFLAG(IS_WIN)
#define MAYBE_RedirectNavigation DISABLED_RedirectNavigation
#else
#define MAYBE_RedirectNavigation RedirectNavigation
#endif
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       MAYBE_RedirectNavigation) {
  std::array<GURL, 3> page_urls = {
      https_server()->GetURL("origin1.com", "/"),
      https_server()->GetURL("origin2.com", "/foo/bar"),
      https_server()->GetURL("origin3.com", "/index.html"),
  };

  // Iterate through the configurations. URLs whose index is matched by the mask
  // will send the header, the others won't.
  for (int mask = 0; mask < (1 << 3); ++mask) {
    std::array<GURL, 3> urls;

    // Set up the expectations.
    for (int i = 0; i < 3; ++i) {
      urls[i] = page_urls[i];
      if (mask & (1 << i))
        AddQuery(&urls[i], "header", kClearCookiesHeader);

      if (mask & (1 << i))
        delegate()->ExpectClearSiteDataCookiesCall(
            storage_partition_config(), url::Origin::Create(urls[i]),
            /*override_partition_key_cross_site=*/false);
    }

    // Set up redirects between urls 0 --> 1 --> 2.
    AddQuery(&urls[1], "redirect", urls[2].spec());
    AddQuery(&urls[0], "redirect", urls[1].spec());

    // Navigate to the first url of the redirect chain.
    EXPECT_TRUE(
        NavigateToURL(shell(), urls[0], urls[2] /* expected_commit_url */));

    // We reached the end of the redirect chain.
    EXPECT_EQ(urls[2], shell()->web_contents()->GetLastCommittedURL());

    delegate()->VerifyAndClearExpectations();
  }
}

// Tests that the header is recognized on the beginning, in the middle, and on
// the end of a resource load redirect chain. Each of the three parts of the
// chain may or may not send the header, so there are 8 configurations to test.

// Crashes on Win only. https://crbug.com/741189
#if BUILDFLAG(IS_WIN)
#define MAYBE_RedirectResourceLoad DISABLED_RedirectResourceLoad
#else
#define MAYBE_RedirectResourceLoad RedirectResourceLoad
#endif
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       MAYBE_RedirectResourceLoad) {
  std::array<GURL, 3> resource_urls = {
      https_server()->GetURL("origin1.com", "/redirect-start"),
      https_server()->GetURL("origin2.com", "/redirect-middle"),
      https_server()->GetURL("origin3.com", "/redirect-end"),
  };

  // Iterate through the configurations. URLs whose index is matched by the mask
  // will send the header, the others won't.
  for (int mask = 0; mask < (1 << 3); ++mask) {
    std::array<GURL, 3> urls;

    // Set up the expectations.
    GURL page_with_image = https_server()->GetURL("origin4.com", "/index.html");
    for (int i = 0; i < 3; ++i) {
      urls[i] = resource_urls[i];
      if (mask & (1 << i))
        AddQuery(&urls[i], "header", kClearCookiesHeader);

      if (mask & (1 << i))
        delegate()->ExpectClearSiteDataCookiesCall(
            storage_partition_config(), url::Origin::Create(urls[i]),
            /*override_partition_key_cross_site=*/true,
            net::SchemefulSite(page_with_image));
    }

    // Set up redirects between urls 0 --> 1 --> 2.
    AddQuery(&urls[1], "redirect", urls[2].spec());
    AddQuery(&urls[0], "redirect", urls[1].spec());

    // Navigate to a page that embeds "https://origin1.com/redirect-start"
    // and observe the loading of that resource.
    std::string content_with_image =
        "<html><head></head><body>"
        "<img src=\"" +
        urls[0].spec() +
        "\" />"
        "</body></html>";
    AddQuery(&page_with_image, "html", content_with_image);
    EXPECT_TRUE(NavigateToURL(shell(), page_with_image));

    delegate()->VerifyAndClearExpectations();
  }
}

// Tests that the Clear-Site-Data header is ignored for insecure origins.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest, InsecureNavigation) {
  // ClearSiteData() should not be called on HTTP.
  GURL url = embedded_test_server()->GetURL("example.com", "/");
  AddQuery(&url, "header", kClearCookiesHeader);
  ASSERT_FALSE(url.SchemeIsCryptographic());

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // We do not expect any calls to have been made.
  delegate()->VerifyAndClearExpectations();
}

class ClearSiteDataHandlerBrowserTestWithAutoupgradesDisabled
    : public ClearSiteDataHandlerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ClearSiteDataHandlerBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Tests that the Clear-Site-Data header is honored for secure resource loads
// and ignored for insecure ones.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTestWithAutoupgradesDisabled,
                       SecureAndInsecureResourceLoad) {
  GURL insecure_image =
      embedded_test_server()->GetURL("example.com", "/image.png");
  GURL secure_image = https_server()->GetURL("example.com", "/image.png");

  ASSERT_TRUE(secure_image.SchemeIsCryptographic());
  ASSERT_FALSE(insecure_image.SchemeIsCryptographic());

  AddQuery(&secure_image, "header", kClearCookiesHeader);
  AddQuery(&insecure_image, "header", kClearCookiesHeader);

  std::string content_with_insecure_image =
      "<html><head></head><body>"
      "<img src=\"" +
      insecure_image.spec() +
      "\" />"
      "</body></html>";

  std::string content_with_secure_image =
      "<html><head></head><body>"
      "<img src=\"" +
      secure_image.spec() +
      "\" />"
      "</body></html>";

  // Test insecure resources.
  GURL insecure_page = embedded_test_server()->GetURL("example.com", "/");
  GURL secure_page = https_server()->GetURL("example.com", "/");

  AddQuery(&insecure_page, "html", content_with_insecure_image);
  AddQuery(&secure_page, "html", content_with_insecure_image);

  // Insecure resource on an insecure page does not execute Clear-Site-Data.
  EXPECT_TRUE(NavigateToURL(shell(), insecure_page));

  // Insecure resource on a secure page does not execute Clear-Site-Data.
  EXPECT_TRUE(NavigateToURL(shell(), secure_page));

  // We do not expect any calls to have been made.
  delegate()->VerifyAndClearExpectations();

  // Test secure resources.
  insecure_page = embedded_test_server()->GetURL("example.com", "/");
  secure_page = https_server()->GetURL("example.com", "/");

  AddQuery(&insecure_page, "html", content_with_secure_image);
  AddQuery(&secure_page, "html", content_with_secure_image);

  // Secure resource on an insecure page does execute Clear-Site-Data.
  delegate()->ExpectClearSiteDataCookiesCall(storage_partition_config(),
                                             url::Origin::Create(secure_image));

  EXPECT_TRUE(NavigateToURL(shell(), secure_page));
  delegate()->VerifyAndClearExpectations();

  // Secure resource on a secure page does execute Clear-Site-Data.
  delegate()->ExpectClearSiteDataCookiesCall(storage_partition_config(),
                                             url::Origin::Create(secure_image));

  EXPECT_TRUE(NavigateToURL(shell(), secure_page));
  delegate()->VerifyAndClearExpectations();
}

// Tests that the Clear-Site-Data header is ignored for service worker resource
// loads.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest, ServiceWorker) {
  GURL origin1 = https_server()->GetURL("origin1.com", "/");
  GURL origin2 = https_server()->GetURL("origin2.com", "/");
  GURL origin3 = https_server()->GetURL("origin3.com", "/");
  GURL origin4 = https_server()->GetURL("origin4.com", "/");

  // Navigation to worker_setup.html will install a service worker. Since
  // the installation is asynchronous, the JS side will inform us about it in
  // the page title.
  GURL url = origin1;
  AddQuery(&url, "file", "worker_setup.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  WaitForTitle(shell(), "service worker is ready");

  // The service worker will now serve a page containing several images, which
  // the browser will try to fetch. The service worker will be instructed
  // to handle some of the fetches itself, while others will be handled by
  // the testing server. The setup is the following:
  //
  // origin1.com/resource         (-> server; should respect header)
  // origin2.com/resource_from_sw (-> service worker; should not respect header)
  // origin3.com/resource_from_sw (-> service worker; should not respect header)
  // origin4.com/resource         (-> server; should respect header)
  // origin1.com/resource_from_sw (-> service worker; should not respect header)
  // origin2.com/resource         (-> server; should respect header)
  // origin3.com/resource_from_sw (-> service worker; should not respect header)
  // origin4.com/resource         (-> server; should respect header)
  //
  // |origin1| and |origin2| are used to test that there is no difference
  // between same-origin and third-party fetches. Clear-Site-Data should be
  // called once for each of these origins - caused by the "/resource" fetch,
  // but not by the "/resource_from_sw" fetch. |origin3| and |origin4| prove
  // that the number of calls is dependent on the number of network responses,
  // i.e. that it isn't always 1 as in the case of |origin1| and |origin2|.
  delegate()->ExpectClearSiteDataCookiesCall(
      storage_partition_config(), url::Origin::Create(origin1),
      /*override_partition_key_cross_site=*/false, net::SchemefulSite(url));
  delegate()->ExpectClearSiteDataCookiesCall(
      storage_partition_config(), url::Origin::Create(origin4),
      /*override_partition_key_cross_site=*/true, net::SchemefulSite(url));
  delegate()->ExpectClearSiteDataCookiesCall(
      storage_partition_config(), url::Origin::Create(origin2),
      /*override_partition_key_cross_site=*/true, net::SchemefulSite(url));
  delegate()->ExpectClearSiteDataCookiesCall(
      storage_partition_config(), url::Origin::Create(origin4),
      /*override_partition_key_cross_site=*/true, net::SchemefulSite(url));

  url = https_server()->GetURL("origin1.com", "/anything-in-workers-scope");
  AddQuery(&url, "origin1", origin1.spec());
  AddQuery(&url, "origin2", origin2.spec());
  AddQuery(&url, "origin3", origin3.spec());
  AddQuery(&url, "origin4", origin4.spec());
  EXPECT_TRUE(NavigateToURL(shell(), url));
  WaitForTitle(shell(), "done");
  delegate()->VerifyAndClearExpectations();
}

// Tests that Clear-Site-Data is only executed on a resource fetch
// if credentials are allowed in that fetch.

// Crashes on Win only. https://crbug.com/741189
#if BUILDFLAG(IS_WIN)
#define MAYBE_Credentials DISABLED_Credentials
#else
#define MAYBE_Credentials Credentials
#endif
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest, MAYBE_Credentials) {
  GURL page_template = https_server()->GetURL("origin1.com", "/");
  GURL same_origin_resource =
      https_server()->GetURL("origin1.com", "/resource");
  GURL different_origin_resource =
      https_server()->GetURL("origin2.com", "/resource");

  AddQuery(&same_origin_resource, "header", kClearCookiesHeader);
  AddQuery(&different_origin_resource, "header", kClearCookiesHeader);

  const struct TestCase {
    bool same_origin;
    std::string credentials;
    bool should_run;
    bool override_partition_key_cross_site;
  } kTestCases[] = {
      {true, "", true, false},
      {true, "omit", false, false},
      {true, "same-origin", true, false},
      {true, "include", true, false},
      {false, "", false, false},
      {false, "omit", false, false},
      {false, "same-origin", false, false},
      {false, "include", true, true},
  };

  for (const TestCase& test_case : kTestCases) {
    const GURL& resource = test_case.same_origin ? same_origin_resource
                                                 : different_origin_resource;
    std::string credentials =
        test_case.credentials.empty()
            ? ""
            : "credentials: '" + test_case.credentials + "'";

    // Fetch a resource. Note that the script also handles fetch() error which
    // might be thrown for third-party fetches because of missing
    // Access-Control-Allow-Origin. However, that only affects the visibility
    // of the response; the header will still be processed.
    std::string content = base::StringPrintf(
        "<html><head></head><body><script>"
        "fetch('%s', {%s})"
        ".then(function() { document.title = 'done'; })"
        ".catch(function() { document.title = 'done'; })"
        "</script></body></html>",
        resource.spec().c_str(), credentials.c_str());

    GURL page = page_template;
    AddQuery(&page, "html", content);

    if (test_case.should_run)
      delegate()->ExpectClearSiteDataCookiesCall(
          storage_partition_config(), url::Origin::Create(resource),
          /*override_partition_key_cross_site=*/
          test_case.override_partition_key_cross_site,
          net::SchemefulSite(page));

    EXPECT_TRUE(NavigateToURL(shell(), page));
    WaitForTitle(shell(), "done");
    delegate()->VerifyAndClearExpectations();
  }
}

// Tests that the credentials flag is correctly taken into account when it
// interpretation changes after redirect.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest, CredentialsOnRedirect) {
  GURL urls[2] = {
      https_server()->GetURL("origin1.com", "/image.png"),
      https_server()->GetURL("origin2.com", "/image.png"),
  };

  AddQuery(&urls[0], "header", kClearCookiesHeader);
  AddQuery(&urls[1], "header", kClearCookiesHeader);

  AddQuery(&urls[0], "redirect", urls[1].spec());

  // Fetch a resource on origin1.com, which will redirect to origin2.com.
  // Both URLs will respond with Clear-Site-Data. Since the credentials mode is
  // 'same-origin', the LOAD_DO_NOT_SAVE_COOKIES flag will be set while
  // processing the response from origin1.com, but not while processing the
  // response from origin2.com. Therefore, the deletion will only be executed
  // for origin1.com.
  //
  // Note that the script also handles fetch() error which might be thrown for
  // third-party fetches because of missing Access-Control-Allow-Origin.
  // However, that only affects the visibility of the response; the header will
  // still be processed.
  std::string content = base::StringPrintf(
      "<html><head></head><body><script>"
      "fetch('%s', {'credentials': 'same-origin'})"
      ".then(function() { document.title = 'done'; })"
      ".catch(function() { document.title = 'done'; })"
      "</script></body></html>",
      urls[0].spec().c_str());

  delegate()->ExpectClearSiteDataCookiesCall(storage_partition_config(),
                                             url::Origin::Create(urls[0]));

  GURL page = https_server()->GetURL("origin1.com", "/");
  AddQuery(&page, "html", content);

  EXPECT_TRUE(NavigateToURL(shell(), page));
  WaitForTitle(shell(), "done");
  delegate()->VerifyAndClearExpectations();
}

// Tests that ClearSiteData() is called for the correct data types.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest, Types) {
  GURL base_url = https_server()->GetURL("example.com", "/");

  const struct TestCase {
    const char* value;
    bool remove_cookies;
    bool remove_storage;
    bool remove_cache;
  } kTestCases[] = {
      {"\"cookies\"", true, false, false},
      {"\"storage\"", false, true, false},
      {"\"cache\"", false, false, true},
      {"\"cookies\", \"storage\"", true, true, false},
      {"\"cookies\", \"cache\"", true, false, true},
      {"\"storage\", \"cache\"", false, true, true},
      {"\"cookies\", \"storage\", \"cache\"", true, true, true},
  };

  for (const TestCase& test_case : kTestCases) {
    GURL url = base_url;
    AddQuery(&url, "header", test_case.value);

    delegate()->ExpectClearSiteDataCall(
        storage_partition_config(), url::Origin::Create(url),
        net::SchemefulSite(url), test_case.remove_cookies,
        test_case.remove_storage, test_case.remove_cache);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    delegate()->VerifyAndClearExpectations();
  }
}

// Integration test for the deletion of cookies.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       CookiesIntegrationTest) {
  AddCookie(https_server()->GetURL("origin1.com", "/abc"));
  AddCookie(https_server()->GetURL("subdomain.origin1.com", "/"));
  AddCookie(https_server()->GetURL("origin2.com", "/def"));
  AddCookie(https_server()->GetURL("subdomain.origin2.com", "/"));

  // There are four cookies on two eTLD+1s.
  net::CookieList cookies = GetCookies();
  EXPECT_EQ(4u, cookies.size());

  // Let Clear-Site-Data delete the "cookies" of "origin1.com".
  GURL url = https_server()->GetURL("origin1.com", "/clear-site-data");
  AddQuery(&url, "header", kClearCookiesHeader);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Only the "origin2.com" eTLD now has cookies.
  cookies = GetCookies();
  ASSERT_EQ(2u, cookies.size());
  EXPECT_EQ(cookies[0].Domain(), "origin2.com");
  EXPECT_EQ(cookies[1].Domain(), "subdomain.origin2.com");
}

IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       ThirdPartyCookieBlocking) {
  // First disable third-party cookie blocking.
  network::mojom::CookieManager* cookie_manager =
      storage_partition()->GetCookieManagerForBrowserProcess();
  cookie_manager->BlockThirdPartyCookies(false);

  // When third-party cookie blocking is disabled, both cookies should be
  // cleared.
  AddCookie(https_server()->GetURL("origin1.com", "/"));
  AddCookie(https_server()->GetURL("origin1.com", "/"),
            net::CookiePartitionKey::FromURLForTesting(
                GURL("https://origin2.com"),
                net::CookiePartitionKey::AncestorChainBit::kCrossSite));

  GURL url = https_server()->GetURL("origin2.com", "/");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL csd_url = https_server()->GetURL("origin1.com", "/clear-site-data");
  AddQuery(&csd_url, "header", kClearCookiesHeader);
  std::string origin = url.spec();
  // Pop the last character to remove trailing /.
  origin.erase(origin.size() - 1);
  AddQuery(&csd_url, "access-control-allow-origin", origin);

  // Script that makes a cross-site subresource request that responds with
  // Clear-Site-Data.
  std::string script =
      "fetch('" + csd_url.spec() + "', {credentials: 'include'})";
  script += ".then(resp => resp.ok)";
  script += ".catch(err => { console.error(err); return false; });";

  EXPECT_EQ(true, EvalJs(shell()->web_contents(), script));

  auto cookies = GetCookies();
  ASSERT_EQ(0u, cookies.size());

  // Now enable third-party cookie blocking.
  cookie_manager->BlockThirdPartyCookies(true);

  // Unpartitioned cookie, should not be removed.
  AddCookie(https_server()->GetURL("origin1.com", "/"));
  // Partitioned cookie set in the partition we are clearing, should still
  // be removed.
  AddCookie(https_server()->GetURL("origin1.com", "/"),
            net::CookiePartitionKey::FromURLForTesting(
                GURL("https://origin2.com"),
                net::CookiePartitionKey::AncestorChainBit::kCrossSite));

  EXPECT_EQ(true, EvalJs(shell()->web_contents(), script));

  cookies = GetCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// Integration test for the unregistering of service workers.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       StorageServiceWorkersIntegrationTest) {
  StoragePartition* partition = storage_partition();
  net::EmbeddedTestServer* server = https_server();

  browsing_data_browsertest_utils::AddServiceWorker("origin1.com", partition,
                                                    server);
  browsing_data_browsertest_utils::AddServiceWorker("origin2.com", partition,
                                                    server);

  // There are two service workers installed on two origins.
  std::vector<StorageUsageInfo> service_workers =
      browsing_data_browsertest_utils::GetServiceWorkers(partition);
  EXPECT_EQ(2u, service_workers.size());

  // Navigate to a URL within the scope of "origin1.com" which responds with
  // a Clear-Site-Data header. Verify that this did NOT remove the service
  // worker for "origin1.com", as the header would not be respected outside
  // of the scope.
  GURL url = server->GetURL("origin1.com", "/anything-in-the-scope");
  AddQuery(&url, "header", "\"storage\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  service_workers =
      browsing_data_browsertest_utils::GetServiceWorkers(partition);
  EXPECT_EQ(2u, service_workers.size());

  // This time, we will navigate to a URL on "origin1.com" that is not handled
  // by the serice worker, but results in a network request. One such resource
  // not handled by "worker.js" is the path "resource".
  // The header will be respected and the worker deleted.
  url = server->GetURL("origin1.com", "/resource");
  AddQuery(&url, "header", "\"storage\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Only "origin2.com" now has a service worker.
  service_workers =
      browsing_data_browsertest_utils::GetServiceWorkers(partition);
  ASSERT_EQ(1u, service_workers.size());
  EXPECT_EQ(service_workers[0].storage_key.origin().GetURL(),
            server->GetURL("origin2.com", "/"));

  // TODO(msramek): Test that the service worker update ping also deletes
  // the service worker.
}

// TODO(msramek): Add integration tests for other storage data types, such as
// local storage, indexed DB, etc.

// Disabled due to flakiness. See https://crbug.com/894572.
// Integration test for the deletion of cache entries.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       DISABLED_CacheIntegrationTest) {
  GURL url1 = GetURLForHTTPSHost1("/cachetime/foo");
  GURL url2 = GetURLForHTTPSHost1("/cachetime/bar");
  GURL url3 = GetURLForHTTPSHost2("/cachetime/foo");
  GURL url4 = GetURLForHTTPSHost2("/cachetime/bar");

  // Load the url to create cache entries.
  CreateCacheEntry(url1);
  CreateCacheEntry(url2);
  CreateCacheEntry(url3);
  CreateCacheEntry(url4);

  // There are four cache entries on two origins.
  EXPECT_TRUE(TestCacheEntry(url1));
  EXPECT_TRUE(TestCacheEntry(url2));
  EXPECT_TRUE(TestCacheEntry(url3));
  EXPECT_TRUE(TestCacheEntry(url4));

  // Let Clear-Site-Data delete the "cache" of HTTPS host 2.
  GURL url = GetURLForHTTPSHost2("/clear-site-data");
  AddQuery(&url, "header", "\"cache\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Only HTTPS host 1 now has cache entries.
  EXPECT_TRUE(TestCacheEntry(url1));
  EXPECT_TRUE(TestCacheEntry(url2));
  EXPECT_FALSE(TestCacheEntry(url3));
  EXPECT_FALSE(TestCacheEntry(url4));
}

// Tests that closing the tab right after executing Clear-Site-Data does
// not crash.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest, ClosedTab) {
  GURL url = https_server()->GetURL("example.com", "/");
  AddQuery(&url, "header", kClearCookiesHeader);
  shell()->LoadURL(url);
  shell()->Close();
}

// Tests that sending Clear-Site-Data during a service worker installation
// results in the service worker not installed. (see crbug.com/898465)
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       ClearSiteDataDuringServiceWorkerInstall) {
  GURL url = embedded_test_server()->GetURL("127.0.0.1", "/");
  AddQuery(&url, "file", "worker_test.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  delegate()->ExpectClearSiteDataCall(
      storage_partition_config(), url::Origin::Create(url),
      net::SchemefulSite(url), false, true, false, false, SetStorageKey::kNo);
  SetClearSiteDataHeader("\"storage\"");
  EXPECT_FALSE(RunScriptAndGetBool("installServiceWorker()"));
  delegate()->VerifyAndClearExpectations();
  EXPECT_FALSE(RunScriptAndGetBool("hasServiceWorker()"));

  // Install the service worker again without CSD header to verify that
  // future network requests are not broken and the service worker
  // installs correctly.
  SetClearSiteDataHeader("");
  EXPECT_TRUE(RunScriptAndGetBool("installServiceWorker()"));
  delegate()->VerifyAndClearExpectations();
  EXPECT_TRUE(RunScriptAndGetBool("hasServiceWorker()"));
}

// Tests that sending Clear-Site-Data during a service worker update
// removes the service worker. (see crbug.com/898465)
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       ClearSiteDataDuringServiceWorkerUpdate) {
  GURL url = embedded_test_server()->GetURL("127.0.0.1", "/");
  AddQuery(&url, "file", "worker_test.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // Install a service worker.
  EXPECT_TRUE(RunScriptAndGetBool("installServiceWorker()"));
  delegate()->VerifyAndClearExpectations();
  // Update the service worker and send C-S-D during update.
  delegate()->ExpectClearSiteDataCall(
      storage_partition_config(), url::Origin::Create(url),
      net::SchemefulSite(url), false, true, false, false, SetStorageKey::kNo);

  base::RunLoop loop;
  auto* remover = browser_context()->GetBrowsingDataRemover();
  remover->SetWouldCompleteCallbackForTesting(
      base::BindLambdaForTesting([&](base::OnceClosure callback) {
        std::move(callback).Run();
        loop.Quit();
      }));

  SetClearSiteDataHeader("\"storage\"");
  // Expect the update to fail and the service worker to be removed.
  EXPECT_FALSE(RunScriptAndGetBool("updateServiceWorker()"));
  delegate()->VerifyAndClearExpectations();
  loop.Run();

  // Notify crbug.com/912313 if the test fails here again.
  EXPECT_FALSE(RunScriptAndGetBool("hasServiceWorker()"));
}

enum TestScenario {
  NoFeaturesActivated,
  StorageBucketsActivated,
  ThirdPartyStoragePartitioningActivated,
  AllFeaturesActivated,
};

class ClearSiteDataHandlerStorageBucketsBrowserTest
    : public ClearSiteDataHandlerBrowserTest,
      public testing::WithParamInterface<TestScenario> {
 public:
  ClearSiteDataHandlerStorageBucketsBrowserTest() {
    enum TestScenario test_scenario = GetParam();
    std::vector<base::test::FeatureRef> activated_features = {};

    switch (test_scenario) {
      case NoFeaturesActivated:
        break;

      case StorageBucketsActivated:
        activated_features.push_back(blink::features::kStorageBuckets);
        break;

      case ThirdPartyStoragePartitioningActivated:
        activated_features.push_back(
            net::features::kThirdPartyStoragePartitioning);
        break;

      case AllFeaturesActivated:
        activated_features.push_back(blink::features::kStorageBuckets);
        activated_features.push_back(
            net::features::kThirdPartyStoragePartitioning);
        break;
    }

    feature_list_.InitWithFeatures(activated_features, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(StorageBucketsIntegrationTestSuite,
                         ClearSiteDataHandlerStorageBucketsBrowserTest,
                         testing::Values(NoFeaturesActivated,
                                         StorageBucketsActivated,
                                         ThirdPartyStoragePartitioningActivated,
                                         AllFeaturesActivated));

// Integration test for the deletion of storage buckets.
IN_PROC_BROWSER_TEST_P(ClearSiteDataHandlerStorageBucketsBrowserTest,
                       StorageBucketsIntegrationTest) {
  GURL url = https_server()->GetURL("127.0.0.1", "/");

  const auto storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));

  StorageBucketClearSiteDataTester tester(storage_partition());
  tester.CreateBucketForTesting(
      storage_key, "drafts",
      base::BindOnce(
          [](storage::QuotaErrorOr<storage::BucketInfo> error_or_bucket_info) {
          }));
  tester.CreateBucketForTesting(
      storage_key, "inbox",
      base::BindOnce(
          [](storage::QuotaErrorOr<storage::BucketInfo> error_or_bucket_info) {
          }));
  tester.CreateBucketForTesting(
      storage_key, "attachments",
      base::BindOnce(
          [](storage::QuotaErrorOr<storage::BucketInfo> error_or_bucket_info) {
          }));

  AddQuery(&url, "header", "\"storage:drafts\", \"storage:attachments\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  tester.GetBucketsForStorageKey(
      storage_key,
      base::BindOnce([](storage::QuotaErrorOr<std::set<storage::BucketInfo>>
                            error_or_buckets) {
        EXPECT_EQ(base::FeatureList::IsEnabled(blink::features::kStorageBuckets)
                      ? 1u
                      : 3u,
                  error_or_buckets.value().size());
      }));

  delegate()->VerifyAndClearExpectations();
}

class ClearSiteDataHandlerSharedStorageBrowserTest
    : public ClearSiteDataHandlerBrowserTest {
 public:
  ClearSiteDataHandlerSharedStorageBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Integration test for the deletion of shared storage.
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerSharedStorageBrowserTest,
                       SharedStorageIntegrationTest) {
  SharedStorageClearSiteDataTester tester(storage_partition());

  GURL url1 = https_server()->GetURL("origin1.com", "/");
  const url::Origin kOrigin1 = url::Origin::Create(url1);
  tester.AddConsecutiveSharedStorageEntries(kOrigin1, u"key", u"value", 10);

  GURL url2 = https_server()->GetURL("origin2.com", "/");
  const url::Origin kOrigin2 = url::Origin::Create(url2);
  tester.AddConsecutiveSharedStorageEntries(kOrigin2, u"key", u"value", 5);

  // There are 15 entries for two origins.
  EXPECT_THAT(tester.GetSharedStorageOrigins(),
              testing::UnorderedElementsAre(kOrigin1, kOrigin2));

  // Note that u"key" concatenated with a single digit has 4 char16_t's and
  // hence 8 bytes. Similarly, u"value" concatenated with one digit has
  // 6 char16_t's and hence 12 bytes. A pair of these together thus has
  // 20 bytes.
  const int kNumBytesPerEntry = 20;
  EXPECT_EQ(10 * kNumBytesPerEntry,
            tester.GetSharedStorageNumBytesForOrigin(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerEntry,
            tester.GetSharedStorageNumBytesForOrigin(kOrigin2));
  EXPECT_EQ(15 * kNumBytesPerEntry, tester.GetSharedStorageTotalBytes());

  // Let Clear-Site-Data delete the shared storage of "origin1.com".
  delegate()->ExpectClearSiteDataCall(storage_partition_config(), kOrigin1,
                                      net::SchemefulSite(kOrigin1),
                                      /*cookies=*/false,
                                      /*storage=*/true, /*cache=*/false);
  AddQuery(&url1, "header", "\"storage\"");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  delegate()->VerifyAndClearExpectations();

  // There are now only 5 entries for one origin.
  EXPECT_THAT(tester.GetSharedStorageOrigins(),
              testing::UnorderedElementsAre(kOrigin2));
  EXPECT_EQ(0, tester.GetSharedStorageNumBytesForOrigin(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerEntry,
            tester.GetSharedStorageNumBytesForOrigin(kOrigin2));
  EXPECT_EQ(5 * kNumBytesPerEntry, tester.GetSharedStorageTotalBytes());
}

}  // namespace content
