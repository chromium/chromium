// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc_security_test_util.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/alternative_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-test-utils.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

void SetCookieFromJS(RenderFrameHost* frame, std::string cookie) {
  EvalJsResult result = EvalJs(frame, "document.cookie = '" + cookie + "'");
  EXPECT_TRUE(result.error.empty()) << result.error;
}

std::string GetCookieFromJS(RenderFrameHost* frame) {
  return EvalJs(frame, "document.cookie;").ExtractString();
}

void SetCookieDirect(WebContentsImpl* tab,
                     const GURL& url,
                     const std::string& cookie_line) {
  net::CookieOptions options;
  // Allow setting SameSite cookies.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  auto cookie_obj = net::CanonicalCookie::Create(
      url, cookie_line, base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  base::RunLoop run_loop;
  tab->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetCanonicalCookie(
          *cookie_obj, url, options,
          base::BindLambdaForTesting(
              [&](net::CookieAccessResult status) { run_loop.Quit(); }));
  run_loop.Run();
}

std::string GetCookiesDirect(WebContentsImpl* tab, const GURL& url) {
  net::CookieOptions options;
  // Allow setting SameSite cookies.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  net::CookieList result;
  base::RunLoop run_loop;
  tab->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(
          url, options, net::CookiePartitionKeyCollection(),
          base::BindLambdaForTesting(
              [&](const net::CookieAccessResultList& cookie_list,
                  const net::CookieAccessResultList& excluded_cookies) {
                result = net::cookie_util::StripAccessResults(cookie_list);
                run_loop.Quit();
              }));
  run_loop.Run();
  return net::CanonicalCookie::BuildCookieLine(result);
}

}  // namespace

class CookieBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Exercises basic cookie operations via javascript, including an http page
// interacting with secure cookies.
IN_PROC_BROWSER_TEST_F(CookieBrowserTest, Cookies) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server.Start());

  // The server sends a HttpOnly cookie. The RestrictedCookieManager should
  // never allow this to be sent to any renderer process.
  GURL https_url =
      https_server.GetURL("a.test", "/set-cookie?notforjs=1;HttpOnly");
  GURL http_url =
      embedded_test_server()->GetURL("a.test", "/frame_with_load_event.html");

  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  EXPECT_TRUE(NavigateToURL(shell2, https_url));

  WebContentsImpl* web_contents_https =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  if (AreDefaultSiteInstancesEnabled()) {
    // Note: Both use the default SiteInstance because the URLs don't require
    // a dedicated process, but these default SiteInstances are not the same
    // object because they come from different BrowsingInstances.
    EXPECT_TRUE(web_contents_http->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_TRUE(web_contents_https->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_NE(web_contents_http->GetSiteInstance(),
              web_contents_https->GetSiteInstance());
    EXPECT_FALSE(web_contents_http->GetSiteInstance()->IsRelatedSiteInstance(
        web_contents_https->GetSiteInstance()));
  } else {
    EXPECT_EQ("http://a.test/",
              web_contents_http->GetSiteInstance()->GetSiteURL().spec());
    EXPECT_EQ("https://a.test/",
              web_contents_https->GetSiteInstance()->GetSiteURL().spec());
  }

  EXPECT_NE(web_contents_http->GetSiteInstance()->GetProcess(),
            web_contents_https->GetSiteInstance()->GetProcess());

  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetPrimaryMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetPrimaryMainFrame()));

  // Non-TLS page writes secure cookie.
  EXPECT_TRUE(ExecJs(web_contents_http->GetPrimaryMainFrame(),
                     "document.cookie = 'A=1; secure;';"));
  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetPrimaryMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetPrimaryMainFrame()));

  // Non-TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecJs(web_contents_http->GetPrimaryMainFrame(),
                     "document.cookie = 'B=2';"));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_https->GetPrimaryMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetPrimaryMainFrame()));

  // TLS page writes secure cookie.
  EXPECT_TRUE(ExecJs(web_contents_https->GetPrimaryMainFrame(),
                     "document.cookie = 'C=3;secure;';"));
  EXPECT_EQ("B=2; C=3",
            GetCookieFromJS(web_contents_https->GetPrimaryMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetPrimaryMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecJs(web_contents_https->GetPrimaryMainFrame(),
                     "document.cookie = 'D=4';"));
  EXPECT_EQ("B=2; C=3; D=4",
            GetCookieFromJS(web_contents_https->GetPrimaryMainFrame()));
  EXPECT_EQ("B=2; D=4",
            GetCookieFromJS(web_contents_http->GetPrimaryMainFrame()));
}

