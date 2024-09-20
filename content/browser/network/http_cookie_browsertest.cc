// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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

constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";
constexpr char kSameSiteNoneCookieName[] = "samesite_none_cookie";
constexpr char kSameSiteStrictCookieName[] = "samesite_strict_cookie";
constexpr char kSameSiteLaxCookieName[] = "samesite_lax_cookie";
constexpr char kSameSiteUnspecifiedCookieName[] = "samesite_unspecified_cookie";
constexpr char kEchoCookiesWithCorsPath[] = "/echocookieswithcors";

std::string FrameTreeForHostAndUrl(std::string_view host, const GURL& url) {
  return base::StrCat({host, "(", url.spec(), ")"});
}

std::string FrameTreeForUrl(const GURL& url) {
  return FrameTreeForHostAndUrl(kHostA, url);
}

GURL RedirectUrl(net::EmbeddedTestServer* test_server,
                 const std::string& host,
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
    ASSERT_TRUE(https_server()->Start());
  }

  bool DoesSameSiteConsiderRedirectChain() { return GetParam(); }

  const std::string kSetSameSiteCookiesURL = base::StrCat({
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

  void SetSameSiteCookies(const std::string& host) {
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

  GURL EchoCookiesUrl(net::EmbeddedTestServer* test_server,
                      const std::string& host) {
    return test_server->GetURL(host, "/echoheader?Cookie");
  }

  GURL SetSameSiteCookiesUrl(net::EmbeddedTestServer* test_server,
                             const std::string& host) {
    return test_server->GetURL(host, kSetSameSiteCookiesURL);
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
    const char* target = is_cross_site_navigation ? kHostB : kHostA;
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
      web_contents(), https_server()->GetURL(kHostA, kSetSameSiteCookiesURL)));
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

class ThirdPartyCookiesBlockedHttpCookieBrowserTest
    : public ContentBrowserTest {
 public:
  ThirdPartyCookiesBlockedHttpCookieBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        {
            net::features::kForceThirdPartyCookieBlocking,
        },
        {});
  }

  ~ThirdPartyCookiesBlockedHttpCookieBrowserTest() override = default;

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

  GURL EchoCookiesUrl(const std::string& host) {
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
                     const std::string& mode,
                     const std::string& credentials) {
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
                                     const std::string& host) {
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
      const std::string& intermediate_host,
      const std::string& destination_host) {
    constexpr char script[] = "fetch($1).then((result) => result.text());";

    GURL redirect_url = RedirectUrl(https_server(), intermediate_host,
                                    EchoCookiesUrl(destination_host));

    return EvalJs(frame, JsReplace(script, redirect_url));
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
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
          .error.empty());

  // Confirm that navigation from subresource occurred and cookies are still
  // available.
  EXPECT_THAT(web_contents()->GetLastCommittedURL().host(), kHostB);

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

class AncestorChainBitEnabledThirdPartyCookiesBlockedTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  AncestorChainBitEnabledThirdPartyCookiesBlockedTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatureStates(
        {{net::features::kForceThirdPartyCookieBlocking, true},
         {net::features::kAncestorChainBitEnabledInPartitionedCookies,
          AncestorChainBitEnabled()}});
  }

  bool AncestorChainBitEnabled() { return GetParam(); }

  ~AncestorChainBitEnabledThirdPartyCookiesBlockedTest() override = default;

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

  GURL EchoCookiesUrl(const std::string& host) const {
    return https_server_.GetURL(host, "/echoheader?Cookie");
  }

  std::string ExtractFrameContent(RenderFrameHost* frame) const {
    return EvalJs(frame, "document.body.textContent").ExtractString();
  }

  std::string ExtractCookieFromDocument(RenderFrameHost* frame) const {
    return EvalJs(frame, "document.cookie").ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
                       TestCrossSitePartitionKeyNotAvailable) {
  // Set cookie for site A kSameSite ancestor.
  // Create initial frame tree A->B (B is an iframe) and check cookie.
  // Navigate iframe with site B to A and confirm that cookie is present.

  // Set kSameSite cookie
  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
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
      &partition_key));
  // confirm that there is a cookie with kHostC url in the mojom cookie manager
  // and that the cookie is partitioned and third party.
  std::vector<net::CanonicalCookie> cookies = GetCanonicalCookies(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostC, "/"),
      net::CookiePartitionKeyCollection::FromOptional(
          std::make_optional(partition_key)));
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

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
                       TestTopLevelRedirects) {
  // Navigate to Site A and set cookie on site A.
  // Redirect from site A to site B and back to site A.
  // Confirm cookie is present on site A after redirection.

  ASSERT_TRUE(NavigateToURL(web_contents(), EchoCookiesUrl(kHostA)));
  // Check to make sure that there are no cookies set on kHostA.
  ASSERT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              "None");
  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(
    AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
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

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostB, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
                       CrossSiteToSameSiteIframeRedirects) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect back to A.
  // Confirm that cookie is accessible from the iframe.

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
                       RedirectCrossSiteThroughSameSiteIframe) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect back to a
  // different page with domain of A and then redirect back to the original
  // site. Confirm that cookie is accessible from the header in the final
  // redirect.

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
                       RedirectTwoCrossSitesThroughSameSiteIframe) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect to a second
  // cross-domain (site C) and then redirect back to A.
  // Confirm that cookie is accessible from the header in the final
  // redirect.

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(
    AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
    RedirectCrossSiteIframeToSameSiteThenNavigateToSameSite) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B) and redirect back to site A.
  // Then navigate to site A again.
  // Confirm that cookie is accessible from the header in the final navigation.

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

IN_PROC_BROWSER_TEST_P(AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
                       CrossSiteToSameSiteIframeNavigation) {
  // Set partitioned kSameSite ancestor cookie on top level site A.
  // Embed an iframe of site A and confirm cookie is accessible from iframe.
  // Navigate the iframe to a cross-domain (site B).
  // Then navigate the iframe back to site A.
  // Confirm that cookie is accessible from the iframe.

  net::CookiePartitionKey partition_key =
      net::CookiePartitionKey::FromURLForTesting(
          https_server()->GetURL(kHostA, "/"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite);

  ASSERT_TRUE(SetCookie(
      web_contents()->GetBrowserContext(), https_server()->GetURL(kHostA, "/"),
      base::StrCat(
          {kSameSiteNoneCookieName, "=1;Secure;SameSite=None;Partitioned"}),
      net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
      &partition_key));

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

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         HttpCookieBrowserTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    AncestorChainBitEnabledThirdPartyCookiesBlockedTest,
    ::testing::Bool());

}  // namespace
}  // namespace content
