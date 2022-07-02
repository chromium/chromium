// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
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

class HttpCookieBrowserTest : public ContentBrowserTest,
                              public ::testing::WithParamInterface<bool> {
 public:
  HttpCookieBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (DoesSameSiteConsiderRedirectChain()) {
      feature_list_.InitAndEnableFeature(
          net::features::kCookieSameSiteConsidersRedirectChain);
    }
  }

  ~HttpCookieBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StringPrintf("https://%s,https://%s,https://%s", kHostA, kHostB,
                           kHostC));
  }

  bool DoesSameSiteConsiderRedirectChain() { return GetParam(); }

  const char* kHostA = "a.test";
  const char* kHostB = "b.test";
  const char* kHostC = "c.test";
  const char* kHostD = "d.test";
  const char* kSameSiteStrictCookieName = "samesite_strict_cookie";
  const char* kSameSiteLaxCookieName = "samesite_lax_cookie";
  const char* kSameSiteNoneCookieName = "samesite_none_cookie";
  const char* kSameSiteUnspecifiedCookieName = "samesite_unspecified_cookie";
  const char* kSamePartyLaxCookieName = "sameparty_lax_cookie";
  const char* kSamePartyNoneCookieName = "sameparty_none_cookie";
  const char* kSamePartyUnspecifiedCookieName = "sameparty_unspecified_cookie";
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
  const std::string kSetSamePartyCookiesURL = base::StrCat({
      "/set-cookie?",
      kSamePartyLaxCookieName,
      "=1;SameParty;Secure;SameSite=Lax&",
      kSamePartyNoneCookieName,
      "=1;SameParty;Secure;SameSite=None&",
      kSamePartyUnspecifiedCookieName,
      "=1;SameParty;Secure",
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

  void SetSamePartyCookies(const std::string& host) {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    EXPECT_TRUE(
        SetCookie(context, https_server()->GetURL(host, "/"),
                  base::StrCat({kSamePartyLaxCookieName,
                                "=1; samesite=lax; secure; sameparty"})));
    EXPECT_TRUE(
        SetCookie(context, https_server()->GetURL(host, "/"),
                  base::StrCat({kSamePartyNoneCookieName,
                                "=1; samesite=none; secure; sameparty"})));
    EXPECT_TRUE(SetCookie(context, https_server()->GetURL(host, "/"),
                          base::StrCat({kSamePartyUnspecifiedCookieName,
                                        "=1; secure; sameparty"})));
  }

  GURL EchoCookiesUrl(net::EmbeddedTestServer* test_server,
                      const std::string& host) {
    return test_server->GetURL(host, "/echoheader?Cookie");
  }

  GURL SetSameSiteCookiesUrl(net::EmbeddedTestServer* test_server,
                             const std::string& host) {
    return test_server->GetURL(host, kSetSameSiteCookiesURL);
  }

  GURL SetSamePartyCookiesUrl(net::EmbeddedTestServer* test_server,
                              const std::string& host) {
    return test_server->GetURL(host, kSetSamePartyCookiesURL);
  }

  GURL RedirectUrl(net::EmbeddedTestServer* test_server,
                   const std::string& host,
                   const GURL& target_url) {
    return test_server->GetURL(host, "/server-redirect?" + target_url.spec());
  }

  std::string ExtractFrameContent(RenderFrameHost* frame) const {
    return EvalJs(frame, "document.body.textContent").ExtractString();
  }

  void NavigateFrameHostToURL(RenderFrameHost* iframe, const GURL& url) {
    TestNavigationObserver nav_observer(shell()->web_contents());
    ExecuteScriptAsync(iframe, JsReplace("location = $1", url));
    nav_observer.Wait();
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
          web_contents(), https_server(), "a.test(%s)", {0},
          EchoCookiesUrl(https_server(), kHostA)),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Cross-site iframe (B embedded in A) sends only None cookies.
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(%s)", {0},
          EchoCookiesUrl(https_server(), kHostB)),
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
          web_contents(), https_server(), "a.test(%s)", {0},
          RedirectUrl(https_server(), kHostA,
                      EchoCookiesUrl(https_server(), kHostA))),
      net::CookieStringIs(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  if (DoesSameSiteConsiderRedirectChain()) {
    // A cross-site redirected iframe in a same-site context (B->A embedded in
    // A) does not send SameSite cookies...
    EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                    web_contents(), https_server(), "a.test(%s)", {0},
                    RedirectUrl(https_server(), kHostB,
                                EchoCookiesUrl(https_server(), kHostA))),
                net::CookieStringIs(
                    UnorderedElementsAre(Key(kSameSiteNoneCookieName))));

    // ... even if the first URL is same-site. (A->B->A embedded in A)
    EXPECT_THAT(
        content::ArrangeFramesAndGetContentFromLeaf(
            web_contents(), https_server(), "a.test(%s)", {0},
            RedirectUrl(https_server(), kHostA,
                        RedirectUrl(https_server(), kHostB,
                                    EchoCookiesUrl(https_server(), kHostA)))),
        net::CookieStringIs(
            UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
  } else {
    // If redirect chains are not considered, then cross-site redirects do not
    // make the request cross-site.
    EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                    web_contents(), https_server(), "a.test(%s)", {0},
                    RedirectUrl(https_server(), kHostB,
                                EchoCookiesUrl(https_server(), kHostA))),
                net::CookieStringIs(UnorderedElementsAre(
                    Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
                    Key(kSameSiteNoneCookieName),
                    Key(kSameSiteUnspecifiedCookieName))));

    EXPECT_THAT(
        content::ArrangeFramesAndGetContentFromLeaf(
            web_contents(), https_server(), "a.test(%s)", {0},
            RedirectUrl(https_server(), kHostA,
                        RedirectUrl(https_server(), kHostB,
                                    EchoCookiesUrl(https_server(), kHostA)))),
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
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSameSiteCookiesUrl(https_server(), kHostA)),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSameSiteStrictCookieName),
                  net::MatchesCookieWithName(kSameSiteLaxCookieName),
                  net::MatchesCookieWithName(kSameSiteNoneCookieName),
                  net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // Cross-site iframe (B embedded in A) sets only None cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSameSiteCookiesUrl(https_server(), kHostB)),
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
                  web_contents(), https_server(), "a.test(%s)",
                  RedirectUrl(https_server(), kHostA,
                              SetSameSiteCookiesUrl(https_server(), kHostA)),
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
                    web_contents(), https_server(), "a.test(%s)",
                    RedirectUrl(https_server(), kHostB,
                                SetSameSiteCookiesUrl(https_server(), kHostA)),
                    https_server()->GetURL(kHostA, "/")),
                UnorderedElementsAre(
                    net::MatchesCookieWithName(kSameSiteNoneCookieName)));
    ASSERT_EQ(1U, ClearCookies());
  } else {
    EXPECT_THAT(
        content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(), "a.test(%s)",
            RedirectUrl(https_server(), kHostB,
                        SetSameSiteCookiesUrl(https_server(), kHostA)),
            https_server()->GetURL(kHostA, "/")),
        UnorderedElementsAre(
            net::MatchesCookieWithName(kSameSiteStrictCookieName),
            net::MatchesCookieWithName(kSameSiteLaxCookieName),
            net::MatchesCookieWithName(kSameSiteNoneCookieName),
            net::MatchesCookieWithName(kSameSiteUnspecifiedCookieName)));
    ASSERT_EQ(4U, ClearCookies());
  }
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SendSamePartyCookies) {
  SetSamePartyCookies(kHostA);
  SetSamePartyCookies(kHostD);

  // No embedded frame. The top-level site has access to its cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Same-site FPS-member iframe (A embedded in A) sends A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "a.test(%s)", {0},
                  EchoCookiesUrl(https_server(), kHostA)),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, same-party iframe (B embedded in A) does not send A's SameParty
  // cookies (since it's the wrong domain).
  EXPECT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(), "a.test(%s)", {0},
                EchoCookiesUrl(https_server(), kHostB)),
            "None");

  // Cross-site, same-party iframe (A embedded in B) sends A's SameParty
  // cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "b.test(%s)", {0},
                  EchoCookiesUrl(https_server(), kHostA)),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, same-party nested iframe (A embedded in B embedded in A) sends
  // A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "a.test(b.test(%s))", {0, 0},
                  EchoCookiesUrl(https_server(), kHostA)),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A) sends A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
                  {0, 0, 0}, EchoCookiesUrl(https_server(), kHostA)),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, cross-party iframe (D embedded in A) sends only D's
  // SameSite=None cookie, since D is not in A's First-Party Set.
  EXPECT_THAT(
      content::ArrangeFramesAndGetContentFromLeaf(
          web_contents(), https_server(), "a.test(%s)", {0},
          EchoCookiesUrl(https_server(), kHostD)),
      net::CookieStringIs(UnorderedElementsAre(Key(kSamePartyNoneCookieName))));

  // Cross-site, cross-party iframe (A embedded in D) doesn't send A's SameParty
  // cookies.
  EXPECT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(), "d.test(%s)", {0},
                EchoCookiesUrl(https_server(), kHostA)),
            "None");

  // Cross-site, cross-party nested iframe (A embedded in B embedded in D)
  // doesn't send A's SameParty cookies.
  EXPECT_EQ(content::ArrangeFramesAndGetContentFromLeaf(
                web_contents(), https_server(), "d.test(b.test(%s))", {0, 0},
                EchoCookiesUrl(https_server(), kHostA)),
            "None");

  // No embedded frame. The top-level site has access to its cookies, regardless
  // of whether the site is in an FPS, or whether the cookies are SameParty.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostD, "/echoheader?Cookie")));
  EXPECT_THAT(ExtractFrameContent(web_contents()->GetPrimaryMainFrame()),
              net::CookieStringIs(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_P(HttpCookieBrowserTest, SetSamePartyCookies) {
  // No embedded frame, FPS member. The top-level FPS site can set its cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostA, kSetSamePartyCookiesURL)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostA, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyLaxCookieName),
                  net::MatchesCookieWithName(kSamePartyNoneCookieName),
                  net::MatchesCookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Same-site FPS-member iframe (A embedded in A) sets A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSamePartyCookiesUrl(https_server(), kHostA)),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyLaxCookieName),
                  net::MatchesCookieWithName(kSamePartyNoneCookieName),
                  net::MatchesCookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, same-party iframe (A embedded in B) sets A's SameParty
  // cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "b.test(%s)",
                  SetSamePartyCookiesUrl(https_server(), kHostA)),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyLaxCookieName),
                  net::MatchesCookieWithName(kSamePartyNoneCookieName),
                  net::MatchesCookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, same-party nested iframe (A embedded in B embedded in A) sets
  // A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(b.test(%s))",
                  SetSamePartyCookiesUrl(https_server(), kHostA)),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyLaxCookieName),
                  net::MatchesCookieWithName(kSamePartyNoneCookieName),
                  net::MatchesCookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A) sets A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(c.test(b.test(%s)))",
                  SetSamePartyCookiesUrl(https_server(), kHostA)),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyLaxCookieName),
                  net::MatchesCookieWithName(kSamePartyNoneCookieName),
                  net::MatchesCookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, cross-party iframe (D embedded in A) sets D's SameSite=None
  // cookie, since it's not an FPS member (and SameParty is ignored).
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "a.test(%s)",
                  SetSamePartyCookiesUrl(https_server(), kHostD)),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyNoneCookieName)));
  ASSERT_EQ(1U, ClearCookies());

  // Cross-site, cross-party iframe (A embedded in D) doesn't set A's SameParty
  // cookies, since A is an FPS member and SameParty is not ignored..
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "d.test(%s)",
                  SetSamePartyCookiesUrl(https_server(), kHostA)),
              IsEmpty());
  ASSERT_EQ(0U, ClearCookies());

  // Cross-site, cross-party nested iframe (A embedded in B embedded in D)
  // doesn't set A's SameParty cookies.
  EXPECT_THAT(content::ArrangeFramesAndGetCanonicalCookiesForLeaf(
                  web_contents(), https_server(), "d.test(b.test(%s))",
                  SetSamePartyCookiesUrl(https_server(), kHostA)),
              IsEmpty());
  ASSERT_EQ(0U, ClearCookies());

  // No embedded frame, non-FPS member. The top-level site can set its cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents(), https_server()->GetURL(kHostD, kSetSamePartyCookiesURL)));
  EXPECT_THAT(GetCanonicalCookies(web_contents()->GetBrowserContext(),
                                  https_server()->GetURL(kHostD, "/")),
              UnorderedElementsAre(
                  net::MatchesCookieWithName(kSamePartyLaxCookieName),
                  net::MatchesCookieWithName(kSamePartyNoneCookieName),
                  net::MatchesCookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());
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

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         HttpCookieBrowserTest,
                         ::testing::Bool());

