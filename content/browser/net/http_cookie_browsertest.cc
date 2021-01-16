// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::UnorderedElementsAre;

// Matches a CanonicalCookie with the given name.
MATCHER_P(CookieWithName, name, "") {
  return testing::ExplainMatchResult(testing::Eq(name), arg.Name(),
                                     result_listener);
}

// Splits a string into key-value pairs, and executes the provided matcher on
// the result.
MATCHER_P3(WhenKVSplit, pair_delim, kv_delim, inner_matcher, "") {
  std::vector<std::pair<std::string, std::string>> pairs;
  // Ignore the return value of SplitStringIntoKeyValuePairs, to allow pairs
  // with no associated value.
  base::SplitStringIntoKeyValuePairs(arg, kv_delim, pair_delim, &pairs);
  return testing::ExplainMatchResult(inner_matcher, pairs, result_listener);
}

// Splits a ';'-delimited string of Cookie 'name=value' or 'name' pairs, and
// executes the provided matcher on the result.
MATCHER_P(CookieString, inner_matcher, "") {
  return testing::ExplainMatchResult(WhenKVSplit(';', '=', inner_matcher), arg,
                                     result_listener);
}

// This file contains tests for cookie access via HTTP requests.
// See also (tests for cookie access via JavaScript):
// //content/browser/renderer_host/cookie_browsertest.cc

class HttpCookieBrowserTest : public ContentBrowserTest {
 public:
  HttpCookieBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

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

  RenderFrameHostImpl* SelectDescendentFrame(
      const std::vector<int>& indices) const {
    RenderFrameHostImpl* selected_frame = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
    for (int index : indices) {
      selected_frame = selected_frame->child_at(index)->current_frame_host();
    }
    return selected_frame;
  }

  std::string ExtractFrameContent(RenderFrameHost* frame) const {
    std::string content;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        frame, "window.domAutomationController.send(document.body.textContent)",
        &content));
    return content;
  }

  void NavigateFrameHostToURL(RenderFrameHost* iframe, const GURL& url) {
    TestNavigationObserver nav_observer(shell()->web_contents());
    ExecuteScriptAsync(iframe, JsReplace("location = $1", url));
    nav_observer.Wait();
  }

  RenderFrameHostImpl* ArrangeFrames(const std::string& frame_tree,
                                     const std::vector<int>& leaf_path) {
    EXPECT_TRUE(NavigateToURL(
        shell()->web_contents(),
        https_server()->GetURL(
            frame_tree.substr(0, frame_tree.find("(")),
            base::StrCat({"/cross_site_iframe_factory.html?", frame_tree}))));
    return SelectDescendentFrame(leaf_path);
  }

  // Returns the contents of the Cookie header from the leaf frame.
  std::string ArrangeFramesAndGetCookiesFromLeaf(
      const std::string& frame_tree,
      const std::vector<int>& leaf_path) {
    // NB: This doesn't get the cookies that were sent when the leaf is
    // initially loaded; it instead loads the leaf, then does a same-site
    // navigation to get to /echoheader?Cookie. This results in a different
    // initiator origin; loading /echoheader?Cookie directly would have been a
    // cross-site navigation.
    //
    // In practice this is ok, since the scenarios where
    // this difference matters are the scenarios where the frame is cross-site
    // anyway, so the only cookies that would be sent are SameSite=None (or
    // potentially SameParty).
    //
    // We could consider making a custom cross_site_iframe_factory (or modifying
    // the existing one) to navigate to /echoheader?Cookie directly in the
    // leaves.
    RenderFrameHostImpl* leaf = ArrangeFrames(frame_tree, leaf_path);
    NavigateFrameHostToURL(
        leaf, https_server()->GetURL(leaf->GetLastCommittedOrigin().host(),
                                     "/echoheader?Cookie"));
    return ExtractFrameContent(leaf);
  }

  // Returns the cookies set by the leaf frame.
  std::vector<net::CanonicalCookie> ArrangeFramesAndSetCookiesInLeaf(
      const std::string& frame_tree,
      const std::vector<int>& leaf_path,
      const std::string& cookies_url) {
    RenderFrameHostImpl* leaf = ArrangeFrames(frame_tree, leaf_path);
    const std::string& leaf_host = leaf->GetLastCommittedOrigin().host();
    NavigateFrameHostToURL(leaf,
                           https_server()->GetURL(leaf_host, cookies_url));
    return GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(),
                               https_server()->GetURL(leaf_host, "/"));
  }

  uint32_t ClearCookies() {
    return DeleteCookies(shell()->web_contents()->GetBrowserContext(),
                         network::mojom::CookieDeletionFilter());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(HttpCookieBrowserTest, SendSameSiteCookies) {
  SetSameSiteCookies(kHostA);
  SetSameSiteCookies(kHostB);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Main frame browser-initiated navigation sends all SameSite cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(web_contents->GetMainFrame()),
      CookieString(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Main frame same-site (A => A) navigation sends all SameSite cookies.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents->GetMainFrame(),
      https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(web_contents->GetMainFrame()),
      CookieString(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Main frame cross-site (A => B) navigation sends all but Strict cookies.
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents->GetMainFrame(),
      https_server()->GetURL(kHostB, "/echoheader?Cookie")));
  EXPECT_THAT(ExtractFrameContent(web_contents->GetMainFrame()),
              CookieString(UnorderedElementsAre(
                  Key(kSameSiteLaxCookieName), Key(kSameSiteNoneCookieName),
                  Key(kSameSiteUnspecifiedCookieName))));

  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostA, "/page_with_iframe.html")));

  // Same-site iframe (A embedded in A) sends all SameSite cookies.
  ASSERT_TRUE(NavigateIframeToURL(
      web_contents, "test_iframe",
      https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(
      ExtractFrameContent(SelectDescendentFrame({0})),
      CookieString(UnorderedElementsAre(
          Key(kSameSiteStrictCookieName), Key(kSameSiteLaxCookieName),
          Key(kSameSiteNoneCookieName), Key(kSameSiteUnspecifiedCookieName))));

  // Cross-site iframe (B embedded in A) sends only None cookies.
  ASSERT_TRUE(NavigateIframeToURL(
      web_contents, "test_iframe",
      https_server()->GetURL(kHostB, "/echoheader?Cookie")));
  EXPECT_THAT(ExtractFrameContent(SelectDescendentFrame({0})),
              CookieString(UnorderedElementsAre(Key(kSameSiteNoneCookieName))));
}

IN_PROC_BROWSER_TEST_F(HttpCookieBrowserTest, SetSameSiteCookies) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Main frame can set all SameSite cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostA, kSetSameSiteCookiesURL)));
  EXPECT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(),
                          https_server()->GetURL(kHostA, "/")),
      UnorderedElementsAre(CookieWithName(kSameSiteStrictCookieName),
                           CookieWithName(kSameSiteLaxCookieName),
                           CookieWithName(kSameSiteNoneCookieName),
                           CookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // Same-site iframe (A embedded in A) sets all SameSite cookies.
  EXPECT_THAT(
      ArrangeFramesAndSetCookiesInLeaf("a.test(a.test)", {0},
                                       kSetSameSiteCookiesURL),
      UnorderedElementsAre(CookieWithName(kSameSiteStrictCookieName),
                           CookieWithName(kSameSiteLaxCookieName),
                           CookieWithName(kSameSiteNoneCookieName),
                           CookieWithName(kSameSiteUnspecifiedCookieName)));
  ASSERT_EQ(4U, ClearCookies());

  // Cross-site iframe (B embedded in A) sets only None cookies.
  EXPECT_THAT(ArrangeFramesAndSetCookiesInLeaf("a.test(b.test)", {0},
                                               kSetSameSiteCookiesURL),
              UnorderedElementsAre(CookieWithName(kSameSiteNoneCookieName)));
  ASSERT_EQ(1U, ClearCookies());
}

