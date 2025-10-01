// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/frame_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::UnorderedElementsAre;

// This file contains tests for cookie access via HTTP requests.
// See also (tests for cookie access via JavaScript):
// //content/browser/renderer_host/cookie_browsertest.cc

constexpr std::string_view kHostA = "a.test";
constexpr std::string_view kHostB = "b.test";
constexpr std::string_view kHostC = "c.test";
constexpr std::string_view kSameSiteNoneCookieName = "samesite_none_cookie";
constexpr std::string_view kSameSiteStrictCookieName = "samesite_strict_cookie";
constexpr std::string_view kSameSiteLaxCookieName = "samesite_lax_cookie";
constexpr std::string_view kSameSiteUnspecifiedCookieName =
    "samesite_unspecified_cookie";
constexpr std::string_view kHostPrefixCookieName = "__Host-prefixed_cookie";
constexpr std::string_view kSecurePrefixCookieName = "__Secure-prefixed_cookie";
constexpr std::string_view kEchoCookiesWithCorsPath = "/echocookieswithcors";

std::string FrameTreeForHostAndUrl(std::string_view host, const GURL& url) {
  return base::StrCat({host, "(", url.spec(), ")"});
}

std::string FrameTreeForUrl(const GURL& url) {
  return FrameTreeForHostAndUrl(kHostA, url);
}

GURL RedirectUrl(net::EmbeddedTestServer* test_server,
                 std::string_view host,
                 const GURL& target_url) {
  return test_server->GetURL(host, "/server-redirect?" + target_url.spec());
}