struct OriginTrialTestOptions {
  bool has_ot_token = true;
  bool valid_ot_token = true;
  bool has_set_cookie = true;
  bool has_partitioned = true;
};

// This class tests the origin trial mechanism for partitioned cookies.
// Partitioned cookies should be reverted to unpartitioned if the navigation
// has a Set-Cookie header with the Partitioned attribute and the site does
// not send a valid Origin-Trial header.
// This test exercises the origin trial for top-level navigation requests.
class PartitionedCookiesOriginTrialBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({net::features::kPartitionedCookies},
                                          {});
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &PartitionedCookiesOriginTrialBrowserTest::InterceptRequest,
            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetTestOptions(const OriginTrialTestOptions& test_setting,
                      const std::set<GURL>& expected_request_urls) {
    test_options_ = test_setting;
    expected_request_urls_ = expected_request_urls;
  }

  virtual const char* OriginTrialToken() const {
    // The test Origin Trial token was generated by running:
    // python tools/origin_trials/generate_token.py https://127.0.0.1:44444 \
    //     PartitionedCookies \
    //     --expire-timestamp=2000000000
    return "A4s/"
           "iPKfhEfgqQIIuz4zLuCpONpXOuYyJFBhBx1MfgS1aNhFujyhsg4lkfTRfjzQCI3aUbM"
           "wtNm25elLTR4UIgAAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0ND"
           "QiLCAiZmVhdHVyZSI6ICJQYXJ0aXRpb25lZENvb2tpZXMiLCAiZXhwaXJ5IjogMjAwM"
           "DAwMDAwMH0=";
  }

  // We use URLLoaderInterceptor because we cannot control which port that
  // EmbeddedTestServer uses. Since origin trials depend on the entire origin
  // (including port) we need to intercept the requests using
  // URLLoaderInterceptor.
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end()) {
      return false;
    }

    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    std::string body = "<html><body>Hello world!</body></html>";
    if (test_options_.has_set_cookie) {
      base::StrAppend(
          &headers,
          {"Set-Cookie: __Host-foo=bar; Secure; Path=/; SameSite=None;",
           test_options_.has_partitioned ? " Partitioned" : "", "\n"});
    }
    if (test_options_.has_ot_token) {
      base::StrAppend(
          &headers,
          {"Origin-Trial: ",
           test_options_.valid_ot_token ? OriginTrialToken() : "invalid",
           "\n"});
    }
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get(),
                                        absl::nullopt,
                                        /*url=*/params->url_request.url);
    return true;
  }

  network::mojom::CookieManager* GetCookieManager() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess();
  }

  void SetCookie(const std::string& name,
                 const std::string& value,
                 const GURL& url,
                 const absl::optional<net::CookiePartitionKey>& partition_key) {
    auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
        name, value, url.host(), "/", base::Time::Now() - base::Days(1),
        base::Time::Now() + base::Days(1), base::Time::Now(), base::Time::Now(),
        /*secure=*/true, /*httponly=*/false,
        net::CookieSameSite::NO_RESTRICTION,
        net::CookiePriority::COOKIE_PRIORITY_DEFAULT, /*same_party=*/false,
        partition_key);
    EXPECT_TRUE(cookie->IsCanonical());

    base::RunLoop run_loop;
    GetCookieManager()->SetCanonicalCookie(
        *cookie, url, net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting(
            [&](net::CookieAccessResult set_cookie_result) {
              EXPECT_TRUE(set_cookie_result.status.IsInclude());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  std::vector<net::CanonicalCookie> GetCookies(const GURL& url) {
    std::vector<net::CanonicalCookie> cookies;

    base::RunLoop run_loop;
    GetCookieManager()->GetCookieList(
        url, net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection::ContainsAll(),
        base::BindLambdaForTesting(
            [&](const std::vector<::net::CookieWithAccessResult>& result,
                const std::vector<::net::CookieWithAccessResult>&
                    excluded_cookies) {
              EXPECT_TRUE(excluded_cookies.empty());
              for (const auto& el : result) {
                cookies.push_back(el.cookie);
              }
              run_loop.Quit();
            }));
    run_loop.Run();

    return cookies;
  }

  const GURL CookieUrl() { return GURL("https://127.0.0.1:44444"); }

  void WaitForPage(const GURL& url) {
    EXPECT_TRUE(NavigateToURL(shell(), url));
    WebContents* contents = shell()->web_contents();
    EXPECT_TRUE(WaitForLoadStop(contents));
    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
  }

 protected:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  OriginTrialTestOptions test_options_;
  std::set<GURL> expected_request_urls_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the partitioned cookie set before the request remains partitioned
// when the site sends a Set-Cookie header with the Partitioned attribute
// and a valid OT token.
IN_PROC_BROWSER_TEST_F(PartitionedCookiesOriginTrialBrowserTest,
                       ValidParticipant) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true, /*has_set_cookie=*/true,
       /*has_partitioned=*/true},
      {CookieUrl()});

  WaitForPage(CookieUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// Test that the partitioned cookie is reverted to unpartitioned if the site
// sends a Set-Cookie with Partitioned but an invalid OT token.
IN_PROC_BROWSER_TEST_F(PartitionedCookiesOriginTrialBrowserTest, InvalidToken) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/false, /*has_set_cookie=*/true,
       /*has_partitioned=*/true},
      {CookieUrl()});

  WaitForPage(CookieUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// Test that the partitioned cookie is reverted to unpartitioned if the site
// sends a Set-Cookie with Partitioned but do not send an OT token.
IN_PROC_BROWSER_TEST_F(PartitionedCookiesOriginTrialBrowserTest, NoToken) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/true,
                  /*has_partitioned=*/true},
                 {CookieUrl()});

  WaitForPage(CookieUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// The partitioned cookie should stay partitioned since we should not check
// the OT token on responses with no Set-Cookie header.
IN_PROC_BROWSER_TEST_F(PartitionedCookiesOriginTrialBrowserTest, NoSetCookie) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/false,
                  /*has_partitioned=*/true},
                 {CookieUrl()});

  WaitForPage(CookieUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// The partitioned cookie should stay partitioned since we should not check
// the OT token on responses with a Set-Cookie header without Partitioned.
IN_PROC_BROWSER_TEST_F(PartitionedCookiesOriginTrialBrowserTest,
                       NoPartitioned) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/true,
                  /*has_partitioned=*/false},
                 {CookieUrl()});

  WaitForPage(CookieUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// This class tests the origin trial mechanism for partitioned cookies.
// Partitioned cookies should be reverted to unpartitioned if the navigation
// has a Set-Cookie header with the Partitioned attribute and the site does
// not send a valid Origin-Trial header.
// This test exercises navigation requests in <iframe> embeds.
class EmbedPartitionedCookiesOriginTrialBrowserTest
    : public PartitionedCookiesOriginTrialBrowserTest {
 public:
  void SetUpOnMainThread() override {
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &EmbedPartitionedCookiesOriginTrialBrowserTest::InterceptRequest,
            base::Unretained(this)));
  }

  const char* OriginTrialToken() const override {
    // The test Origin Trial token was generated by running:
    // python tools/origin_trials/generate_token.py https://127.0.0.1:44444 \
    //     PartitionedCookies \
    //     --expire-timestamp=2000000000
    //     --is-third-party
    return "A1mBOyrOKGAaaoT8mjM1qSNrOSrdDUa9WyqicVLlDGW3feIBSdWqSiHDAXUeKkGKaVq"
           "UiCX8avwCM0gpG5LtxgAAAAByeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6ND"
           "Q0NDQiLCAiZmVhdHVyZSI6ICJQYXJ0aXRpb25lZENvb2tpZXMiLCAiZXhwaXJ5IjogM"
           "jAwMDAwMDAwMCwgImlzVGhpcmRQYXJ0eSI6IHRydWV9";
  }

  // We use URLLoaderInterceptor because we cannot control which port that
  // EmbeddedTestServer uses. Since origin trials depend on the entire origin
  // (including port) we need to intercept the requests using
  // URLLoaderInterceptor.
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end()) {
      return false;
    }

    if (params->url_request.url == TopLevelUrl()) {
      std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
      std::string body = "<html><body><iframe src=\"";
      base::StrAppend(&body, {CookieUrl().spec(), "\"></body></html>"});
      URLLoaderInterceptor::WriteResponse(headers, body, params->client.get(),
                                          absl::nullopt,
                                          /*url=*/params->url_request.url);
      return true;
    }

    return PartitionedCookiesOriginTrialBrowserTest::InterceptRequest(params);
  }

  GURL TopLevelUrl() { return GURL("https://mysite.com:44444"); }
};

