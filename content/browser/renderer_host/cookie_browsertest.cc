// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/alternative_service.h"
#include "net/storage_access_api/status.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-test-utils.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

using testing::IsEmpty;
using testing::Key;
using testing::UnorderedElementsAre;

namespace content {

namespace {

void EnableDevtoolsThirdPartyCookieRestriction(
    TestDevToolsProtocolClient& frame_devtools_client) {
  base::Value::Dict command_params;
  frame_devtools_client.SendCommandSync("Network.enable");
  command_params.Set("enableThirdPartyCookieRestriction", true);
  command_params.Set("disableThirdPartyCookieMetadata", false);
  command_params.Set("disableThirdPartyCookieHeuristics", false);
  frame_devtools_client.SendCommandAsync("Network.setCookieControls",
                                         std::move(command_params));
}

void SetCookieFromJS(RenderFrameHost* frame, std::string cookie) {
  EvalJsResult result = EvalJs(frame, "document.cookie = '" + cookie + "'");
  EXPECT_TRUE(result.is_ok()) << result;
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

  auto cookie_obj = net::CanonicalCookie::CreateForTesting(url, cookie_line,
                                                           base::Time::Now());

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

class CookieBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CookieBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatureStates(
        {{network::features::kGetCookiesOnSet, GetCookiesOnSetEnabled()},
         {blink::features::kAsyncSetCookie, AsyncSetCookieEnabled()}});
  }
  ~CookieBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  bool GetCookiesOnSetEnabled() { return std::get<0>(GetParam()); }

  bool AsyncSetCookieEnabled() { return std::get<1>(GetParam()); }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CookieBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      std::string name =
          std::get<0>(info.param) ? "GetOnSetEnabled" : "GetOnSetDisabled";
      name += "_";
      name += std::get<1>(info.param) ? "Async" : "Sync";
      return name;
    });

// Exercises basic cookie operations via javascript, including an http page
// interacting with secure cookies.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, Cookies) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // The server sends a HttpOnly cookie. The RestrictedCookieManager should
  // never allow this to be sent to any renderer process.
  GURL https_url =
      https_server_.GetURL("a.test", "/set-cookie?notforjs=1;HttpOnly");
  GURL http_url =
      embedded_test_server()->GetURL("a.test", "/frame_with_load_event.html");

  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  EXPECT_TRUE(NavigateToURL(shell2, https_url));

  WebContentsImpl* web_contents_https =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_EQ("http://a.test/",
              web_contents_http->GetSiteInstance()->GetSiteURL().spec());
    // Create expected site url, including port if origin isolation is enabled.
    std::string expected_site_url =
        SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()
            ? url::Origin::Create(https_url).GetURL().spec()
            : std::string("https://a.test/");
    EXPECT_EQ(expected_site_url,
              web_contents_https->GetSiteInstance()->GetSiteURL().spec());
  } else {
    // Note: Both use the default SiteInstance because the URLs don't require
    // a dedicated process, but these default SiteInstances are not the same
    // object because they come from different BrowsingInstances.
    EXPECT_TRUE(web_contents_http->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_TRUE(web_contents_https->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_NE(web_contents_http->GetSiteInstance(),
              web_contents_https->GetSiteInstance());
    EXPECT_FALSE(web_contents_http->GetSiteInstance()->IsRelatedSiteInstance(
        web_contents_https->GetSiteInstance()));
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
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetPrimaryMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_https->GetPrimaryMainFrame()));

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
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, CookiePriority) {
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
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, SameSiteCookies) {
  // Must use HTTPS because SameSite=None cookies must be Secure.

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

  GURL url = https_server_.GetURL("a.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  url = https_server_.GetURL("b.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  url = https_server_.GetURL(
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

// Prefixed cookies (that aren't marked as http-only) should be available to
// JavaScript.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, PrefixedCookies_Read) {
  // Must use HTTPS because prefixed cookies must be Secure.

  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        https_server_.GetURL("a.test", "/"),
                        "__Host-cookie=1;Secure;Path=/"));
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        https_server_.GetURL("a.test", "/"),
                        "__Secure-cookie=1;Secure"));
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        https_server_.GetURL("a.test", "/"),
                        "__Secure-http-cookie=1;Secure;HttpOnly"));

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server_.GetURL("a.test", "/empty.html")));

  EXPECT_THAT(EvalJs(shell(), "document.cookie").ExtractString(),
              net::CookieStringIs(UnorderedElementsAre(
                  Key("__Host-cookie"), Key("__Secure-cookie"))));
}

