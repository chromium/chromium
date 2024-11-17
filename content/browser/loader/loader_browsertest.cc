// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::HasSubstr;
using testing::Not;

namespace content {

class LoaderBrowserTest : public ContentBrowserTest,
                          public DownloadManager::Observer {
 public:
  LoaderBrowserTest() : got_downloads_(false) {}

 protected:
  void SetUpOnMainThread() override {
    base::FilePath path = GetTestFilePath("", "");
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&net::URLRequestMockHTTPJob::AddUrlHandlers, path));
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void CheckTitleTest(const GURL& url, const std::string& expected_title) {
    std::u16string expected_title16(ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  bool GetPopupTitle(const GURL& url, std::u16string* title) {
    EXPECT_TRUE(NavigateToURL(shell(), url));

    ShellAddedObserver new_shell_observer;

    // Create dynamic popup.
    if (!ExecJs(shell(), "OpenPopup();")) {
      return false;
    }

    Shell* new_shell = new_shell_observer.GetShell();
    *title = new_shell->web_contents()->GetTitle();
    return true;
  }

  std::string GetCookies(const GURL& url) {
    return content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                               url);
  }

  bool got_downloads() const { return got_downloads_; }

 private:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "cors_exempt_header_list", "ExemptFoo");
    ContentBrowserTest::SetUp();
  }

  bool got_downloads_;
};

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DynamicTitle1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/dynamic1.html"));
  std::u16string title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(
      base::StartsWith(title, u"My Popup Title", base::CompareCase::SENSITIVE))
      << "Actual title: " << title;
}

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DynamicTitle2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/dynamic2.html"));
  std::u16string title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(base::StartsWith(title, u"My Dynamic Title",
                               base::CompareCase::SENSITIVE))
      << "Actual title: " << title;
}

// Tests that the renderer does not crash when issuing a stale-revalidation
// request when the enable_referrers renderer preference is `false`. See
// https://crbug.com/966140.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       DisableReferrersStaleWhileRevalidate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to the page that will eventually fetch a stale-revalidation
  // request. Ensure that the renderer has not crashed.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/stale-while-revalidate.html")));

  // Force-disable the |enable_referrers| preference.
  web_contents->GetMutableRendererPrefs()->enable_referrers = false;
  web_contents->SyncRendererPrefs();

  // Wait for the stale-while-revalidate tests to pass by observing the page's
  // title. If the renderer crashes, the test immediately fails.
  std::u16string expected_title = u"Pass";
  TitleWatcher title_watcher(web_contents, expected_title);

  // The invocation of runTest() below starts a test written in JavaScript, that
  // after some time, creates a stale-revalidation request. The above IPC
  // message should be handled by the renderer (thus updating its preferences),
  // before this stale-revalidation request is sent. Technically nothing
  // guarantees this will happen, so it is theoretically possible the test is
  // racy, however in practice the renderer will always handle the IPC message
  // before the stale-revalidation request. This is because the renderer is
  // never completely blocked from the time the test starts.
  EXPECT_TRUE(ExecJs(shell(), "runTest()"));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SniffNoContentTypeNoData) {
  // Make sure no downloads start.
  shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDownloadManager()
      ->AddObserver(this);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-sniffer-test3.html"));
  CheckTitleTest(url, "Content Sniffer Test 3");
  EXPECT_EQ(1u, Shell::windows().size());
  ASSERT_FALSE(got_downloads());
}

// Make sure file URLs are not sniffed as HTML when they don't end in HTML.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DoNotSniffHTMLFromFileUrl) {
  base::FilePath path =
      GetTestFilePath(nullptr, "content-sniffer-test5.not-html");
  GURL file_url = net::FilePathToFileURL(path);
  // If the file isn't rendered as HTML, the title will match the name of the
  // file, rather than the contents of the file's title tag.
  CheckTitleTest(file_url, path.BaseName().MaybeAsASCII());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, ContentDispositionEmpty) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-disposition-empty.html"));
  CheckTitleTest(url, "success");
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, ContentDispositionInline) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-disposition-inline.html"));
  CheckTitleTest(url, "success");
}

// Test for bug #1091358.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SyncXMLHttpRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/sync_xmlhttprequest.html")));

  // Let's check the XMLHttpRequest ran successfully.
  EXPECT_EQ(true, EvalJs(shell(), "DidSyncRequestSucceed();"));
}

// If this flakes, use http://crbug.com/62776.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SyncXMLHttpRequest_Disallowed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/sync_xmlhttprequest_disallowed.html")));

  // Let's check the XMLHttpRequest ran successfully.
  EXPECT_EQ(true, EvalJs(shell(), "DidSucceed();"));
}