class HttpCookieBrowserTest : public ContentBrowserTest,
                              public ::testing::WithParamInterface<bool> {
 public:
  HttpCookieBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatureState(
        net::features::kCookieSameSiteConsidersRedirectChain,
        DoesSameSiteConsiderRedirectChain());
  }

  ~HttpCookieBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool DoesSameSiteConsiderRedirectChain() { return GetParam(); }

  const std::string kSetSameSiteCookiesPath = base::StrCat({
      "/set-cookie?",
      kSameSiteStrictCookieName,
      "=1;SameSite=Strict&",
      kSameSiteLaxCookieName,
      "=1;SameSite=Lax&",
      kSameSiteUnspecifiedCookieName,
      "=1&",
      kSameSiteNoneCookieName,
      "=1;Secure;SameSite=None",
  });

  void SetSameSiteCookies(std::string_view host) {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    EXPECT_TRUE(SetCookie(
        context, https_server()->GetURL(host, "/"),
        base::StrCat({kSameSiteStrictCookieName, "=1; samesite=strict"})));
    EXPECT_TRUE(
        SetCookie(context, https_server()->GetURL(host, "/"),
                  base::StrCat({kSameSiteLaxCookieName, "=1; samesite=lax"})));
    EXPECT_TRUE(SetCookie(
        context, https_server()->GetURL(host, "/"),
        base::StrCat({kSameSiteNoneCookieName, "=1; samesite=none; secure"})));
    EXPECT_TRUE(
        SetCookie(context, https_server()->GetURL(host, "/"),
                  base::StrCat({kSameSiteUnspecifiedCookieName, "=1"})));
  }

  // Gets a path that causes the EmbeddedTestServer to attempt to set prefixed
  // cookies (some valid, some not).
  std::string GetSetPrefixedCookiesPath(std::string_view host) {
    return base::StrCat({
        "/set-cookie?",
        kSecurePrefixCookieName,
        "=1;Secure&",
        kHostPrefixCookieName,
        "=1;Secure;Path=/&",
        "__Secure-missing-attr=1&",
        "__Host-wrong-path=1;Secure&",
        "__Host-wrong-domain=1;Secure;Domain=",
        host,
        "&",
        "__Host-wrong-secure=1;Path=/&",
    });
  }

  // Sets some (valid) prefixed cookies.
  void SetPrefixedCookies(std::string_view host,
                          net::EmbeddedTestServer* test_server = nullptr) {
    if (!test_server) {
      test_server = https_server();
    }
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    ASSERT_TRUE(
        SetCookie(context, test_server->GetURL(host, "/"),
                  base::StrCat({kHostPrefixCookieName, "=1; Secure; Path=/"})));
    ASSERT_TRUE(
        SetCookie(context, test_server->GetURL(host, "/"),
                  base::StrCat({kSecurePrefixCookieName, "=1; Secure"})));
  }

  GURL EchoCookiesUrl(net::EmbeddedTestServer* test_server,
                      std::string_view host) {
    return test_server->GetURL(host, "/echoheader?Cookie");
  }

  GURL SetSameSiteCookiesUrl(net::EmbeddedTestServer* test_server,
                             std::string_view host) {
    return test_server->GetURL(host, kSetSameSiteCookiesPath);
  }

  std::string ExtractFrameContent(RenderFrameHost* frame) const {
    return EvalJs(frame, "document.body.textContent").ExtractString();
  }

  uint32_t ClearCookies() {
    return DeleteCookies(shell()->web_contents()->GetBrowserContext(),
                         network::mojom::CookieDeletionFilter());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  WebContents* web_contents() { return shell()->web_contents(); }

  // is_user_initiated_navigation - true if user initiates navigation to 404,
  //                              - false script initiates this navigation
  // is_cross_site_navigation - true if testing in a cross-site context,
  //                          - false if testing in a same-site context
  // is_user_initiated_reload - true if user initates the reload,
  //                            false if the reload via script
  std::string Test404ReloadCookie(bool is_user_initiated_navigation,
                                  bool is_cross_site_navigation,
                                  bool is_user_initiated_reload) {
    // Set target as A or B and cookies based on cross_site param
    std::string_view target = is_cross_site_navigation ? kHostB : kHostA;
    GURL target_URL =
        https_server()->GetURL(target, "/echo-cookie-with-status?status=404");
    SetSameSiteCookies(target);

    // Start at a website A
    EXPECT_TRUE(
        NavigateToURL(web_contents(), EchoCookiesUrl(https_server(), kHostA)));

    // Navigate method based on whether user or script initiated
    if (is_user_initiated_navigation) {
      EXPECT_TRUE(NavigateToURL(web_contents(), target_URL));
    } else {
      EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), target_URL));
    }

    // Trigger either user or script reload
    TestNavigationObserver nav_observer(web_contents());
    if (is_user_initiated_reload) {
      shell()->Reload();
    } else {
      ExecuteScriptAsync(
          web_contents()->GetPrimaryMainFrame(),
          content::JsReplace("window.location.reload($1);", true));
    }
    nav_observer.Wait();

    // Return the cookies rendered on frame
    return ExtractFrameContent(web_contents()->GetPrimaryMainFrame());
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SendSameSiteCookies) {
  SetSameSiteCookies(kHostA);
  SetSameSiteCookies(kHostB);

  // Main frame browser-initiated navigation sends all SameSite cookies.
  ASSERT_TRUE(
      NavigateToURL(web_contents(), EchoCookiesUrl(https_server(), kHostA)));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Main frame same-site (A => A) navigation sends all SameSite cookies.
  ASSERT_TRUE(
      NavigateToURLFromRenderer(web_contents()->GetPrimaryMainFrame(),
                                EchoCookiesUrl(https_server(), kHostA)));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Main frame cross-site (A => B) navigation sends all but Strict cookies.
  ASSERT_TRUE(
      NavigateToURLFromRenderer(web_contents()->GetPrimaryMainFrame(),
                                EchoCookiesUrl(https_server(), kHostB)));
  EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSameSiteLaxCookieName), Key(kSameSiteNoneCookieName),
                  Key(kSameSiteUnspecifiedCookieName))));

  // Same-site iframe (A embedded in A) sends all SameSite cookies.
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(),
          FrameTreeForUrl(EchoCookiesUrl(https_server(), kHostA)), {0}),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Cross-site iframe (B embedded in A) sends only None cookies.
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(),
          FrameTreeForUrl(EchoCookiesUrl(https_server(), kHostB)), {0}),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SendSameSiteCookies_Redirect) {
  SetSameSiteCookies(kHostA);

  // Main frame same-site redirect (A->A) sends all SameSite cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      RedirectUrl(https_server(), kHostA,
                  EchoCookiesUrl(https_server(), kHostA)),
      /*expected_commit_url=*/EchoCookiesUrl(https_server(), kHostA)));
  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  if (DoesSameSiteConsiderRedirectChain()) {
    // Main frame cross-site redirect (B->A) sends Lax but not Strict SameSite
    // cookies...
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        RedirectUrl(https_server(), kHostB,
                    EchoCookiesUrl(https_server(), kHostA)),
        /*expected_commit_url=*/EchoCookiesUrl(https_server(), kHostA)));
    EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteLaxCookieName), Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));

    // ... even if the first URL is same-site. (A->B->A)
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        RedirectUrl(https_server(), kHostA,
                    RedirectUrl(https_server(), kHostB,
                                EchoCookiesUrl(https_server(), kHostA))),
        /*expected_commit_url=*/EchoCookiesUrl(https_server(), kHostA)));
    EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteLaxCookieName), Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));
  } else {
    // If redirect chains are not considered, then cross-site redirects do not
    // make the request cross-site.
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        RedirectUrl(https_server(), kHostB,
                    EchoCookiesUrl(https_server(), kHostA)),
        /*expected_commit_url=*/EchoCookiesUrl(https_server(), kHostA)));
    EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
                    Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));

    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        RedirectUrl(https_server(), kHostA,
                    RedirectUrl(https_server(), kHostB,
                                EchoCookiesUrl(https_server(), kHostA))),
        /*expected_commit_url=*/EchoCookiesUrl(https_server(), kHostA)));
    EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
                    Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));
  }

  // A same-site redirected iframe (A->A embedded in A) sends all SameSite
  // cookies.
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(),
          FrameTreeForUrl(RedirectUrl(https_server(), kHostA,
                                      EchoCookiesUrl(https_server(), kHostA))),
          {0}),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  if (DoesSameSiteConsiderRedirectChain()) {
    // A cross-site redirected iframe in a same-site context (B->A embedded in
    // A) does not send SameSite cookies...
    EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                    web_contents(), https_server(),
                    FrameTreeForUrl(
                        RedirectUrl(https_server(), kHostB,
                                    EchoCookiesUrl(https_server(), kHostA))),
                    {0}),
                net::CookieStringIs(
                    UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

    // ... even if the first URL is same-site. (A->B->A embedded in A)
    EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                    web_contents(), https_server(),
                    FrameTreeForUrl(RedirectUrl(
                        https_server(), kHostA,
                        RedirectUrl(https_server(), kHostB,
                                    EchoCookiesUrl(https_server(), kHostA)))),
                    {0}),
                net::CookieStringIs(
                    UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
  } else {
    // If redirect chains are not considered, then cross-site redirects do not
    // make the request cross-site.
    EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                    web_contents(), https_server(),
                    FrameTreeForUrl(
                        RedirectUrl(https_server(), kHostB,
                                    EchoCookiesUrl(https_server(), kHostA))),
                    {0}),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
                    Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));

    EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                    web_contents(), https_server(),
                    FrameTreeForUrl(
                        RedirectUrl(https_server(), kHostB,
                                    EchoCookiesUrl(https_server(), kHostA))),
                    {0}),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
                    Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));
  }
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SetSameSiteCookies) {
  // Main frame can set all SameSite cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, kSetSameSiteCookiesPath)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteStrictCookieName),
                  net::MatchesCookieWithName(kSameSiteLaxCookieName),
                  net::MatchesCookieWithName(kSameSiteNoneCookieName),
                  net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // Same-site iframe (A embedded in A) sets all SameSite cookies.
  const GURL url_a = SetSameSiteCookiesUrl(https_server(), kHostA);
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), FrameTreeForUrl(url_a),
                  url::Origin::Create(url_a).GetURL()),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteStrictCookieName),
                  net::MatchesCookieWithName(kSameSiteLaxCookieName),
                  net::MatchesCookieWithName(kSameSiteNoneCookieName),
                  net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // Cross-site iframe (B embedded in A) sets only None cookies.
  const GURL url_b = SetSameSiteCookiesUrl(https_server(), kHostB);
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), FrameTreeForUrl(url_b),
                  url::Origin::Create(url_b).GetURL()),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteNoneCookieName)));
  ASSERT_EQ(1U, ClearCookies());
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SetSameSiteCookies_Redirect) {
  // Same-site redirected main frame navigation can set all SameSite cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      RedirectUrl(https_server(), kHostA,
                  SetSameSiteCookiesUrl(https_server(), kHostA)),
      /*expected_commit_url=*/SetSameSiteCookiesUrl(https_server(), kHostA)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteStrictCookieName),
                  net::MatchesCookieWithName(kSameSiteLaxCookieName),
                  net::MatchesCookieWithName(kSameSiteNoneCookieName),
                  net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // Cross-site redirected main frame navigation can set all SameSite cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      RedirectUrl(https_server(), kHostB,
                  SetSameSiteCookiesUrl(https_server(), kHostA)),
      /*expected_commit_url=*/SetSameSiteCookiesUrl(https_server(), kHostA)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteStrictCookieName),
                  net::MatchesCookieWithName(kSameSiteLaxCookieName),
                  net::MatchesCookieWithName(kSameSiteNoneCookieName),
                  net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // A same-site redirected iframe sets all SameSite cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(),
                  FrameTreeForUrl(RedirectUrl(
                      https_server(), kHostA,
                      SetSameSiteCookiesUrl(https_server(), kHostA))),
                  https_server()->GetURL(kHostA, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteStrictCookieName),
                  net::MatchesCookieWithName(kSameSiteLaxCookieName),
                  net::MatchesCookieWithName(kSameSiteNoneCookieName),
                  net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  if (DoesSameSiteConsiderRedirectChain()) {
    // A cross-site redirected iframe only sets SameSite=None cookies.
    EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                    web_contents(), https_server(),
                    FrameTreeForUrl(RedirectUrl(
                        https_server(), kHostB,
                        SetSameSiteCookiesUrl(https_server(), kHostA))),
                    https_server()->GetURL(kHostA, "/")),
                UnorderedElementsAre(
                    net::MatchesCookieWithName(kSameSiteNoneCookieName)));
    ASSERT_EQ(1U, ClearCookies());
  } else {
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(),
            FrameTreeForUrl(
                RedirectUrl(https_server(), kHostB,
                            SetSameSiteCookiesUrl(https_server(), kHostA))),
            https_server()->GetURL(kHostA, "/")),
        UnorderedElementsAre(
            net::MatchesCookieWithName(kSameSiteStrictCookieName),
            net::MatchesCookieWithName(kSameSiteLaxCookieName),
            net::MatchesCookieWithName(kSameSiteNoneCookieName),
            net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
    ASSERT_EQ(4U, ClearCookies());
  }
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SendPrefixedCookies) {
  SetPrefixedCookies(kHostA);

  // Main frame browser-initiated navigation sends all prefixed cookies.
  ASSERT_TRUE(
      NavigateToURL(web_contents(), EchoCookiesUrl(https_server(), kHostA)));
  EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSecurePrefixCookieName), Key(kHostPrefixCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       SendPrefixedCookies_OmitsIfInsecure) {
  SetPrefixedCookies(kHostA);

  // Main frame browser-initiated navigation omits all prefixed cookies on
  // insecure connections.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            EchoCookiesUrl(embedded_test_server(), kHostA)));
  EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              net::CookieStringIs(IsEmpty()));
}

