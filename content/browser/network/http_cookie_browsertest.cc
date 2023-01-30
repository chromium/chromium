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
        base::StringPrintf(R"({"primary": "https://%s",)"
                           R"("associatedSites": ["https://%s","https://%s"]})",
                           kHostA, kHostB, kHostC));
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

  GURL RedirectUrl(net::EmbeddedTestServer* test_server,
                   const std::string& host,
                   const GURL& target_url) {
    return test_server->GetURL(host, "/server-redirect?" + target_url.spec());
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

}  // namespace
}  // namespace content