// Test for bug #1159553 -- A synchronous xhr (whose content-type is
// downloadable) would trigger download and hang the renderer process,
// if executed while navigating to a new page.
// Disabled on Mac: see http://crbug.com/56264
#if BUILDFLAG(IS_MAC)
#define MAYBE_SyncXMLHttpRequest_DuringUnload \
  DISABLED_SyncXMLHttpRequest_DuringUnload
#else
#define MAYBE_SyncXMLHttpRequest_DuringUnload SyncXMLHttpRequest_DuringUnload
#endif
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       MAYBE_SyncXMLHttpRequest_DuringUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDownloadManager()
      ->AddObserver(this);

  CheckTitleTest(
      embedded_test_server()->GetURL("/sync_xmlhttprequest_during_unload.html"),
      "sync xhr on unload");

  // Navigate to a new page, to dispatch unload event and trigger xhr.
  // (the bug would make this step hang the renderer).
  CheckTitleTest(embedded_test_server()->GetURL("/title2.html"),
                 "Title Of Awesomeness");

  ASSERT_FALSE(got_downloads());
}

namespace {

// Responds with a HungResponse for the specified URL to hang on the request.
// It crashes the process.
//
// |crash_network_service_callback| crashes the network service when invoked,
// and must be called on the UI thread.
std::unique_ptr<net::test_server::HttpResponse> CancelOnRequest(
    const std::string& relative_url,
    int child_id,
    base::RepeatingClosure crash_network_service_callback,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != relative_url) {
    return nullptr;
  }

  GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                      crash_network_service_callback);

  return std::make_unique<net::test_server::HungResponse>();
}

}  // namespace

// Tests the case where the request is cancelled by a layer above the
// URLRequest, which passes the error on ResourceLoader teardown, rather than in
// response to call to AsyncResourceHandler::OnResponseComplete.
// Failed on Android M builder. See crbug/1111427.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SyncXMLHttpRequest_Cancelled DISABLED_SyncXMLHttpRequest_Cancelled
#else
#define MAYBE_SyncXMLHttpRequest_Cancelled SyncXMLHttpRequest_Cancelled
#endif
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, MAYBE_SyncXMLHttpRequest_Cancelled) {
  // If network service is running in-process, we can't simulate a crash.
  if (IsInProcessNetworkService()) {
    return;
  }

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &CancelOnRequest, "/hung",
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
      base::BindRepeating(&BrowserTestBase::SimulateNetworkServiceCrash,
                          base::Unretained(this))));

  ASSERT_TRUE(embedded_test_server()->Start());
  WaitForLoadStop(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/sync_xmlhttprequest_cancelled.html")));

  // 19 is the value of NETWORK_ERROR on DOMException.
  EXPECT_EQ(19, EvalJs(shell(), "getErrorCode();"));
}

// Flaky everywhere. http://crbug.com/130404
// Tests that onunload is run for cross-site requests.  (Bug 1114994)
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DISABLED_CrossSiteOnunloadCookie) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a new cross-site page, to dispatch unload event and set the
  // cookie.
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");

  // Check that the cookie was set.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));
}

// If this flakes, use http://crbug.com/130404
// Tests that onunload is run for cross-site requests to URLs that complete
// without network loads (e.g., about:blank, data URLs).
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       DISABLED_CrossSiteImmediateLoadOnunloadCookie) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a cross-site page that loads immediately without making a
  // network request.  The unload event should still be run.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Check that the cookie was set.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));
}

namespace {

// Handles |request| by serving a redirect response.
std::unique_ptr<net::test_server::HttpResponse> NoContentResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_NO_CONTENT);
  return std::move(http_response);
}

}  // namespace

// Tests that the unload handler is not run for 204 responses.
// If this flakes use http://crbug.com/80596.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteNoUnloadOn204) {
  const char kNoContentPath[] = "/nocontent";
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&NoContentResponseHandler, kNoContentPath));

  ASSERT_TRUE(embedded_test_server()->Start());

  // Start with a URL that sets a cookie in its unload handler.
  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a cross-site URL that returns a 204 No Content response.
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(
      shell(), embedded_test_server()->GetURL(kNoContentPath)));

  // Check that the unload cookie was not set.
  EXPECT_EQ("", GetCookies(url));
}