// embedded_test_server() uses http, which is insecure, but localhost is
// allowed to set prefixed cookies anyway.
IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SendPrefixedCookiesLocalhost) {
  SetPrefixedCookies("localhost", embedded_test_server());

  // Main frame browser-initiated navigation sends all prefixed cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), EchoCookiesUrl(embedded_test_server(), "localhost")));
  EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSecurePrefixCookieName), Key(kHostPrefixCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SetPrefixedCookies) {
  // Main frame can set cookies with all prefixes.
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(kHostA, GetSetPrefixedCookiesPath(kHostA))));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kHostPrefixCookieName),
                  net::MatchesCookieWithName(kSecurePrefixCookieName)));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       SetPrefixedCookies_DisallowedIfInsecure) {
  // Main frame cannot set cookies with any prefix over an insecure connection.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(
                                kHostA, GetSetPrefixedCookiesPath(kHostA))));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  embedded_test_server()->GetURL(kHostA, "/")),
              IsEmpty());
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       ScriptNavigationSameSite404ScriptReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ false,
                          /* is_cross_site_navigation= */ false,
                          /* is_user_initiated_reload= */ false),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       ScriptNavigationSameSite404UserReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ false,
                          /* is_cross_site_navigation= */ false,
                          /* is_user_initiated_reload= */ true),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       ScriptNavigationCrossSite404ScriptReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ false,
                          /* is_cross_site_navigation= */ true,
                          /* is_user_initiated_reload= */ false),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       ScriptNavigationCrossSite404UserReload) {
  EXPECT_THAT(Test404ReloadCookie(/* is_user_initiated_navigation= */ false,
                                  /* is_cross_site_navigation= */ true,
                                  /* is_user_initiated_reload= */ true),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSameSiteLaxCookieName), Key(kSameSiteNoneCookieName),
                  Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       UserNavigationSameSite404ScriptReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ true,
                          /* is_cross_site_navigation= */ false,
                          /* is_user_initiated_reload= */ false),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       UserNavigationSameSite404UserReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ true,
                          /* is_cross_site_navigation= */ false,
                          /* is_user_initiated_reload= */ true),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       UserNavigationCrossSite404ScriptReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ true,
                          /* is_cross_site_navigation= */ true,
                          /* is_user_initiated_reload= */ false),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest,
                       UserNavigationCrossSite404UserReload) {
  EXPECT_THAT(
      Test404ReloadCookie(/* is_user_initiated_navigation= */ true,
                          /* is_cross_site_navigation= */ true,
                          /* is_user_initiated_reload= */ true),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));
}