// Ensure "priority" cookie option is settable via document.cookie.
IN_PROC_BROWSER_TEST_F(CookieBrowserTest, CookiePriority) {
  ASSERT_TRUE(embedded_test_server()->Start());

  struct {
    std::string param;
    net::CookiePriority priority;
  } cases[] = {{"name=value", net::COOKIE_PRIORITY_DEFAULT},
               {"name=value;priority=Low", net::COOKIE_PRIORITY_LOW},
               {"name=value;priority=Medium", net::COOKIE_PRIORITY_MEDIUM},
               {"name=value;priority=High", net::COOKIE_PRIORITY_HIGH}};

  for (auto test_case : cases) {
    GURL url = embedded_test_server()->GetURL("/set_document_cookie.html?" +
                                              test_case.param);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    std::vector<net::CanonicalCookie> cookies =
        GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(), url);

    EXPECT_EQ(1u, cookies.size());
    EXPECT_EQ("name", cookies[0].Name());
    EXPECT_EQ("value", cookies[0].Value());
    EXPECT_EQ(test_case.priority, cookies[0].Priority());
  }
}

// SameSite cookies (that aren't marked as http-only) should be available to
// JavaScript.
IN_PROC_BROWSER_TEST_F(CookieBrowserTest, SameSiteCookies) {
  // Must use HTTPS because SameSite=None cookies must be Secure.
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  server.AddDefaultHandlers(GetTestDataFilePath());
  SetupCrossSiteRedirector(&server);
  ASSERT_TRUE(server.Start());

  // The server sets eight cookies on 'a.test' and on 'b.test', then loads
  // a page that frames both 'a.test' and 'b.test' under 'a.test'.
  std::string cookies_to_set =
      "/set-cookie?none=1;SameSite=None;Secure"  // SameSite=None must be
                                                 // Secure.
      "&none-insecure=1;SameSite=None"
      "&strict=1;SameSite=Strict"
      "&unspecified=1"  // unspecified SameSite should be treated as Lax.
      "&lax=1;SameSite=Lax"
      "&none-http=1;SameSite=None;Secure;httponly"
      "&strict-http=1;SameSite=Strict;httponly"
      "&unspecified-http=1;httponly"
      "&lax-http=1;SameSite=Lax;httponly";

  GURL url = server.GetURL("a.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  url = server.GetURL("b.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  url = server.GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(a.test(),b.test())");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  RenderFrameHost* a_iframe = web_contents->GetPrimaryFrameTree()
                                  .root()
                                  ->child_at(0)
                                  ->current_frame_host();
  RenderFrameHost* b_iframe = web_contents->GetPrimaryFrameTree()
                                  .root()
                                  ->child_at(1)
                                  ->current_frame_host();

  // The top-level frame should get all same-site cookies.
  EXPECT_EQ("none=1; strict=1; unspecified=1; lax=1",
            GetCookieFromJS(main_frame));

  // Same-site cookies will be delievered to the 'a.com' frame, as it is same-
  // site with its ancestors.
  EXPECT_EQ("none=1; strict=1; unspecified=1; lax=1",
            GetCookieFromJS(a_iframe));

  // Same-site cookies should not be delievered to the 'b.com' frame, as it
  // isn't same-site with its ancestors. The SameSite=None but insecure cookie
  // is rejected.
  EXPECT_EQ("none=1", GetCookieFromJS(b_iframe));
}

IN_PROC_BROWSER_TEST_F(CookieBrowserTest, CookieTruncatingChar) {
  using std::string_literals::operator""s;

  std::string cookie_string;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->AddCustomHeader("Set-Cookie", cookie_string);
        return std::move(response);
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL http_url = embedded_test_server()->GetURL("/");
  base::HistogramTester histogram;

  // Test scenarios where a control char may appear at start, middle and end of
  // a cookie line. Control char array with NULL (\x0), CR (\xD), and LF (xA)
  char kTestChars[] = {'\x0', '\xD', '\xA'};

  for (const auto& test : kTestChars) {
    std::string ctl_string(1, test);

    // ctrl char at start of string
    cookie_string = ctl_string + "foo=bar"s;
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    // ctrl char at middle of string
    cookie_string = "foo=bar;"s + ctl_string + "httponly"s;
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    // ctrl char at end of string
    cookie_string = "foo=bar;"s + "httponly;"s + ctl_string;
    EXPECT_TRUE(NavigateToURL(shell(), http_url));
  }
  // Test if there are multiple control characters that terminate.
  cookie_string = "foo=bar;\xA\xDhttponly"s;
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FetchHistogramsFromChildProcesses();
  histogram.ExpectBucketCount(
      "Cookie.TruncatingCharacterInCookieString",
      net::TruncatingCharacterInCookieStringType::kTruncatingCharNull, 0);
  histogram.ExpectBucketCount(
      "Cookie.TruncatingCharacterInCookieString",
      net::TruncatingCharacterInCookieStringType::kTruncatingCharNewline, 0);
  histogram.ExpectBucketCount(
      "Cookie.TruncatingCharacterInCookieString",
      net::TruncatingCharacterInCookieStringType::kTruncatingCharLineFeed, 0);
}

class RestrictedCookieManagerInterceptor
    : public network::mojom::RestrictedCookieManagerInterceptorForTesting {
 public:
  RestrictedCookieManagerInterceptor(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      mojo::PendingRemote<network::mojom::RestrictedCookieManager> real_rcm)
      : receiver_(this, std::move(receiver)), real_rcm_(std::move(real_rcm)) {}

  void set_override_url(absl::optional<std::string> maybe_url) {
    override_url_ = std::move(maybe_url);
  }

  void SetCookieFromString(const GURL& url,
                           const net::SiteForCookies& site_for_cookies,
                           const url::Origin& top_frame_origin,
                           const std::string& cookie,
                           SetCookieFromStringCallback callback) override {
    GetForwardingInterface()->SetCookieFromString(
        URLToUse(url), site_for_cookies, top_frame_origin, std::move(cookie),
        std::move(callback));
  }

  void GetCookiesString(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        GetCookiesStringCallback callback) override {
    GetForwardingInterface()->GetCookiesString(
        URLToUse(url), site_for_cookies, top_frame_origin, std::move(callback));
  }

 private:
  network::mojom::RestrictedCookieManager* GetForwardingInterface() override {
    return real_rcm_.get();
  }

  GURL URLToUse(const GURL& url_in) {
    return override_url_ ? GURL(override_url_.value()) : url_in;
  }

  absl::optional<std::string> override_url_;

  mojo::Receiver<network::mojom::RestrictedCookieManager> receiver_;
  mojo::Remote<network::mojom::RestrictedCookieManager> real_rcm_;
};

class CookieStoreContentBrowserClient : public ContentBrowserClient {
 public:
  ~CookieStoreContentBrowserClient() override = default;

  bool WillCreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      bool is_service_worker,
      int process_id,
      int routing_id,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver)
      override {
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>
        orig_receiver = std::move(*receiver);

    mojo::PendingRemote<network::mojom::RestrictedCookieManager> real_rcm;
    *receiver = real_rcm.InitWithNewPipeAndPassReceiver();

    rcm_interceptor_ = std::make_unique<RestrictedCookieManagerInterceptor>(
        std::move(orig_receiver), std::move(real_rcm));
    rcm_interceptor_->set_override_url(override_url_);

    return false;  // only made a proxy, still need the actual impl to be made.
  }

  void set_override_url(absl::optional<std::string> maybe_url) {
    override_url_ = maybe_url;
    if (rcm_interceptor_)
      rcm_interceptor_->set_override_url(override_url_);
  }

 private:
  absl::optional<std::string> override_url_;
  std::unique_ptr<RestrictedCookieManagerInterceptor> rcm_interceptor_;
};