// Tests that the onbeforeunload and onunload logic is short-circuited if the
// old renderer is gone.  In that case, we don't want to wait for the old
// renderer to run the handlers.
// We need to disable this on Mac because the crash causes the OS CrashReporter
// process to kick in to analyze the poor dead renderer.  Unfortunately, if the
// app isn't stripped of debug symbols, this takes about five minutes to
// complete and isn't conducive to quick turnarounds. As we don't currently
// strip the app on the build bots, this is bad times.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CrossSiteAfterCrash DISABLED_CrossSiteAfterCrash
#else
#define MAYBE_CrossSiteAfterCrash CrossSiteAfterCrash
#endif
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, MAYBE_CrossSiteAfterCrash) {
  // Make sure we have a live process before trying to kill it.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Cause the renderer to crash.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_FALSE(NavigateToURL(shell(), GURL(blink::kChromeUICrashURL)));
  // Wait for browser to notice the renderer crash.
  crash_observer.Wait();

  // Navigate to a new cross-site page.  The browser should not wait around for
  // the old renderer's on{before}unload handlers to run.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-sniffer-test0.html"));
  CheckTitleTest(url, "Content Sniffer Test 0");
}

// Tests that cross-site navigations work when the new page does not go through
// the BufferedEventHandler (e.g., non-http{s} URLs).  (Bug 1225872)
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteNavigationNonBuffered) {
  // Start with an HTTP page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("/content-sniffer-test0.html"));
  CheckTitleTest(url1, "Content Sniffer Test 0");

  // Now load a file:// page, which does not use the BufferedEventHandler.
  // Make sure that the page loads and displays a title, and doesn't get stuck.
  GURL url2 = GetTestUrl("", "title2.html");
  CheckTitleTest(url2, "Title Of Awesomeness");
}

// Flaky everywhere. http://crbug.com/130404
// Tests that a cross-site navigation to an error page (resulting in the link
// doctor page) still runs the onunload handler and can support navigations
// away from the link doctor page.  (Bug 1235537)
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       DISABLED_CrossSiteNavigationErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/onunload_cookie.html"));
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a new cross-site URL that results in an error.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  GURL failed_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(failed_url,
                                                   net::ERR_NAME_NOT_RESOLVED);
  EXPECT_FALSE(NavigateToURL(shell(), failed_url));

  EXPECT_NE(u"set cookie on unload", shell()->web_contents()->GetTitle());

  // Check that the cookie was set, meaning that the onunload handler ran.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));

  // Check that renderer-initiated navigations still work.  In a previous bug,
  // the ResourceDispatcherHost would think that such navigations were
  // cross-site, because we didn't clean up from the previous request.  Since
  // WebContentsImpl was in the NORMAL state, it would ignore the attempt to run
  // the onunload handler, and the navigation would fail. We can't test by
  // redirecting to javascript:window.location='someURL', since javascript:
  // URLs are prohibited by policy from interacting with sensitive chrome
  // pages of which the error page is one.  Instead, use automation to kick
  // off the navigation, and wait to see that the tab loads.
  std::u16string expected_title16(u"Title Of Awesomeness");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);

  GURL test_url(embedded_test_server()->GetURL("/title2.html"));
  std::string redirect_script =
      "window.location='" + test_url.possibly_invalid_spec() + "';" + "true;";
  EXPECT_EQ(true, EvalJs(shell(), redirect_script));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteNavigationErrorPage2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title2.html"));
  CheckTitleTest(url, "Title Of Awesomeness");

  // Navigate to a new cross-site URL that results in an error.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  GURL failed_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(failed_url,
                                                   net::ERR_NAME_NOT_RESOLVED);

  EXPECT_FALSE(NavigateToURL(shell(), failed_url));
  EXPECT_NE(u"Title Of Awesomeness", shell()->web_contents()->GetTitle());

  // Repeat navigation.  We are testing that this completes.
  EXPECT_FALSE(NavigateToURL(shell(), failed_url));
  EXPECT_NE(u"Title Of Awesomeness", shell()->web_contents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossOriginRedirectBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/cross-origin-redirect-blocked.html"));
  // We expect the following URL requests from this test:
  // 1- navigation to http://127.0.0.1:[port]/cross-origin-redirect-blocked.html
  // 2- XHR to
  // http://127.0.0.1:[port]/server-redirect-302?http://a.com:[port]/title2.html
  // 3- above XHR is redirected to http://a.com:[port]/title2.html which should
  // be blocked
  // 4- When the page notices the above request is blocked, it issues an XHR to
  // http://127.0.0.1:[port]/title2.html
  // 5- When the above XHR succeed, the page navigates to
  // http://127.0.0.1:[port]/title3.html
  //
  // If the redirect in #3 were not blocked, we'd instead see a navigation
  // to http://a.com[port]/title2.html, and the title would be different.
  CheckTitleTest(url, "Title Of More Awesomeness");
}

// Tests that ResourceRequestInfoImpl is updated correctly on failed
// requests, to prevent calling Read on a request that has already failed.
// See bug 40250.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteFailedRequest) {
  // Visit another URL first to trigger a cross-site navigation.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "simple_page.html")));

  // Visit a URL that fails without calling ResourceDispatcherHost::Read.
  GURL broken_url("chrome://theme");
  EXPECT_FALSE(NavigateToURL(shell(), broken_url));
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
    const std::string& request_path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, request_path,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader(
      "Location", request.relative_url.substr(request_path.length()));
  return std::move(http_response);
}

}  // namespace