// Test that the partitioned cookie set before the request remains partitioned
// when the site sends a Set-Cookie header with the Partitioned attribute
// and a valid OT token.
IN_PROC_BROWSER_TEST_F(EmbedPartitionedCookiesOriginTrialBrowserTest,
                       ValidParticipant) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true, /*has_set_cookie=*/true,
       /*has_partitioned=*/true},
      {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// Test that the partitioned cookie is reverted to unpartitioned if the site
// sends a Set-Cookie with Partitioned but an invalid OT token.
IN_PROC_BROWSER_TEST_F(EmbedPartitionedCookiesOriginTrialBrowserTest,
                       InvalidToken) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/false, /*has_set_cookie=*/true,
       /*has_partitioned=*/true},
      {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// Test that the partitioned cookie is reverted to unpartitioned if the site
// sends a Set-Cookie with Partitioned but do not send an OT token.
IN_PROC_BROWSER_TEST_F(EmbedPartitionedCookiesOriginTrialBrowserTest, NoToken) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/true,
                  /*has_partitioned=*/true},
                 {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// The partitioned cookie should stay partitioned since we should not check
// the OT token on responses with no Set-Cookie header.
IN_PROC_BROWSER_TEST_F(EmbedPartitionedCookiesOriginTrialBrowserTest,
                       NoSetCookie) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/false,
                  /*has_partitioned=*/true},
                 {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// The partitioned cookie should stay partitioned since we should not check
// the OT token on responses with a Set-Cookie header without Partitioned.
IN_PROC_BROWSER_TEST_F(EmbedPartitionedCookiesOriginTrialBrowserTest,
                       NoPartitioned) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/true,
                  /*has_partitioned=*/false},
                 {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// This test exercises the partitioned cookie origin trial for subresource
// requests. This browser test is meant to verify the feature works end-to-end
// though there is nothing about this test particularly related to navigation.
//
// I put the test here because I can subclass other partitioned cookies origin
// trial tests that do test navigation requests to reuse the test
// infrastructure.
// TODO(https://crbug.com/1296161): Move to another file/delete this test when
// OT is over.
class SubresourcePartitionedCookiesOriginTrialBrowserTest
    : public EmbedPartitionedCookiesOriginTrialBrowserTest {
 public:
  void SetUpOnMainThread() override {
    url_loader_interceptor_ = std::make_unique<
        URLLoaderInterceptor>(base::BindRepeating(
        &SubresourcePartitionedCookiesOriginTrialBrowserTest::InterceptRequest,
        base::Unretained(this)));
  }

  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end()) {
      return false;
    }

    if (params->url_request.url == TopLevelUrl()) {
      std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
      std::string body = "<html><body><script src=\"";
      base::StrAppend(&body,
                      {CookieUrl().spec(), "\"></script></body></html>"});
      URLLoaderInterceptor::WriteResponse(headers, body, params->client.get(),
                                          absl::nullopt,
                                          /*url=*/params->url_request.url);
      return true;
    }

    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/javascript\n";
    std::string body = "console.log('Hello world!');";
    if (test_options_.has_set_cookie) {
      base::StrAppend(
          &headers,
          {"Set-Cookie: __Host-foo=bar; Secure; Path=/; SameSite=None;",
           test_options_.has_partitioned ? " Partitioned" : "", "\n"});
    }
    if (test_options_.has_ot_token) {
      base::StrAppend(
          &headers,
          {"Origin-Trial: ",
           test_options_.valid_ot_token ? OriginTrialToken() : "invalid",
           "\n"});
    }
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get(),
                                        absl::nullopt,
                                        /*url=*/params->url_request.url);
    return true;
  }
};