IN_PROC_BROWSER_TEST_P(CookieBrowserTest, PrefixedCookies_Read_Insecure) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        https_server_.GetURL("a.test", "/"),
                        "__Host-cookie=1;Secure;Path=/"));
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        https_server_.GetURL("a.test", "/"),
                        "__Secure-cookie=1;Secure"));
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        https_server_.GetURL("a.test", "/"),
                        "__Secure-http-cookie=1;Secure;HttpOnly"));

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/empty.html")));

  EXPECT_EQ(EvalJs(shell(), "document.cookie"), "");
}

// Prefixed cookies should be writable by JavaScript.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, PrefixedCookies_Write) {
  // Must use HTTPS because prefixed cookies must be Secure.

  EXPECT_TRUE(
      NavigateToURL(shell(), https_server_.GetURL("a.test", "/empty.html")));

  EXPECT_TRUE(ExecJs(shell(), R"js(
    // Valid cookies:
    document.cookie = "__Host-cookie=1;Secure;Path=/";
    document.cookie = "__Secure-cookie=1;Secure";
    // Invalid cookies:
    document.cookie = "__Secure-http-cookie=1;Secure;HttpOnly";
    document.cookie = "__Secure-missing-attr=1";
    document.cookie = "__Host-wrong-path=1;Secure;";
    document.cookie = "__Host-wrong-domain=1;Secure;Domain=a.test";
    document.cookie = "__Host-wrong-secure=1;Path=/";
    )js"));

  EXPECT_THAT(GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(),
                                  https_server_.GetURL("a.test", "/")),
              UnorderedElementsAre(
                  net::MatchesCookieNameValue("__Host-cookie", "1"),
                  net::MatchesCookieNameValue("__Secure-cookie", "1")));
}

IN_PROC_BROWSER_TEST_P(CookieBrowserTest, PrefixedCookies_Write_Insecure) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/empty.html")));

  EXPECT_TRUE(ExecJs(shell(), R"js(
    document.cookie = "__Host-cookie=1;Secure;Path=/";
    document.cookie = "__Secure-cookie=1;Secure";
    document.cookie = "__Secure-http-cookie=1;Secure;HttpOnly";
    document.cookie = "__Secure-missing-attr=1";
    document.cookie = "__Host-wrong-path=1;Secure;";
    document.cookie = "__Host-wrong-domain=1;Secure;Domain=a.test";
    document.cookie = "__Host-wrong-secure=1;Path=/";
    )js"));

  EXPECT_THAT(
      GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(),
                          embedded_test_server()->GetURL("a.test", "/")),
      IsEmpty());
}

// embedded_test_server() uses http, which is insecure, but localhost is
// allowed to set prefixed cookies anyway.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, PrefixedCookies_Write_Localhost) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("localhost", "/empty.html")));

  EXPECT_TRUE(ExecJs(shell(), R"js(
    document.cookie = "__Host-cookie=1;Secure;Path=/";
    document.cookie = "__Secure-cookie=1;Secure";
    document.cookie = "__Secure-http-cookie=1;Secure;HttpOnly";
    document.cookie = "__Secure-missing-attr=1";
    document.cookie = "__Host-wrong-path=1;Secure;";
    document.cookie = "__Host-wrong-domain=1;Secure;Domain=a.test";
    document.cookie = "__Host-wrong-secure=1;Path=/";
    )js"));

  EXPECT_THAT(
      GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(),
                          embedded_test_server()->GetURL("localhost", "/")),
      UnorderedElementsAre(
          net::MatchesCookieNameValue("__Host-cookie", "1"),
          net::MatchesCookieNameValue("__Secure-cookie", "1")));
}