// Test that we update the cookie policy URLs correctly when transferring
// navigations.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CookiePolicy) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRedirectRequest, "/redirect?"));
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string set_cookie_url(base::StringPrintf(
      "http://localhost:%u/set_cookie.html", embedded_test_server()->port()));
  GURL url(embedded_test_server()->GetURL("/redirect?" + set_cookie_url));

  std::u16string expected_title16(u"cookie set");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  EXPECT_TRUE(NavigateToURL(shell(), url,
                            GURL(set_cookie_url) /* expected_commit_url */));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// Test that ui::PAGE_TRANSITION_CLIENT_REDIRECT is correctly set
// when encountering a meta refresh tag.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, PageTransitionClientRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/client_redirect.html"), 2);

  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();

  EXPECT_TRUE(entry->GetTransitionType() & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SubresourceRedirectToDataURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  GURL subresource_url = embedded_test_server()->GetURL(
      "/server-redirect?data:text/plain,redirected1");
  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    return new Promise(resolve => {
      xhr.onload = () => resolve("ALLOWED");
      xhr.onerror = () => resolve("BLOCKED");
      xhr.send();
    });
  }))";

  EXPECT_EQ("BLOCKED",
            EvalJs(shell(), script + "('" + subresource_url.spec() + "')"));
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, RedirectToDataURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_FALSE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/server-redirect?data:text/plain,redirected1")));
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, RedirectToAboutURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_FALSE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/server-redirect?" + std::string(url::kAboutBlankURL))));
}

namespace {

// Creates a valid filesystem URL.
GURL CreateFileSystemURL(Shell* window) {
  std::string filesystem_url_string = EvalJs(window, R"(
      var blob = new Blob(['<html><body>hello</body></html>'],
                          {type: 'text/html'});
      new Promise(resolve => {
        window.webkitRequestFileSystem(TEMPORARY, blob.size, fs => {
          fs.root.getFile('foo.html', {create: true}, file => {
            file.createWriter(writer => {
              writer.write(blob);
              writer.onwriteend = () => {
                resolve(file.toURL());
              }
            });
          });
        });
      });)")
                                          .ExtractString();
  GURL filesystem_url(filesystem_url_string);
  EXPECT_TRUE(filesystem_url.is_valid());
  EXPECT_TRUE(filesystem_url.SchemeIsFileSystem());
  return filesystem_url;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       SubresourceRedirectToFileSystemURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  GURL subresource_url = embedded_test_server()->GetURL(
      "/server-redirect?" + CreateFileSystemURL(shell()).spec());
  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    return new Promise(resolve => {
      xhr.onload = () => resolve("ALLOWED");
      xhr.onerror = () => resolve("BLOCKED");
      xhr.send();
    });
  }))";

  EXPECT_EQ("BLOCKED",
            EvalJs(shell(), script + "('" + subresource_url.spec() + "')"));
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, RedirectToFileSystemURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Need to navigate to a URL first so the filesystem can be created.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_FALSE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/server-redirect?" + CreateFileSystemURL(shell()).spec())));
}

namespace {

struct RequestData {
  const GURL url;
  const net::SiteForCookies site_for_cookies;
  const std::optional<url::Origin> initiator;
  const int load_flags;
  const std::string referrer;

  RequestData(const GURL& url,
              const net::SiteForCookies& site_for_cookies,
              const std::optional<url::Origin>& initiator,
              int load_flags,
              const std::string& referrer)
      : url(url),
        site_for_cookies(site_for_cookies),
        initiator(initiator),
        load_flags(load_flags),
        referrer(referrer) {}
};

}  // namespace

class RequestDataBrowserTest : public ContentBrowserTest {
 public:
  RequestDataBrowserTest()
      : interceptor_(std::make_unique<content::URLLoaderInterceptor>(
            base::BindRepeating(&RequestDataBrowserTest::OnRequest,
                                base::Unretained(this)))) {}
  ~RequestDataBrowserTest() override {}

  std::vector<RequestData> data() {
    base::AutoLock auto_lock(requests_lock_);
    auto copy = requests_;
    return copy;
  }

  void WaitForRequests(size_t count) {
    while (true) {
      base::RunLoop run_loop;
      {
        base::AutoLock auto_lock(requests_lock_);
        if (requests_.size() == count) {
          return;
        }
        requests_closure_ = run_loop.QuitClosure();
      }
      run_loop.Run();
    }
  }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override { interceptor_.reset(); }