// Test that the partitioned cookie set before the request remains partitioned
// when the site sends a Set-Cookie header with the Partitioned attribute
// and a valid OT token.
IN_PROC_BROWSER_TEST_F(SubresourcePartitionedCookiesOriginTrialBrowserTest,
                       ValidParticipant) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true, /*has_set_cookie=*/true,
       /*has_partitioned=*/true},
      {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// Test that the partitioned cookie is reverted to unpartitioned if the site
// sends a Set-Cookie with Partitioned but an invalid OT token.
IN_PROC_BROWSER_TEST_F(SubresourcePartitionedCookiesOriginTrialBrowserTest,
                       InvalidToken) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/false, /*has_set_cookie=*/true,
       /*has_partitioned=*/true},
      {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// Test that the partitioned cookie is reverted to unpartitioned if the site
// sends a Set-Cookie with Partitioned but do not send an OT token.
IN_PROC_BROWSER_TEST_F(SubresourcePartitionedCookiesOriginTrialBrowserTest,
                       NoToken) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/true,
                  /*has_partitioned=*/true},
                 {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_FALSE(cookies[0].IsPartitioned());
}

// The partitioned cookie should stay partitioned since we should not check
// the OT token on responses with no Set-Cookie header.
IN_PROC_BROWSER_TEST_F(SubresourcePartitionedCookiesOriginTrialBrowserTest,
                       NoSetCookie) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/false,
                  /*has_partitioned=*/true},
                 {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

// The partitioned cookie should stay partitioned since we should not check
// the OT token on responses with a Set-Cookie header without Partitioned.
IN_PROC_BROWSER_TEST_F(SubresourcePartitionedCookiesOriginTrialBrowserTest,
                       NoPartitioned) {
  SetCookie(
      "__Host-foo", "bar", CookieUrl(),
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
  SetTestOptions({/*has_ot_token=*/false, /*valid_ot_token=*/false,
                  /*has_set_cookie=*/true,
                  /*has_partitioned=*/false},
                 {TopLevelUrl(), CookieUrl()});

  WaitForPage(TopLevelUrl());

  auto cookies = GetCookies(CookieUrl());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
}

}  // namespace
}  // namespace content