// Cookie access in loader is locked to a particular origin, so messages
// for wrong URLs are rejected.
// TODO(https://crbug.com/954603): This should actually result in renderer
// kills.
IN_PROC_BROWSER_TEST_F(CookieBrowserTest, CrossSiteCookieSecurityEnforcement) {
  // The code under test is only active under site isolation.
  if (!AreAllSitesIsolatedForTesting()) {
    return;
  }

  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_with_load_event.html")));

  WebContentsImpl* tab = static_cast<WebContentsImpl*>(shell()->web_contents());

  // The iframe on the http page should get its own process.
  FrameTreeVisualizer v;
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetPrimaryFrameTree().root()));

  RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  RenderFrameHost* iframe =
      tab->GetPrimaryFrameTree().root()->child_at(0)->current_frame_host();

  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  SetCookieDirect(tab, GURL("http://127.0.0.1/"), "A_cookie = parent");
  SetCookieDirect(tab, GURL("http://baz.com/"), "B_cookie = child");
  EXPECT_EQ("A_cookie=parent",
            GetCookiesDirect(tab, GURL("http://127.0.0.1/")));

  EXPECT_EQ("B_cookie=child", GetCookiesDirect(tab, GURL("http://baz.com/")));

  // Try to get cross-site cookies from the subframe's process.
  {
    CookieStoreContentBrowserClient browser_client;
    content::ContentBrowserClient* old_browser_client =
        content::SetBrowserClientForTesting(&browser_client);
    browser_client.set_override_url("http://127.0.0.1/");
    EXPECT_EQ("", GetCookieFromJS(iframe));

    content::SetBrowserClientForTesting(old_browser_client);
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetPrimaryFrameTree().root()));

  // Now set a cross-site cookie from the main frame's process.
  {
    CookieStoreContentBrowserClient browser_client;
    content::ContentBrowserClient* old_browser_client =
        content::SetBrowserClientForTesting(&browser_client);

    browser_client.set_override_url("https://baz.com/");
    SetCookieFromJS(iframe, "pwn=ed");

    EXPECT_EQ("B_cookie=child", GetCookiesDirect(tab, GURL("http://baz.com/")));

    content::SetBrowserClientForTesting(old_browser_client);
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetPrimaryFrameTree().root()));
}

}  // namespace content