  bool OnRequest(URLLoaderInterceptor::RequestParams* params) {
    RequestCreated(RequestData(
        params->url_request.url, params->url_request.site_for_cookies,
        params->url_request.request_initiator, params->url_request.load_flags,
        params->url_request.referrer.spec()));
    return false;
  }

  void RequestCreated(RequestData data) {
    base::AutoLock auto_lock(requests_lock_);
    requests_.push_back(data);
    if (requests_closure_) {
      std::move(requests_closure_).Run();
    }
  }

  base::Lock requests_lock_;
  std::vector<RequestData> requests_;
  base::OnceClosure requests_closure_;
  std::unique_ptr<URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, Basic) {
  GURL top_url(embedded_test_server()->GetURL("/page_with_subresources.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(8u);

  auto requests = data();
  EXPECT_EQ(8u, requests.size());

  // All resources loaded directly by the top-level document should have a
  // |first_party| and |initiator| that match the URL of the top-level document.
  // The top-level document itself doesn't have an |initiator|.
  const RequestData* first_request = &requests[0];
  EXPECT_TRUE(first_request->site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(top_url)));
  EXPECT_FALSE(first_request->initiator.has_value());
  for (size_t i = 1; i < requests.size(); i++) {
    const RequestData* request = &requests[i];
    EXPECT_TRUE(request->site_for_cookies.IsEquivalent(
        net::SiteForCookies::FromOrigin(top_origin)));
    ASSERT_TRUE(request->initiator.has_value());
    EXPECT_EQ(top_origin, request->initiator);
  }
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, LinkRelPrefetch) {
  GURL top_url(embedded_test_server()->GetURL("/link_rel_prefetch.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(2u);

  auto requests = data();
  EXPECT_EQ(2u, requests.size());
  auto* request = &requests[1];
  EXPECT_EQ(top_origin, request->initiator);
  EXPECT_EQ(top_url, request->referrer);
  EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, LinkRelPrefetchReferrerPolicy) {
  GURL top_url(embedded_test_server()->GetURL(
      "/link_rel_prefetch_referrer_policy.html"));
  GURL img_url(embedded_test_server()->GetURL("/image.jpg"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(2u);

  auto requests = data();
  EXPECT_EQ(2u, requests.size());
  auto* main_frame_request = &requests[0];
  auto* image_request = &requests[1];

  // Check the main frame request.
  EXPECT_EQ(top_url, main_frame_request->url);
  EXPECT_FALSE(main_frame_request->initiator.has_value());

  // Check the image request.
  EXPECT_EQ(img_url, image_request->url);
  EXPECT_TRUE(image_request->initiator.has_value());
  EXPECT_EQ(top_origin, image_request->initiator);
  // Respect the "origin" policy set by the <meta> tag.
  EXPECT_EQ(top_url.DeprecatedGetOriginAsURL().spec(), image_request->referrer);
  EXPECT_TRUE(image_request->load_flags & net::LOAD_PREFETCH);
}

// TODO(crbug.com/40805845): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, DISABLED_BasicCrossSite) {
  GURL top_url(embedded_test_server()->GetURL(
      "a.com", "/nested_page_with_subresources.html"));
  GURL nested_url(embedded_test_server()->GetURL(
      "not-a.com", "/page_with_subresources.html"));
  url::Origin top_origin = url::Origin::Create(top_url);
  url::Origin nested_origin = url::Origin::Create(nested_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(9u, requests.size());

  // The first items loaded are the top-level and nested documents. These should
  // both have a |site_for_cookies| that matches the origin of the top-level
  // document. The top-level document has no initiator and the nested frame is
  // initiated by the top-level document.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_TRUE(requests[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_FALSE(requests[0].initiator.has_value());

  EXPECT_EQ(nested_url, requests[1].url);
  EXPECT_TRUE(requests[1].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_EQ(top_origin, requests[1].initiator);

  // The remaining items are loaded as subresources in the nested document, and
  // should have a unique first-party, and an initiator that matches the
  // document in which they're embedded.
  for (size_t i = 2; i < requests.size(); i++) {
    SCOPED_TRACE(requests[i].url);
    EXPECT_TRUE(requests[i].site_for_cookies.IsNull());
    EXPECT_EQ(nested_origin, requests[i].initiator);
  }
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, SameOriginNested) {
  GURL top_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL image_url(embedded_test_server()->GetURL("/image.jpg"));
  GURL nested_url(embedded_test_server()->GetURL("/title1.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(3u);

  auto requests = data();
  EXPECT_EQ(3u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate. The navigation was initiated outside of a
  // document, so there is no |initiator|.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_TRUE(requests[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_FALSE(requests[0].initiator.has_value());

  // Subresource requests have a first-party and initiator that matches the
  // document in which they're embedded.
  EXPECT_EQ(image_url, requests[1].url);
  EXPECT_TRUE(requests[1].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_EQ(top_origin, requests[1].initiator);

  // Same-origin nested frames have a first-party and initiator that matches
  // the document in which they're embedded (since the frame is same site with
  // toplevel).
  EXPECT_EQ(nested_url, requests[2].url);
  EXPECT_TRUE(requests[2].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_EQ(top_origin, requests[2].initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, SameOriginAuxiliary) {
  GURL top_url(embedded_test_server()->GetURL("/simple_links.html"));
  GURL auxiliary_url(embedded_test_server()->GetURL("/title2.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  ShellAddedObserver new_shell_observer;
  EXPECT_EQ(true, EvalJs(shell(), "clickSameSiteNewWindowLink();"));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  WaitForRequests(2u);

  auto requests = data();
  EXPECT_EQ(2u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate, even if they fail to load. The navigation was
  // initiated outside of a document, so there is no |initiator|.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_TRUE(requests[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_FALSE(requests[0].initiator.has_value());

  // Auxiliary navigations have a first-party that matches the URL to which they
  // navigate, and an initiator that matches the document that triggered them.
  EXPECT_EQ(auxiliary_url, requests[1].url);
  EXPECT_TRUE(requests[1].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(auxiliary_url)));
  EXPECT_EQ(top_origin, requests[1].initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, CrossOriginAuxiliary) {
  GURL top_url(embedded_test_server()->GetURL("/simple_links.html"));
  GURL auxiliary_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  const char kReplacePortNumber[] = "setPortNumber(%d);";
  uint16_t port_number = embedded_test_server()->port();
  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf(kReplacePortNumber, port_number)));

  ShellAddedObserver new_shell_observer;
  EXPECT_EQ(true, EvalJs(shell(), "clickCrossSiteNewWindowLink();"));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  auto requests = data();
  EXPECT_EQ(2u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate, even if they fail to load. The navigation was
  // initiated outside of a document, so there is no initiator.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_TRUE(requests[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_FALSE(requests[0].initiator.has_value());

  // Auxiliary navigations have a first-party that matches the URL to which they
  // navigate, and an initiator that matches the document that triggered them.
  EXPECT_EQ(auxiliary_url, requests[1].url);
  EXPECT_TRUE(requests[1].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(auxiliary_url)));
  EXPECT_EQ(top_origin, requests[1].initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, FailedNavigation) {
  // Navigating to this URL will fail, as we haven't taught the host resolver
  // about 'a.com'.
  GURL top_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(1u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate, even if they fail to load. The navigation was
  // initiated outside of a document, so there is no initiator.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_TRUE(requests[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_FALSE(requests[0].initiator.has_value());
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, CrossOriginNested) {
  GURL top_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL top_js_url(
      embedded_test_server()->GetURL("a.com", "/tree_parser_util.js"));
  GURL nested_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b()"));
  GURL nested_js_url(
      embedded_test_server()->GetURL("b.com", "/tree_parser_util.js"));
  url::Origin top_origin = url::Origin::Create(top_url);
  url::Origin nested_origin = url::Origin::Create(nested_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(4u);

  auto requests = data();
  EXPECT_EQ(4u, requests.size());

  // User-initiated top-level navigations have a |first-party|. The navigation
  // was initiated outside of a document, so there are no initiator.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_TRUE(requests[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_FALSE(requests[0].initiator.has_value());

  EXPECT_EQ(top_js_url, requests[1].url);
  EXPECT_TRUE(requests[1].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_EQ(top_origin, requests[1].initiator);

  // Cross-origin frames have a first-party and initiator that matches the URL
  // in which they're embedded (if they are the first cross-origin thing)
  EXPECT_EQ(nested_url, requests[2].url);
  EXPECT_TRUE(requests[2].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(top_origin)));
  EXPECT_EQ(top_origin, requests[2].initiator);

  // Cross-origin subresource requests have a unique first-party, and an
  // initiator that matches the document in which they're embedded.
  EXPECT_EQ(nested_js_url, requests[3].url);
  EXPECT_TRUE(requests[3].site_for_cookies.IsNull());
  EXPECT_EQ(nested_origin, requests[3].initiator);
}

// Regression test for https://crbug.com/648608. An attacker could trivially
// bypass cookies SameSite=Strict protections by navigating a new window twice.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       CookieSameSiteStrictOpenNewNamedWindowTwice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Add cookies for 'a.com', one of them with the "SameSite=Strict" option.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  GURL a_url("http://a.com");
  EXPECT_TRUE(SetCookie(context, a_url, "cookie_A=A; SameSite=Strict;"));
  EXPECT_TRUE(SetCookie(context, a_url, "cookie_B=B"));

  // 2) Navigate to malicious.com.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "malicious.com", "/title1.html")));

  // 2.1) malicious.com opens a new window to 'http://a.com/echoall'.
  GURL echoall_url = embedded_test_server()->GetURL("a.com", "/echoall");
  std::string script = base::StringPrintf("window.open('%s', 'named_frame');",
                                          echoall_url.spec().c_str());
  {
    TestNavigationObserver new_tab_observer(shell()->web_contents(), 1);
    new_tab_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(shell(), script));
    new_tab_observer.Wait();
    ASSERT_EQ(2u, Shell::windows().size());
    Shell* new_shell = Shell::windows()[1];
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    // Only the cookie without "SameSite=Strict" should be sent.
    std::string html_content =
        EvalJs(new_shell, "document.body.textContent").ExtractString();
    EXPECT_THAT(html_content.c_str(), Not(HasSubstr("cookie_A=A")));
    EXPECT_THAT(html_content.c_str(), HasSubstr("cookie_B=B"));
  }

  // 2.2) Same as in 2.1). The difference is that the new tab will be reused.
  {
    Shell* new_shell = Shell::windows()[1];
    TestNavigationObserver new_tab_observer(new_shell->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), script));
    new_tab_observer.Wait();
    ASSERT_EQ(2u, Shell::windows().size());
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    // Only the cookie without "SameSite=Strict" should be sent.
    std::string html_content =
        EvalJs(new_shell, "document.body.textContent").ExtractString();
    EXPECT_THAT(html_content.c_str(), Not(HasSubstr("cookie_A=A")));
    EXPECT_THAT(html_content.c_str(), HasSubstr("cookie_B=B"));
  }
}

class URLModifyingThrottle : public blink::URLLoaderThrottle {
 public:
  URLModifyingThrottle(bool modify_start, bool modify_redirect)
      : modify_start_(modify_start), modify_redirect_(modify_redirect) {}

  URLModifyingThrottle(const URLModifyingThrottle&) = delete;
  URLModifyingThrottle& operator=(const URLModifyingThrottle&) = delete;

  ~URLModifyingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    if (!modify_start_) {
      return;
    }

    GURL::Replacements replacements;
    replacements.SetQueryStr("foo=bar");
    request->url = request->url.ReplaceComponents(replacements);
    request->headers.SetHeader("Foo", "BarRequest");
    request->cors_exempt_headers.SetHeader("ExemptFoo", "ExemptBarRequest");
  }

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override {
    if (!modify_redirect_) {
      return;
    }

    modified_request_headers->SetHeader("Foo", "BarRedirect");
    modified_cors_exempt_request_headers->SetHeader("ExemptFoo",
                                                    "ExemptBarRedirect");

    if (modified_redirect_url_) {
      return;  // Only need to do this once.
    }

    modified_redirect_url_ = true;
    GURL::Replacements replacements;
    replacements.SetQueryStr("foo=bar");
    redirect_info->new_url =
        redirect_info->new_url.ReplaceComponents(replacements);
  }

 private:
  bool modify_start_;
  bool modify_redirect_;
  bool modified_redirect_url_ = false;
};

class ThrottleContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  ThrottleContentBrowserClient(bool modify_start, bool modify_redirect)
      : modify_start_(modify_start), modify_redirect_(modify_redirect) {}

  ThrottleContentBrowserClient(const ThrottleContentBrowserClient&) = delete;
  ThrottleContentBrowserClient& operator=(const ThrottleContentBrowserClient&) =
      delete;

  ~ThrottleContentBrowserClient() override {}

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    auto throttle =
        std::make_unique<URLModifyingThrottle>(modify_start_, modify_redirect_);
    throttles.push_back(std::move(throttle));
    return throttles;
  }

 private:
  bool modify_start_;
  bool modify_redirect_;
};

// Ensures if a URLLoaderThrottle modifies a URL in WillStartRequest the
// new request matches
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, URLLoaderThrottleStartModify) {
  base::Lock lock;
  ThrottleContentBrowserClient content_browser_client(true, false);

  std::set<GURL> urls_requested;
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> header_map;
  embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        base::AutoLock auto_lock(lock);
        urls_requested.insert(request.GetURL());
        header_map[request.GetURL()] = request.headers;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/simple_page.html");
  GURL expected_url(url.spec() + "?foo=bar");
  EXPECT_TRUE(
      NavigateToURL(shell(), url, expected_url /* expected_commit_url */));

  {
    base::AutoLock auto_lock(lock);
    ASSERT_TRUE(urls_requested.find(expected_url) != urls_requested.end());
    ASSERT_TRUE(header_map[expected_url]["Foo"] == "BarRequest");
    ASSERT_TRUE(header_map[expected_url]["ExemptFoo"] == "ExemptBarRequest");
  }
}

// Ensures if a URLLoaderThrottle modifies a URL and headers in
// WillRedirectRequest the new request matches.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, URLLoaderThrottleRedirectModify) {
  base::Lock lock;
  ThrottleContentBrowserClient content_browser_client(false, true);

  std::set<GURL> urls_requested;
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> header_map;
  embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        base::AutoLock auto_lock(lock);
        urls_requested.insert(request.GetURL());
        header_map[request.GetURL()] = request.headers;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url =
      embedded_test_server()->GetURL("/server-redirect?simple_page.html");
  GURL expected_url =
      embedded_test_server()->GetURL("/simple_page.html?foo=bar");
  EXPECT_TRUE(
      NavigateToURL(shell(), url, expected_url /* expected_commit_url */));

  {
    base::AutoLock auto_lock(lock);
    ASSERT_EQ(header_map[expected_url]["Foo"], "BarRedirect");
    ASSERT_EQ(header_map[expected_url]["ExemptFoo"], "ExemptBarRedirect");
    ASSERT_NE(urls_requested.find(expected_url), urls_requested.end());
  }
}

class LoaderNoScriptStreamingBrowserTest : public ContentBrowserTest {
 public:
  LoaderNoScriptStreamingBrowserTest() = default;
  LoaderNoScriptStreamingBrowserTest(
      const LoaderNoScriptStreamingBrowserTest&) = delete;
  LoaderNoScriptStreamingBrowserTest& operator=(
      const LoaderNoScriptStreamingBrowserTest&) = delete;
  ~LoaderNoScriptStreamingBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("js-flags", "--no-script-streaming");
  }
};

// Regression test for https://crbug.com/348520461
// Loading a script should not cause a crash even when Script Streaming
// is disabled on V8 side.
IN_PROC_BROWSER_TEST_F(LoaderNoScriptStreamingBrowserTest, LoadScript) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/loader/blank.html")));
  std::string expected_title("DONE");
  std::u16string expected_title16(ASCIIToUTF16(expected_title));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  ASSERT_TRUE(ExecJs(shell(), R"(
      (() => {
        const script = document.createElement('script');
        script.src = './change_title.js';
        document.body.appendChild(script);
      })();
    )"));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

// Regression test for https://crbug.com/362788339
// Tests that script can be loaded when the server responded 304 response.
// TODO(crbug.com/369439037):  Re-enable once flakiness is resolved for Windows
// ASAN.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_Subresource304Response DISABLED_Subresource304Response
#else
#define MAYBE_Subresource304Response Subresource304Response
#endif
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, MAYBE_Subresource304Response) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/test.html") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_content_type("text/html");
          const size_t kScriptCount = 100;
          std::vector<std::string> html_strings;
          html_strings.emplace_back("<head><title></title><head><script>");
          html_strings.emplace_back("const kScriptCount = ");
          html_strings.emplace_back(base::NumberToString(kScriptCount));
          html_strings.emplace_back(";\n");
          html_strings.emplace_back(R"(
              let count = 0;
              function done() {
                if (++count == kScriptCount) {
                  document.title='Scripts Loaded';
                }
              }
            )");
          html_strings.emplace_back("</script>");
          for (size_t i = 0; i < kScriptCount; ++i) {
            html_strings.emplace_back("<script src=\"./test.js?");
            html_strings.emplace_back(base::NumberToString(i));
            html_strings.emplace_back("\"></script>");
          }
          response->set_content(base::StrCat(html_strings));
          return response;
        } else if (request.relative_url.starts_with("/test.js?")) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          if (request.headers.contains("if-modified-since")) {
            response->set_code(net::HTTP_NOT_MODIFIED);
            return response;
          }
          response->set_content_type("application/javascript");
          response->set_content("done();");
          response->AddCustomHeader("Cache-Control", "max-age=0, no-cache");
          response->AddCustomHeader("pragma", "no-cache");
          response->AddCustomHeader("Last-Modified",
                                    "Wed, 20 Dec 2023 01:00:00 GMT");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());
  {
    std::u16string expected_title(u"Scripts Loaded");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/test.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  {
    {
      std::u16string expected_title(u"Title Cleared");
      TitleWatcher title_watcher(shell()->web_contents(), expected_title);
      EXPECT_EQ("Title Cleared",
                EvalJs(shell(), "document.title = 'Title Cleared';"));
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }
    {
      std::u16string expected_title(u"Scripts Loaded");
      TitleWatcher title_watcher(shell()->web_contents(), expected_title);
      shell()->Reload();
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }
  }
}

}  // namespace content