IN_PROC_BROWSER_TEST_P(CookieBrowserTest,
                       CookieJarInvalidatesCacheWithNewDevtoolsControls) {
  // Must use HTTPS because SameSite=None cookies must be Secure.

  // Set a single cookie that we'll access from a third-party context
  std::string cookies_to_set =
      "/set-cookie?none=1;SameSite=None;Secure";  // SameSite=None must be
                                                  // Secure

  GURL url = https_server_.GetURL("b.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // Turn on third-party cookie restriction from devtools. This needs to happen
  // from a top level client
  TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(web_contents);
  EnableDevtoolsThirdPartyCookieRestriction(devtools_client);

  url = https_server_.GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(b.test())");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* oop_iframe = web_contents->GetPrimaryFrameTree()
                                    .root()
                                    ->child_at(0)
                                    ->current_frame_host();

  // Attach devtools client to the sub frame, but disable the controls at first
  devtools_client.DetachProtocolClient();
  devtools_client.AttachToFrameTreeHost(oop_iframe);
  devtools_client.SendCommandSync("Network.disable");

  // Check Get->Get
  // Overrides should not apply after disabling the controls
  EXPECT_EQ("none=1", GetCookieFromJS(oop_iframe));

  // Confirm cache is invalidated by observing new value from document.cookie
  // when re-enabling devtools
  devtools_client.SendCommandSync("Network.enable");
  EXPECT_EQ("", GetCookieFromJS(oop_iframe));

  // Check Set->Get
  // Set a cookie with devtools disabled
  devtools_client.SendCommandSync("Network.disable");
  SetCookieFromJS(oop_iframe, "none=2; SameSite=None; Secure");

  // Confirm cache is invalidated by observing no cookie from document.cookie
  // when re-enabling devtools
  devtools_client.SendCommandSync("Network.enable");
  EXPECT_EQ("", GetCookieFromJS(oop_iframe));

  devtools_client.DetachProtocolClient();
}

IN_PROC_BROWSER_TEST_P(CookieBrowserTest, CookieTruncatingCharFromJavascript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  WebContentsImpl* tab = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* frame = tab->GetPrimaryMainFrame();

  // Test scenarios where a control char may appear at start, middle and end of
  // a cookie line. Control char array with NULL (\x0), CR (\xD), and LF (xA).
  const std::string kTestChars[] = {"\\x00", "\\x0D", "\\x0A"};

  for (const std::string& ctl_string : kTestChars) {
    // Control char at the start of the string.
    // Note that when truncation of this cookie string occurs, no histogram
    // entries get recorded because the code bails out early on the resulting
    // empty cookie string.
    std::string cookie_string = base::StrCat({ctl_string, "foo1=bar"});
    SetCookieFromJS(frame, cookie_string);

    // Control char in the middle of the string.
    cookie_string = base::StrCat({"foo2=bar;", ctl_string, "httponly"});
    SetCookieFromJS(frame, cookie_string);

    cookie_string = base::StrCat({"foo3=ba", ctl_string, "r; httponly"});
    SetCookieFromJS(frame, cookie_string);

    // Control char at the end of the string.
    cookie_string = base::StrCat({"foo4=bar;", ctl_string});
    SetCookieFromJS(frame, cookie_string);
  }

  EXPECT_EQ("", GetCookieFromJS(frame));
}

IN_PROC_BROWSER_TEST_P(CookieBrowserTest, CookieTruncatingCharFromHeaders) {
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

  // Test scenarios where a control char may appear at start, middle and end of
  // a cookie line. Control char array with NULL (\x0), CR (\xD), and LF (xA)
  char kTestChars[] = {'\x0', '\xD', '\xA'};

  for (const auto& test : kTestChars) {
    std::string ctl_string(1, test);

    // ctrl char at start of string
    cookie_string = base::StrCat({ctl_string, "foo=bar"});
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    // ctrl char at middle of string
    cookie_string = base::StrCat({"foo=bar;", ctl_string, "httponly"});
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    cookie_string = base::StrCat({"foo=ba", ctl_string, "r; httponly"});
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    // ctrl char at end of string
    cookie_string = base::StrCat({"foo=bar;", "httponly;", ctl_string});
    EXPECT_TRUE(NavigateToURL(shell(), http_url));
  }
  // Test if there are multiple control characters that terminate.
  cookie_string = "foo=bar;\xA\xDhttponly";
  EXPECT_TRUE(NavigateToURL(shell(), http_url));
}

class RestrictedCookieManagerInterceptor
    : public network::mojom::RestrictedCookieManagerInterceptorForTesting {
 public:
  RestrictedCookieManagerInterceptor(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      mojo::PendingRemote<network::mojom::RestrictedCookieManager> real_rcm)
      : receiver_(this, std::move(receiver)), real_rcm_(std::move(real_rcm)) {}

  void set_override_url(std::optional<std::string> maybe_url) {
    override_url_ = std::move(maybe_url);
  }

  void SetCookieFromString(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool get_version_shared_memory,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      const std::string& cookie,
      SetCookieFromStringCallback callback) override {
    GetForwardingInterface()->SetCookieFromString(
        URLToUse(url), site_for_cookies, top_frame_origin,
        storage_access_api_status, get_version_shared_memory, is_ad_tagged,
        apply_devtools_overrides, std::move(cookie), std::move(callback));
  }

  void GetCookiesString(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        net::StorageAccessApiStatus storage_access_api_status,
                        bool get_version_shared_memory,
                        bool is_ad_tagged,
                        bool force_disable_third_party_cookies,
                        bool apply_devtools_overrides,
                        GetCookiesStringCallback callback) override {
    GetForwardingInterface()->GetCookiesString(
        URLToUse(url), site_for_cookies, top_frame_origin,
        storage_access_api_status, get_version_shared_memory, is_ad_tagged,
        force_disable_third_party_cookies, apply_devtools_overrides,
        std::move(callback));
  }

 private:
  network::mojom::RestrictedCookieManager* GetForwardingInterface() override {
    return real_rcm_.get();
  }

  GURL URLToUse(const GURL& url_in) {
    return override_url_ ? GURL(override_url_.value()) : url_in;
  }

  std::optional<std::string> override_url_;

  mojo::Receiver<network::mojom::RestrictedCookieManager> receiver_;
  mojo::Remote<network::mojom::RestrictedCookieManager> real_rcm_;
};

class CookieStoreContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
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

  void set_override_url(std::optional<std::string> maybe_url) {
    override_url_ = maybe_url;
    if (rcm_interceptor_)
      rcm_interceptor_->set_override_url(override_url_);
  }

 private:
  std::optional<std::string> override_url_;
  std::unique_ptr<RestrictedCookieManagerInterceptor> rcm_interceptor_;
};