// Responds to a request to /echocookieswithcors with the cookies that were sent
// with the request. We can't use the default handler /echoheader?Cookie here,
// because it doesn't send the appropriate Access-Control-Allow-Origin and
// Access-Control-Allow-Credentials headers (which are required for this to
// work for cross-origin requests in the tests).
std::unique_ptr<net::test_server::HttpResponse>
HandleEchoCookiesWithCorsRequest(const net::test_server::HttpRequest& request) {
  if (request.relative_url != kEchoCookiesWithCorsPath) {
    return nullptr;
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  std::string content;

  // Get the 'Cookie' header that was sent in the request.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kCookie);
      it != request.headers.end()) {
    content = it->second;
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  // Set the cors enabled headers.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kOrigin);
      it != request.headers.end()) {
    http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                   "credentials");
    http_response->AddCustomHeader("Access-Control-Allow-Origin", it->second);
    http_response->AddCustomHeader("Origin", it->second);
    http_response->AddCustomHeader("Vary", "Origin");
    http_response->AddCustomHeader("Access-Control-Allow-Methods", "POST");
    http_response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
  }
  http_response->set_content(content);

  return http_response;
}

class ThirdPartyCookiesHttpCookieBrowserTest : public ContentBrowserTest {
 public:
  ThirdPartyCookiesHttpCookieBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~ThirdPartyCookiesHttpCookieBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleEchoCookiesWithCorsRequest));
    ASSERT_TRUE(https_server()->Start());
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  GURL EchoCookiesUrl(std::string_view host) {
    return https_server()->GetURL(host, "/echoheader?Cookie");
  }

  std::string ExtractFrameContent(RenderFrameHost* frame) const {
    return EvalJs(frame, "document.body.textContent").ExtractString();
  }

  std::string ExtractCookieFromDocument(RenderFrameHost* frame) const {
    return EvalJs(frame, "document.cookie").ExtractString();
  }

  std::string PostWithCredentials(RenderFrameHost* frame, const GURL& url) {
    constexpr char script[] = R"JS(
      fetch($1, {method: 'POST', 'credentials' : 'include'}
      ).then((result) => result.text());
      )JS";
    return EvalJs(frame, JsReplace(script, url)).ExtractString();
  }

  EvalJsResult Fetch(RenderFrameHost* frame,
                     const GURL& url,
                     std::string_view mode,
                     std::string_view credentials) {
    constexpr char script[] = R"JS(
      fetch($1, {mode: $2, credentials: $3}).then(result => result.text());
    )JS";
    return EvalJs(frame, JsReplace(script, url, mode, credentials));
  }

  bool CookieStoreEmpty(RenderFrameHost* frame) {
    constexpr char script[] = R"JS(
          (async () => {
            let cookies = await cookieStore.getAll();
            return cookies.length == 0;
          })();
      )JS";
    return EvalJs(frame, script).ExtractBool();
  }

  EvalJsResult NavigateToURLWithPOST(RenderFrameHost* frame,
                                     std::string_view host) {
    TestNavigationObserver observer(web_contents());

    constexpr char script[] = R"JS(
        let form = document.createElement('form');
        form.setAttribute('method', 'POST');
        form.setAttribute('action', $1);
        document.body.appendChild(form);
        form.submit();
     )JS";

    EvalJsResult result =
        EvalJs(frame, JsReplace(script, EchoCookiesUrl(host)));
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    return result;
  }

  EvalJsResult ReadCookiesViaFetchWithRedirect(
      RenderFrameHost* frame,
      std::string_view intermediate_host,
      std::string_view destination_host) {
    constexpr char script[] = "fetch($1).then((result) => result.text());";

    GURL redirect_url = RedirectUrl(https_server(), intermediate_host,
                                    EchoCookiesUrl(destination_host));

    return EvalJs(frame, JsReplace(script, redirect_url));
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
};