IN_PROC_BROWSER_TEST_F(HttpCookieBrowserTest, SendSamePartyCookies) {
  SetSamePartyCookies(kHostA);
  SetSamePartyCookies(kHostD);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // No embedded frame. The top-level site has access to its cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostA, "/echoheader?Cookie")));
  EXPECT_THAT(ExtractFrameContent(web_contents->GetMainFrame()),
              CookieString(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Same-site FPS-member iframe (A embedded in A) sends A's SameParty cookies.
  EXPECT_THAT(ArrangeFramesAndGetCookiesFromLeaf("a.test(a.test)", {0}),
              CookieString(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, same-party iframe (B embedded in A) does not send A's SameParty
  // cookies (since it's the wrong domain).
  EXPECT_EQ(ArrangeFramesAndGetCookiesFromLeaf("a.test(b.test)", {0}), "None");

  // Cross-site, same-party iframe (A embedded in B) sends A's SameParty
  // cookies.
  EXPECT_THAT(ArrangeFramesAndGetCookiesFromLeaf("b.test(a.test)", {0}),
              CookieString(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, same-party nested iframe (A embedded in B embedded in A) sends
  // A's SameParty cookies.
  EXPECT_THAT(
      ArrangeFramesAndGetCookiesFromLeaf("a.test(b.test(a.test))", {0, 0}),
      CookieString(UnorderedElementsAre(Key(kSamePartyLaxCookieName),
                                        Key(kSamePartyNoneCookieName),
                                        Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A) sends A's SameParty cookies.
  EXPECT_THAT(ArrangeFramesAndGetCookiesFromLeaf(
                  "a.test(c.test(b.test(a.test)))", {0, 0, 0}),
              CookieString(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));

  // Cross-site, cross-party iframe (D embedded in A) sends only D's
  // SameSite=None cookie, since D is not in A's First-Party Set.
  EXPECT_THAT(
      ArrangeFramesAndGetCookiesFromLeaf("a.test(d.test)", {0}),
      CookieString(UnorderedElementsAre(Key(kSamePartyNoneCookieName))));

  // Cross-site, cross-party iframe (A embedded in D) doesn't send A's SameParty
  // cookies.
  EXPECT_EQ(ArrangeFramesAndGetCookiesFromLeaf("d.test(a.test)", {0}), "None");

  // Cross-site, cross-party nested iframe (A embedded in B embedded in D)
  // doesn't send A's SameParty cookies.
  EXPECT_EQ(
      ArrangeFramesAndGetCookiesFromLeaf("d.test(b.test(a.test))", {0, 0}),
      "None");

  // No embedded frame. The top-level site has access to its cookies, regardless
  // of whether the site is in an FPS, or whether the cookies are SameParty.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostD, "/echoheader?Cookie")));
  EXPECT_THAT(ExtractFrameContent(web_contents->GetMainFrame()),
              CookieString(UnorderedElementsAre(
                  Key(kSamePartyLaxCookieName), Key(kSamePartyNoneCookieName),
                  Key(kSamePartyUnspecifiedCookieName))));
}

IN_PROC_BROWSER_TEST_F(HttpCookieBrowserTest, SetSamePartyCookies) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // No embedded frame, FPS member. The top-level FPS site can set its cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostA, kSetSamePartyCookiesURL)));
  EXPECT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(),
                          https_server()->GetURL(kHostA, "/")),
      UnorderedElementsAre(CookieWithName(kSamePartyLaxCookieName),
                           CookieWithName(kSamePartyNoneCookieName),
                           CookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Same-site FPS-member iframe (A embedded in A) sets A's SameParty cookies.
  EXPECT_THAT(
      ArrangeFramesAndSetCookiesInLeaf("a.test(a.test)", {0},
                                       kSetSamePartyCookiesURL),
      UnorderedElementsAre(CookieWithName(kSamePartyLaxCookieName),
                           CookieWithName(kSamePartyNoneCookieName),
                           CookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, same-party iframe (A embedded in B) sets A's SameParty
  // cookies.
  EXPECT_THAT(
      ArrangeFramesAndSetCookiesInLeaf("b.test(a.test)", {0},
                                       kSetSamePartyCookiesURL),
      UnorderedElementsAre(CookieWithName(kSamePartyLaxCookieName),
                           CookieWithName(kSamePartyNoneCookieName),
                           CookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, same-party nested iframe (A embedded in B embedded in A) sets
  // A's SameParty cookies.
  EXPECT_THAT(
      ArrangeFramesAndSetCookiesInLeaf("a.test(b.test(a.test))", {0, 0},
                                       kSetSamePartyCookiesURL),
      UnorderedElementsAre(CookieWithName(kSamePartyLaxCookieName),
                           CookieWithName(kSamePartyNoneCookieName),
                           CookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, same-party nested iframe (A embedded in B embedded in C
  // embedded in A) sets A's SameParty cookies.
  EXPECT_THAT(
      ArrangeFramesAndSetCookiesInLeaf("a.test(c.test(b.test(a.test)))",
                                       {0, 0, 0}, kSetSamePartyCookiesURL),
      UnorderedElementsAre(CookieWithName(kSamePartyLaxCookieName),
                           CookieWithName(kSamePartyNoneCookieName),
                           CookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());

  // Cross-site, cross-party iframe (D embedded in A) sets D's SameSite=None
  // cookie, since it's not an FPS member (and SameParty is ignored).
  EXPECT_THAT(ArrangeFramesAndSetCookiesInLeaf("a.test(d.test)", {0},
                                               kSetSamePartyCookiesURL),
              UnorderedElementsAre(CookieWithName(kSamePartyNoneCookieName)));
  ASSERT_EQ(1U, ClearCookies());

  // Cross-site, cross-party iframe (A embedded in D) doesn't set A's SameParty
  // cookies, since A is an FPS member and SameParty is not ignored..
  EXPECT_THAT(ArrangeFramesAndSetCookiesInLeaf("d.test(a.test)", {0},
                                               kSetSamePartyCookiesURL),
              IsEmpty());
  ASSERT_EQ(0U, ClearCookies());

  // Cross-site, cross-party nested iframe (A embedded in B embedded in D)
  // doesn't set A's SameParty cookies.
  EXPECT_THAT(ArrangeFramesAndSetCookiesInLeaf("d.test(b.test(a.test))", {0, 0},
                                               kSetSamePartyCookiesURL),
              IsEmpty());
  ASSERT_EQ(0U, ClearCookies());

  // No embedded frame, non-FPS member. The top-level site can set its cookies.
  ASSERT_TRUE(NavigateToURL(
      web_contents, https_server()->GetURL(kHostD, kSetSamePartyCookiesURL)));
  EXPECT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(),
                          https_server()->GetURL(kHostD, "/")),
      UnorderedElementsAre(CookieWithName(kSamePartyLaxCookieName),
                           CookieWithName(kSamePartyNoneCookieName),
                           CookieWithName(kSamePartyUnspecifiedCookieName)));
  ASSERT_EQ(3U, ClearCookies());
}

}  // namespace
}  // namespace content