// Cookie access in loader is locked to a particular origin, so messages
// for wrong URLs are rejected.
// TODO(crbug.com/41453892): This should actually result in renderer
// kills.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, CrossSiteCookieSecurityEnforcement) {
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
    browser_client.set_override_url("http://127.0.0.1/");
    EXPECT_EQ("", GetCookieFromJS(iframe));
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

    browser_client.set_override_url("https://baz.com/");
    SetCookieFromJS(iframe, "pwn=ed");

    EXPECT_EQ("B_cookie=child", GetCookiesDirect(tab, GURL("http://baz.com/")));
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetPrimaryFrameTree().root()));
}

IN_PROC_BROWSER_TEST_P(CookieBrowserTest, CookieNotReadableAfterExpiry) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL http_url = embedded_test_server()->GetURL("example.test", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* frame = web_contents_http->GetPrimaryMainFrame();

  SetCookieFromJS(frame, "c=1;Max-Age=1");
  SetCookieFromJS(frame, "d=1;Max-Age=7200");
  EXPECT_EQ("c=1; d=1", GetCookieFromJS(frame));

  // If cookies properly expire and become unavailable this test will terminate.
  // If they do not the test will time out. The earliest expiry from the cookies
  // is used so the short expiry from c is expected to be used.
  std::string cookie;
  do {
    cookie = GetCookieFromJS(frame);
    base::PlatformThread::Sleep(base::Milliseconds(100));
  } while (cookie != "d=1");
}

// Cookies for an eTLD should be stored (via JS) if they match the URL host,
// even if they begin with `.` or have non-canonical capitalization.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, ETldDomainCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This test uses `gov.br` as an example of an eTLD.
  GURL http_url = embedded_test_server()->GetURL("gov.br", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* frame = web_contents_http->GetPrimaryMainFrame();

  const char* kCases[] = {
      // A host cookie.
      "c=1",
      // A cookie for this domain.
      "c=1; domain=gov.br",
      // Same, but with a preceding dot. This dot should be ignored.
      "c=1; domain=.gov.br",
      // Same, but with non-canonical case. This should be canonicalized.
      "c=1; domain=gOv.bR",
  };

  for (const char* set_cookie : kCases) {
    SCOPED_TRACE(set_cookie);
    SetCookieFromJS(frame, set_cookie);
    EXPECT_EQ("c=1", GetCookieFromJS(frame));
    SetCookieFromJS(frame, "c=;expires=Thu, 01 Jan 1970 00:00:00 GMT");
    EXPECT_EQ("", GetCookieFromJS(frame));
  }
}

