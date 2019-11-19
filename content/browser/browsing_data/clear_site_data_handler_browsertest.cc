// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/url_constants.h"

using testing::_;

namespace content {

namespace {

// Adds a key=value pair to the url's query.
void AddQuery(GURL* url, const std::string& key, const std::string& value) {
  *url = GURL(url->spec() + (url->has_query() ? "&" : "?") + key + "=" +
              net::EscapeQueryParamValue(value, false));
}

// A helper function to synchronize with JS side of the tests. JS can append
// information to the loaded website's title and C++ will wait until that
// happens.
void WaitForTitle(const Shell* shell, const char* expected_title) {
  base::string16 expected_title_16 = base::ASCIIToUTF16(expected_title);
  TitleWatcher title_watcher(shell->web_contents(), expected_title_16);
  ASSERT_EQ(expected_title_16, title_watcher.WaitAndGetTitle());
}

// A value of the Clear-Site-Data header that requests cookie deletion. Reused
// in tests that need a valid header but do not depend on its value.
static const char* kClearCookiesHeader = "\"cookies\"";

// A helper class to observe BrowsingDataRemover deletion tasks coming from
// ClearSiteData.
class TestBrowsingDataRemoverDelegate : public MockBrowsingDataRemoverDelegate {
 public:
  // Sets a test expectation that a Clear-Site-Data header call from |origin|,
  // instructing to delete |cookies|, |storage|, and |cache|, will schedule
  // the corresponding BrowsingDataRemover deletion tasks.
  void ExpectClearSiteDataCall(const url::Origin& origin,
                               bool cookies,
                               bool storage,
                               bool cache) {
    const int kOriginTypeMask =
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
        BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

    if (cookies) {
      int data_type_mask =
          BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS;

      BrowsingDataFilterBuilderImpl filter_builder(
          BrowsingDataFilterBuilder::WHITELIST);
      filter_builder.AddRegisterableDomain(origin.host());
      ExpectCall(base::Time(), base::Time::Max(), data_type_mask,
                 kOriginTypeMask, &filter_builder);
    }
    if (storage || cache) {
      int data_type_mask =
          (storage ? BrowsingDataRemover::DATA_TYPE_DOM_STORAGE : 0) |
          (cache ? BrowsingDataRemover::DATA_TYPE_CACHE : 0);

      BrowsingDataFilterBuilderImpl filter_builder(
          BrowsingDataFilterBuilder::WHITELIST);
      filter_builder.AddOrigin(origin);
      ExpectCall(base::Time(), base::Time::Max(), data_type_mask,
                 kOriginTypeMask, &filter_builder);
    }
  }