class ThirdPartyCookiesBlockedHttpCookieBrowserTest
    : public ThirdPartyCookiesHttpCookieBrowserTest {
 public:
  ThirdPartyCookiesBlockedHttpCookieBrowserTest() {
    feature_list_.InitWithFeatures(
        {
            net::features::kForceThirdPartyCookieBlocking,
        },
        {});
  }

  ~ThirdPartyCookiesBlockedHttpCookieBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       SameSiteNoneCookieNavigateCrossSiteEmbedToSameSiteUrl) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      net::features::kForceThirdPartyCookieBlocking));

  // Set SameSite=None cookie on kHostA.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));

  // Confirm cross-site iframe (kHostB embedded in kHostA) does not
  // send SameSite=None cookie to iframe.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(),
                  FrameTreeForUrl(EchoCookiesUrl(kHostB)), {0}),
              net::CookieStringIs(UnorderedElementsAre()));

  // Navigate embedded iframe from kHostB to kHostA and confirm that
  // SameSite=None cookie is sent.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostA)));

  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       SameSiteNoneCookieCrossSitePostRequest) {
  // Set and confirm SameSite=None cookie on top-level-site kHostB.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostB, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostB)));

  ASSERT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));

  // Perform 'Post' to cross-site (kHostB) and confirm no cookie present in
  // method response. Since there is no redirect action at the same time as
  // the post, the cookie will be blocked as the request is being made
  // cross-site.
  EXPECT_THAT(PostWithCredentials(
                  web_contents()->GetPrimaryMainFrame(),
                  https_server()->GetURL(kHostB, kEchoCookiesWithCorsPath)),
              "");
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       SameSiteNoneCookieCrossSiteSubresourceNavigationPost) {
  // Set and confirm SameSite=None cookie on top-level-site kHostB.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostB, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostB)));

  ASSERT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  // Starting at kHostA create a form that has an action value that causes a
  // navigation to a new top-level-site (kHostB). Submit the form to trigger the
  // navigation and confirm that no error has occurred in the response.
  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));

  ASSERT_TRUE(
      NavigateToURLWithPOST(web_contents()->GetPrimaryMainFrame(), kHostB)
          .is_ok());

  // Confirm that navigation from subresource occurred and cookies are still
  // available.
  EXPECT_THAT(web_contents()->GetLastCommittedURL().GetHost(), kHostB);

  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       RedirectCrossSiteSubresourceToSameSiteUrl) {
  // Set and confirm SameSite=None cookie on top-level-site kHostA.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));

  ASSERT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  // Perform redirect from cross-site subresource and ensure that no cookie was
  // sent even though it was redirected to the top-level-site.
  EXPECT_EQ(ReadCookiesViaFetchWithRedirect(
                web_contents()->GetPrimaryMainFrame(), kHostB, kHostA),
            "None");
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       SameSiteNoneCookieBlockedOnABEmbeddedIframe) {
  // Set and confirm SameSite=None cookie on top-level-site kHostA is
  // present in the cookie header, document.cookie and cookie store.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));
  // Confirm in cookie header.
  ASSERT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
  // Confirm in document.cookie.
  ASSERT_THAT(
      ExtractCookieFromDocument(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
  // Confirm in cookie store.
  ASSERT_TRUE(
      GetCookies(web_contents()->GetBrowserContext(), EchoCookiesUrl(kHostA))
          .starts_with(kSameSiteNoneCookieName));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostB)));

  // Embed an iframe containing A in B and check cookie header.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(),
                  FrameTreeForHostAndUrl(kHostB, EchoCookiesUrl(kHostA)), {0}),
              "None");

  // Check document.cookie.
  EXPECT_TRUE(ExtractCookieFromDocument(
                  ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0))
                  .empty());

  // Check cookie store.
  EXPECT_TRUE(
      CookieStoreEmpty(ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(
    ThirdPartyCookiesBlockedHttpCookieBrowserTest,
    SameSiteNoneCookieBlockedInCrossSiteFetchRequestFromTopLevelFrame) {
  // Set and confirm SameSite=None cookie on Site A.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));

  ASSERT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  // From site B make a fetch call (with credentials) from site B to site A; and
  // check if cookies are present on the request.
  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostB)));

  EXPECT_THAT(Fetch(web_contents()->GetPrimaryMainFrame(),
                    https_server()->GetURL(kHostA, kEchoCookiesWithCorsPath),
                    "cors", "include")
                  .ExtractString(),
              net::CookieStringIs(IsEmpty()));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       TestCrossSitePartitionKeyNotAvailable) {
  // Set cookie for site A kSameSite ancestor.
  // Create initial frame tree A->B (B is an iframe) and check cookie.
  // Navigate iframe with site B to A and confirm that cookie is present.

  // Set kSameSite cookie
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Embed an iframe containing B in A to create initial frame tree A->B.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostB)), {0}),
            "None");

  // Navigate embedded iframe B to A
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostA)));

  // Extract cookie from A
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       TestSubresourceRedirects) {
  // Initial frame tree A->B (B is an iframe).
  // A cookie is set for site C.
  // iframe B is navigated to site C.
  // Frame tree becomes A->C (C is an iframe).
  // Check if cookie set for C is present in C.

  // Embed an iframe containing B in A to create initial frame tree A->B.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostB)), {0}),
            "None");

  // Set SameSite=None partitioned cookie for kHostC from embedded iframe B.

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostC, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;partitioned"}),
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      partition_key));
  // confirm that there is a cookie with kHostC url in the mojom cookie manager
  // and that the cookie is partitioned and third party.
  std::vector<net::CanonicalCookie> cookies = GetCanonicalCookies(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostC, "/"),
      net::CookiePartitionKeyCollection(partition_key));
  ASSERT_EQ(cookies.size(), 1u);
  ASSERT_TRUE(cookies[0].IsPartitioned());
  ASSERT_TRUE(cookies[0].PartitionKey()->IsThirdParty());

  // Navigate embedded iframe B to C
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostC)));

  // Extract cookie from C
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       TestTopLevelRedirects) {
  // Navigate to Site A and set cookie on site A.
  // Redirect from site A to site B and back to site A.
  // Confirm cookie is present on site A after redirection.

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));
  // Check to make sure that there are no cookies set on kHostA.
  ASSERT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              "None");

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Perform redirect from site A to site B and back to site A.
  ASSERT_TRUE(
      NavigateToURL(web_contents(),
                    RedirectUrl(https_server(), kHostB, EchoCookiesUrl(kHostA)),
                    EchoCookiesUrl(kHostA)));

  EXPECT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(
    ThirdPartyCookiesBlockedHttpCookieBrowserTest,
    TestSameSiteEmbeddedResourceToCrossSiteEmbeddedResource) {
  // Initial frame tree A1->A2 (A2 is an iframe)
  // A cookie is set from top-level A1 for site B with kCrossSite ancestor chain
  // bit. iframe A2 is navigated to site B. Frame tree becomes A1->B (B is an
  // iframe). Check if cookie set from A1 is present in B.

  // Embed an iframe containing A in A to create initial frame tree A->A.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostA)), {0}),
            "None");

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostB, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite)));

  // Navigate embedded iframe A2 to B.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostB)));

  // Confirm that the cookie is in the header sent to the iframe.
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       CrossSiteToSameSiteIframeRedirects) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect back to A.
  // Confirm that cookie is accessible from the iframe.

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Embed an iframe containing A in A to create initial frame tree A->A.
  // Confirm that partitioned cookie is accessible from the iframe.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostA)), {0}),
            "samesite_none_cookie=1");

  // Navigate the iframe from A to B to A.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      RedirectUrl(https_server(), kHostB, EchoCookiesUrl(kHostA)),
      /*expected_commit_url=*/EchoCookiesUrl(kHostA)));

  // Confirm that the cookie is in the header sent to the iframe.
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       RedirectCrossSiteThroughSameSiteIframe) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect back to a
  // different page with domain of A and then redirect back to the original
  // site. Confirm that cookie is accessible from the header in the final
  // redirect.

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Embed an iframe containing A in A to create initial frame tree A->A.
  // Confirm that partitioned cookie is accessible from the iframe.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostA)), {0}),
            "samesite_none_cookie=1");

  // Navigate the iframe from A to B redirecting to A redirecting to A.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      RedirectUrl(https_server(), kHostB,
                  RedirectUrl(https_server(), kHostA, EchoCookiesUrl(kHostA))),
      /*expected_commit_url=*/EchoCookiesUrl(kHostA)));

  // Confirm that the cookie is in the header sent to the iframe.
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       RedirectTwoCrossSitesThroughSameSiteIframe) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect to a second
  // cross-domain (site C) and then redirect back to A.
  // Confirm that cookie is accessible from the header in the final
  // redirect.

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Embed an iframe containing A in A to create initial frame tree A->A.
  // Confirm that partitioned cookie is accessible from the iframe.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostA)), {0}),
            "samesite_none_cookie=1");

  // Navigate the iframe from A to B redirecting to C and then back to A.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      RedirectUrl(https_server(), kHostB,
                  RedirectUrl(https_server(), kHostC,
                              RedirectUrl(https_server(), kHostA,
                                          EchoCookiesUrl(kHostA)))),
      EchoCookiesUrl(kHostA)));

  // Confirm that the cookie is in the header sent to the iframe.
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(
    ThirdPartyCookiesBlockedHttpCookieBrowserTest,
    RedirectCrossSiteIframeToSameSiteThenNavigateToSameSite) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect back to site A.
  // Then navigate to site A again.
  // Confirm that cookie is accessible from the header in the final navigation.

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Embed an iframe containing A in A to create initial frame tree A->A.
  // Confirm that partitioned cookie is accessible from the iframe.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostA)), {0}),
            "samesite_none_cookie=1");
  // Navigate the iframe from A to B redirecting to A.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      RedirectUrl(https_server(), kHostB, EchoCookiesUrl(kHostA)),
      /*expected_commit_url=*/EchoCookiesUrl(kHostA)));

  // Navigate to A from the iframe A that was redirected to site A from B.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostA)));

  // Confirm that the cookie is in the header sent to the iframe.
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesBlockedHttpCookieBrowserTest,
                       CrossSiteToSameSiteIframeNavigation) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B).
  // Then navigate the iframe back to site A.
  // Confirm that cookie is accessible from the iframe.

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite)));

  // Embed an iframe containing A in A to create initial frame tree A->A.
  // Confirm that partitioned cookie is accessible from the iframe.
  ASSERT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(),
                FrameTreeForUrl(EchoCookiesUrl(kHostA)), {0}),
            "samesite_none_cookie=1");

  // Navigate the iframe from A to B. Then B to A.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostB)));

  ASSERT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
      EchoCookiesUrl(kHostA)));

  // Confirm that the cookie is in the header sent to the iframe.
  EXPECT_THAT(
      ExtractFrameContent(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  EXPECT_THAT(
      ExtractCookieFromDocument(
          ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

class DevToolsOverridesThirdPartyCookiesBrowserTest
    : public ThirdPartyCookiesHttpCookieBrowserTest {
 public:
  DevToolsOverridesThirdPartyCookiesBrowserTest() = default;

  ~DevToolsOverridesThirdPartyCookiesBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ThirdPartyCookiesHttpCookieBrowserTest::SetUpOnMainThread();
    web_contents_devtools_client.AttachToWebContents(shell()->web_contents());
    web_contents_devtools_client.SendCommandAsync("Network.enable");
  }

  void TearDownOnMainThread() override {
    web_contents_devtools_client.DetachProtocolClient();
    frame_devtools_client.DetachProtocolClient();
  }

 protected:
  void NavigateToPageWith3pIFrame(std::string_view host) {
    frame_devtools_client.DetachProtocolClient();
    GURL main_url(https_server()->GetURL(host, "/page_with_blank_iframe.html"));

    ASSERT_TRUE(content::NavigateToURL(web_contents(), main_url));
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents(), "test_iframe",
                            https_server()->GetURL(kHostB, "/empty.html")));

    frame_devtools_client.AttachToFrameTreeHost(GetFrame());
    frame_devtools_client.SendCommandSync("Network.enable");
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  }

  std::string SetCookieFromJS(content::RenderFrameHost* render_frame_host,
                              std::string cookie) {
    content::EvalJsResult result = content::EvalJs(
        render_frame_host,
        "document.cookie = '" + cookie + "; SameSite=None; Secure'",
        content::EXECUTE_SCRIPT_NO_USER_GESTURE);

    return result.ExtractString();
  }

  std::string ReadCookiesFromJS(content::RenderFrameHost* render_frame_host) {
    std::string res = content::EvalJs(render_frame_host, "document.cookie",
                                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                          .ExtractString();

    return res;
  }

  void SendSetCookieControls(bool enable_third_party_cookie_restriction,
                             bool disable_third_party_cookie_metadata,
                             bool disable_third_party_cookie_heuristics) {
    base::Value::Dict command_params;
    web_contents_devtools_client.SendCommandSync("Network.enable");
    command_params.Set("enableThirdPartyCookieRestriction",
                       enable_third_party_cookie_restriction);
    command_params.Set("disableThirdPartyCookieMetadata",
                       disable_third_party_cookie_metadata);
    command_params.Set("disableThirdPartyCookieHeuristics",
                       disable_third_party_cookie_heuristics);
    web_contents_devtools_client.SendCommandSync("Network.setCookieControls",
                                                 std::move(command_params));
  }

  content::TestDevToolsProtocolClient web_contents_devtools_client;
  content::TestDevToolsProtocolClient frame_devtools_client;
};

IN_PROC_BROWSER_TEST_F(DevToolsOverridesThirdPartyCookiesBrowserTest,
                       DevToolsForceDisableTPCFromJS) {
  // Third-party access should work initially
  NavigateToPageWith3pIFrame(kHostA);
  SetCookieFromJS(GetFrame(), "cookieAllowed=true");
  EXPECT_EQ(ReadCookiesFromJS(GetFrame()), "cookieAllowed=true");

  // Turning on third-party cookie restriction
  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/false,
                        /*disable_third_party_cookie_heuristics=*/false);

  // Refreshing so that RCM is re-created with new controls
  NavigateToPageWith3pIFrame("a.test");

  // Both of these should get blocked now
  SetCookieFromJS(GetFrame(), "cookieAllowed=false");
  EXPECT_EQ(ReadCookiesFromJS(GetFrame()), "");

  // Disabling the network domain should return to normal (unblocked) cookie
  // state
  frame_devtools_client.SendCommandSync("Network.disable");
  web_contents_devtools_client.SendCommandSync("Network.disable");

  // The cookie should be the same as before proving that the last
  // SetCookieFromJS didn't update the cookie
  EXPECT_EQ(ReadCookiesFromJS(GetFrame()), "cookieAllowed=true");
}

IN_PROC_BROWSER_TEST_F(DevToolsOverridesThirdPartyCookiesBrowserTest,
                       DevToolsForceDisableTPC) {
  // Set SameSite=None cookie on top-level-site kHostA.
  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat({kSameSiteNoneCookieName, "=1;Secure;SameSite=None;"})));
  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));
  ASSERT_THAT(
      ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
  // Embed an iframe containing A in B and check 3pc is allowed.
  ASSERT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(),
          FrameTreeForHostAndUrl(kHostB, EchoCookiesUrl(kHostA)), {0}),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

  // Apply devtools overrides to enable 3pc restriction.
  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/false,
                        /*disable_third_party_cookie_heuristics=*/false);

  // 3pc should be blocked due to devtools overrides.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(),
                  FrameTreeForHostAndUrl(kHostB, EchoCookiesUrl(kHostA)), {0}),
              "None");
  EXPECT_THAT(Fetch(web_contents()->GetPrimaryMainFrame(),
                    https_server()->GetURL(kHostA, kEchoCookiesWithCorsPath),
                    "cors", "include")
                  .ExtractString(),
              net::CookieStringIs(IsEmpty()));

  web_contents_devtools_client.SendCommandAsync("Network.disable");
  // The override should stop working and 3pc is re-allowed after devtools is
  // disabled.
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(),
          FrameTreeForHostAndUrl(kHostB, EchoCookiesUrl(kHostA)), {0}),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
  EXPECT_THAT(
      Fetch(web_contents()->GetPrimaryMainFrame(),
            https_server()->GetURL(kHostA, kEchoCookiesWithCorsPath), "cors",
            "include")
          .ExtractString(),
      net::CookieStringIs(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         HttpCookieBrowserTest,
                         ::testing::Bool());

}  // namespace
}  // namespace content