// Cookies for an eTLD should be stored (via header) if they match the URL host,
// even if they begin with `.` or have non-canonical capitalization.
IN_PROC_BROWSER_TEST_P(CookieBrowserTest, ETldDomainCookiesHeader) {
  std::string got_cookie_on_request;
  std::string set_cookie_on_response;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.headers.contains("Cookie")) {
          got_cookie_on_request = request.headers.at("Cookie");
        } else {
          got_cookie_on_request = "";
        }
        if (set_cookie_on_response.size() != 0) {
          response->AddCustomHeader("Set-Cookie", set_cookie_on_response);
          set_cookie_on_response = "";
        }
        return std::move(response);
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  // This test uses `gov.br` as an example of an eTLD.
  GURL http_url = embedded_test_server()->GetURL("gov.br", "/empty.html");

  const char* kCases[] = {
      // A host cookie.
      "c=1",
      // A cookie for this domain.
      "c=1; domain=gov.br",
      // Same, but with a preceding dot. This dot should be ignored.
      "c=1; domain=.gov.br",
      // Same, but with non-canonical case. This should be canonicalized.
      "c=1; domain=gOv.bR",
  };

  for (const char* set_cookie : kCases) {
    SCOPED_TRACE(set_cookie);

    set_cookie_on_response = set_cookie;
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    EXPECT_TRUE(NavigateToURL(shell(), http_url));
    EXPECT_EQ("c=1", got_cookie_on_request);

    set_cookie_on_response = "c=;expires=Thu, 01 Jan 1970 00:00:00 GMT";
    EXPECT_TRUE(NavigateToURL(shell(), http_url));

    EXPECT_TRUE(NavigateToURL(shell(), http_url));
    EXPECT_EQ("", got_cookie_on_request);
  }
}

enum class CookieFileMode { kDefault, kEnabled, kDisabled };

class CookieFileBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<CookieFileMode> {
 protected:
  void SetUpOnMainThread() override {
    // Setup file url.
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(file_directory_.CreateUniqueTempDir());
    base::FilePath file_path =
        file_directory_.GetPath().AppendASCII("index.html");
    EXPECT_TRUE(base::WriteFile(file_path, ""));
    file_url_ = net::FilePathToFileURL(file_path);

    // Setup cookie manager.
    bool file_cookie_enabled;
    switch (GetParam()) {
      case CookieFileMode::kDefault:
        // Nothing to do.
        return;
      case CookieFileMode::kEnabled:
        file_cookie_enabled = true;
        break;
      case CookieFileMode::kDisabled:
        file_cookie_enabled = false;
        break;
    }
    base::RunLoop run_loop;
    shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->AllowFileSchemeCookies(file_cookie_enabled,
                                 base::BindLambdaForTesting([&](bool success) {
                                   EXPECT_TRUE(success);
                                   run_loop.Quit();
                                 }));
    run_loop.Run();
  }

  GURL file_url_;

 private:
  base::ScopedTempDir file_directory_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CookieFileBrowserTest,
                         ::testing::Values(CookieFileMode::kDefault,
                                           CookieFileMode::kEnabled,
                                           CookieFileMode::kDisabled));

// Try to set and get cookies on a file URL.
IN_PROC_BROWSER_TEST_P(CookieFileBrowserTest, SetAndGetCookie) {
  // Navigate to file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url_));
  RenderFrameHost* frame = shell()->web_contents()->GetPrimaryMainFrame();

  // File cookies always appear to be writable. On non-Android platforms a
  // warning is printed when this occurs.
#if !BUILDFLAG(IS_ANDROID)
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "While navigator.cookieEnabled does return true for this file:// "
      "URL, this is done for web compatability reasons. Cookies will not "
      "actually be stored for file:// URLs. If you want this to change "
      "please leave feedback on crbug.com/378604901.");
#endif
  EXPECT_TRUE(EvalJs(frame, "navigator.cookieEnabled").ExtractBool());
#if !BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(console_observer.Wait());
#endif

  // File cookies can only be set if they are enabled.
  bool can_set_cookies;
  switch (GetParam()) {
    case CookieFileMode::kDefault:
      // TODO(crbug.com/378604901): Perhapse this should be allowed by default.
      can_set_cookies = false;
      return;
    case CookieFileMode::kEnabled:
      can_set_cookies = true;
      break;
    case CookieFileMode::kDisabled:
      can_set_cookies = false;
      break;
  }
  SetCookieFromJS(frame, "test=1");
  EXPECT_EQ(can_set_cookies ? "test=1" : "", GetCookieFromJS(frame));
}

}  // namespace content