  // A shortcut for the above method, but with only cookies deleted. This is
  // useful for most tests that use |kClearCookiesHeader|.
  void ExpectClearSiteDataCookiesCall(const url::Origin& origin) {
    ExpectClearSiteDataCall(origin, true, false, false);
  }
};

}  // namespace

class ClearSiteDataHandlerBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    browsing_data_browsertest_utils::SetIgnoreCertificateErrors(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    BrowserContext::GetBrowsingDataRemover(browser_context())
        ->SetEmbedderDelegate(&embedder_delegate_);

    // Set up HTTP and HTTPS test servers that handle all hosts.
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService())
      browsing_data_browsertest_utils::SetUpMockCertVerifier(net::OK);

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ClearSiteDataHandlerBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up HTTPS server.
    https_server_.reset(new net::EmbeddedTestServer(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS));
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
    return BrowserContext::GetDefaultStoragePartition(browser_context());
  }

  // Adds a cookie for the |url|. Used in the cookie integration tests.
  void AddCookie(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();

    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, "A=1", base::Time::Now(), base::nullopt /* server_time */));

    base::RunLoop run_loop;
    cookie_manager->SetCanonicalCookie(
        *cookie, url.scheme(), net::CookieOptions::MakeAllInclusive(),
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
    cookie_manager->GetAllCookies(base::BindRepeating(
        &ClearSiteDataHandlerBrowserTest::GetCookiesCallback,
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
                            0 /* process_id */, 0 /* render_frame_id */,
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
    bool data;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(shell()->web_contents(),
                                                     script, &data));
    return data;
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
  static void AddCookieCallback(
      base::OnceClosure callback,
      net::CanonicalCookie::CookieInclusionStatus status) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ASSERT_TRUE(status.IsInclude());
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
#if defined(OS_WIN)
#define MAYBE_RedirectNavigation DISABLED_RedirectNavigation
#else
#define MAYBE_RedirectNavigation RedirectNavigation
#endif
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       MAYBE_RedirectNavigation) {
  GURL page_urls[3] = {
      https_server()->GetURL("origin1.com", "/"),
      https_server()->GetURL("origin2.com", "/foo/bar"),
      https_server()->GetURL("origin3.com", "/index.html"),
  };

  // Iterate through the configurations. URLs whose index is matched by the mask
  // will send the header, the others won't.
  for (int mask = 0; mask < (1 << 3); ++mask) {
    GURL urls[3];

    // Set up the expectations.
    for (int i = 0; i < 3; ++i) {
      urls[i] = page_urls[i];
      if (mask & (1 << i))
        AddQuery(&urls[i], "header", kClearCookiesHeader);

      if (mask & (1 << i))
        delegate()->ExpectClearSiteDataCookiesCall(
            url::Origin::Create(urls[i]));
    }

    // Set up redirects between urls 0 --> 1 --> 2.
    AddQuery(&urls[1], "redirect", urls[2].spec());
    AddQuery(&urls[0], "redirect", urls[1].spec());

    // Navigate to the first url of the redirect chain.
    EXPECT_TRUE(
        NavigateToURL(shell(), urls[0], urls[2] /* expected_commit_url */));

    // We reached the end of the redirect chain.
    EXPECT_EQ(urls[2], shell()->web_contents()->GetURL());

    delegate()->VerifyAndClearExpectations();
  }
}

// Tests that the header is recognized on the beginning, in the middle, and on
// the end of a resource load redirect chain. Each of the three parts of the
// chain may or may not send the header, so there are 8 configurations to test.

// Crashes on Win only. https://crbug.com/741189
#if defined(OS_WIN)
#define MAYBE_RedirectResourceLoad DISABLED_RedirectResourceLoad
#else
#define MAYBE_RedirectResourceLoad RedirectResourceLoad
#endif
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       MAYBE_RedirectResourceLoad) {
  GURL resource_urls[3] = {
      https_server()->GetURL("origin1.com", "/redirect-start"),
      https_server()->GetURL("origin2.com", "/redirect-middle"),
      https_server()->GetURL("origin3.com", "/redirect-end"),
  };

  // Iterate through the configurations. URLs whose index is matched by the mask
  // will send the header, the others won't.
  for (int mask = 0; mask < (1 << 3); ++mask) {
    GURL urls[3];

    // Set up the expectations.
    for (int i = 0; i < 3; ++i) {
      urls[i] = resource_urls[i];
      if (mask & (1 << i))
        AddQuery(&urls[i], "header", kClearCookiesHeader);

      if (mask & (1 << i))
        delegate()->ExpectClearSiteDataCookiesCall(
            url::Origin::Create(urls[i]));
    }

    // Set up redirects between urls 0 --> 1 --> 2.
    AddQuery(&urls[1], "redirect", urls[2].spec());
    AddQuery(&urls[0], "redirect", urls[1].spec());

    // Navigate to a page that embeds "https://origin1.com/image.png"
    // and observe the loading of that resource.
    GURL page_with_image = https_server()->GetURL("origin4.com", "/index.html");
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
  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(secure_image));

  EXPECT_TRUE(NavigateToURL(shell(), secure_page));
  delegate()->VerifyAndClearExpectations();

  // Secure resource on a secure page does execute Clear-Site-Data.
  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(secure_image));

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
  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(origin1));
  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(origin4));
  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(origin2));
  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(origin4));

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
#if defined(OS_WIN)
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
  } kTestCases[] = {
      {true, "", true},
      {true, "omit", false},
      {true, "same-origin", true},
      {true, "include", true},
      {false, "", false},
      {false, "omit", false},
      {false, "same-origin", false},
      {false, "include", true},
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
      delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(resource));

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

  delegate()->ExpectClearSiteDataCookiesCall(url::Origin::Create(urls[0]));

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
        url::Origin::Create(url), test_case.remove_cookies,
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
  EXPECT_EQ(service_workers[0].origin.GetURL(),
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
// doesn't fail. (see crbug.com/898465)
IN_PROC_BROWSER_TEST_F(ClearSiteDataHandlerBrowserTest,
                       ClearSiteDataDuringServiceWorkerInstall) {
  GURL url = embedded_test_server()->GetURL("127.0.0.1", "/");
  AddQuery(&url, "file", "worker_test.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  delegate()->ExpectClearSiteDataCall(url::Origin::Create(url), false, true,
                                      false);
  SetClearSiteDataHeader("\"storage\"");
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
  delegate()->ExpectClearSiteDataCall(url::Origin::Create(url), false, true,
                                      false);

  base::RunLoop loop;
  auto* remover = BrowserContext::GetBrowsingDataRemover(browser_context());
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

}  // namespace content
