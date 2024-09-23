// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Handles |request| by serving a response with title set to request contents.
std::unique_ptr<net::test_server::HttpResponse> HandleEchoTitleRequest(
    const std::string& echotitle_path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, echotitle_path,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      base::StringPrintf(
          "<html><head><title>%s</title></head></html>",
          request.content.c_str()));
  return std::move(http_response);
}

}  // namespace

class SessionHistoryTest : public ContentBrowserTest {
 protected:
  SessionHistoryTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleEchoTitleRequest, "/echotitle"));

    ASSERT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  }

  // Simulate clicking a link.  Only works on the frames.html testserver page.
  void ClickLink(const std::string& node_id) {
    TestNavigationObserver observer(shell()->web_contents());
    shell()->LoadURL(GURL("javascript:clickLink('" + node_id + "')"));
    observer.Wait();
  }

  // Simulate submitting a form.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.  Assumes that the form
  // submission triggers a navigation and waits for that navigation to complete
  // before returning.  Expects caller to validate the new URL after the
  // navigation.
  void SubmitForm(const std::string& node_id) {
    TestNavigationObserver observer(shell()->web_contents());
    shell()->LoadURL(GURL("javascript:submitForm('" + node_id + "')"));
    observer.Wait();
  }

  // Navigate session history using history.go(distance).
  void JavascriptGo(const std::string& distance) {
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(ToRenderFrameHost(shell()->web_contents()),
                       "history.go('" + distance + "')"));
    observer.Wait();
  }

  std::string GetTabTitle() {
    return base::UTF16ToASCII(shell()->web_contents()->GetTitle());
  }

  GURL GetTabURL() {
    return shell()->web_contents()->GetLastCommittedURL();
  }

  GURL GetURL(const std::string& file) {
    return embedded_test_server()->GetURL(
        std::string("/session_history/") + file);
  }

  void NavigateAndCheckTitle(const char* filename,
                             const std::string& expected_title) {
    std::u16string expected_title16(base::ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(filename)));
    ASSERT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  bool CanGoBack() {
    return shell()->web_contents()->GetController().CanGoBack();
  }

  bool CanGoForward() {
    return shell()->web_contents()->GetController().CanGoForward();
  }

  void GoBack() {
    LoadStopObserver load_stop_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    load_stop_observer.Wait();
  }

  void GoForward() {
    LoadStopObserver load_stop_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    load_stop_observer.Wait();
  }
};

class SessionHistoryScrollAnchorTest : public SessionHistoryTest {
 protected:
  SessionHistoryScrollAnchorTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SessionHistoryTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ScrollAnchorSerialization");
  }
};

// If this flakes, use http://crbug.com/61619 on windows and
// http://crbug.com/102094 on mac.
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, BasicBackForward) {
  ASSERT_FALSE(CanGoBack());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot1.html", "bot1"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot2.html", "bot2"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, bot2, *bot3]

  GoBack();
  EXPECT_EQ("bot2", GetTabTitle());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());

  GoForward();
  EXPECT_EQ("bot2", GetTabTitle());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(CanGoForward());
  EXPECT_EQ("bot3", GetTabTitle());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());

  GoBack();
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());

  ASSERT_FALSE(CanGoBack());
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());

  GoForward();
  EXPECT_EQ("bot1", GetTabTitle());

  GoForward();
  EXPECT_EQ("bot3", GetTabTitle());
}

// Test that back/forward works when navigating in subframes.
// If this flakes, use http://crbug.com/48833
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, FrameBackForward) {
  ASSERT_FALSE(CanGoBack());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("frames.html", "bot1"));

  ClickLink("abot2");
  EXPECT_EQ("bot2", GetTabTitle());
  GURL frames(GetURL("frames.html"));
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot3");
  EXPECT_EQ("bot3", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot3]

  GoBack();
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());
  EXPECT_EQ(GURL(url::kAboutBlankURL), GetTabURL());

  GoForward();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoForward();
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot1");
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot1]

  ASSERT_FALSE(CanGoForward());
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// Test that back/forward preserves POST data and document state in subframes.
// If this flakes use http://crbug.com/61619
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, FrameFormBackForward) {
  ASSERT_FALSE(CanGoBack());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("frames.html", "bot1"));

  ClickLink("aform");
  EXPECT_EQ("form", GetTabTitle());
  GURL frames(GetURL("frames.html"));
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  ClickLink("abot2");
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *bot2]

  GoBack();
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *post]

  // TODO(mpcomplete): reenable this when WebKit bug 10199 is fixed:
  // "returning to a POST result within a frame does a GET instead of a POST"
  ClickLink("abot2");
  EXPECT_EQ("bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  GoBack();
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

IN_PROC_BROWSER_TEST_F(SessionHistoryTest, CrossFrameFormBackForward) {
  ASSERT_FALSE(CanGoBack());

  GURL frames(GetURL("frames.html"));
  // Open a page with "ftop" and  "fbot" iframe.
  // The title of the main frame follows the title of the "fbot" iframe.
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("frames.html", "bot1"));

  // Click link in the "fbot" iframe. This updates the title of the main frame
  // to "form".
  ClickLink("aform");
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // Submit form in the "fbot" iframe. This submits to /echotitle which sets the
  // title to the submission content of the form.
  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // Go back, navigating the "fbot"  iframe. This updates the title of the main
  // frame back to "form".
  GoBack();
  EXPECT_EQ("form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  // Navigate the main frame.
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot2.html", "bot2"));

  // history is [blank, bot1, form, *bot2]

  // Navigate the main frame back. If back/forward cache is enabled, the page
  // will be restored as it was before we navigated away from it, with the title
  // set to "form". If not, the page will be reloaded from scratch, setting the
  // title to "bot1" again.
  GoBack();
  EXPECT_EQ(IsBackForwardCacheEnabled() ? "form" : "bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // Submit the form in the "fbot" iframe again . This submits to /echotitle
  // which sets the title to the submission content of the form.
  SubmitForm("isubmit");
  EXPECT_EQ("text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// Test that back/forward entries are created for reference fragment
// navigations. Bug 730379.
// If this flakes use http://crbug.com/61619.
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, FragmentBackForward) {
  ASSERT_FALSE(CanGoBack());

  GURL fragment(GetURL("fragment.html"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html", "fragment"));

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html#a", "fragment"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html#b", "fragment"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("fragment.html#c", "fragment"));

  // history is [blank, fragment, fragment#a, fragment#b, *fragment#c]

  GoBack();
  EXPECT_EQ(GetURL("fragment.html#b"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html#a"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html"), GetTabURL());

  GoForward();
  EXPECT_EQ(GetURL("fragment.html#a"), GetTabURL());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, fragment, fragment#a, bot3]

  ASSERT_FALSE(CanGoForward());
  EXPECT_EQ(GetURL("bot3.html"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html#a"), GetTabURL());

  GoBack();
  EXPECT_EQ(GetURL("fragment.html"), GetTabURL());
}

// Test that the javascript window.history object works.
// NOTE: history.go(N) does not do anything if N is outside the bounds of the
// back/forward list (such as trigger our start/stop loading events).  This
// means the test will hang if it attempts to navigate too far forward or back,
// since we'll be waiting forever for a load stop event.
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, JavascriptHistory) {
  ASSERT_FALSE(CanGoBack());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot1.html", "bot1"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot2.html", "bot2"));
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-1");
  EXPECT_EQ("bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("2");
  EXPECT_EQ("bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-3");
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());

  ASSERT_FALSE(CanGoBack());
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot1", GetTabTitle());

  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle("bot3.html", "bot3"));

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(CanGoForward());
  EXPECT_EQ("bot3", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());

  ASSERT_FALSE(CanGoBack());
  EXPECT_EQ(std::string(url::kAboutBlankURL), GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ("bot3", GetTabTitle());

  // TODO(creis): Test that JavaScript history navigations work across tab
  // types.  For example, load about:network in a tab, then a real page, then
  // try to go back and forward with JavaScript.  Bug 1136715.
  // (Hard to test right now, because pages like about:network cause the
  // TabProxy to hang.)
}

IN_PROC_BROWSER_TEST_F(SessionHistoryTest, LocationReplace) {
  // Test that using location.replace doesn't leave the title of the old page
  // visible.
  std::u16string expected_title16(u"bot1");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("replace.html?bot1.html"),
                            GetURL("bot1.html") /* expected_commit_url */));
  ASSERT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(SessionHistoryTest, LocationChangeInSubframe) {
  ASSERT_NO_FATAL_FAILURE(NavigateAndCheckTitle(
      "location_redirect.html", "Default Title"));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  TestFrameNavigationObserver observer(root->child_at(0));
  shell()->LoadURL(GURL("javascript:void(frames[0].navigate())"));
  observer.Wait();
  EXPECT_EQ("foo", GetTabTitle());
  EXPECT_EQ(GetURL("location_redirect_frame2.html"),
            root->child_at(0)->current_url());

  GoBack();
  EXPECT_EQ("Default Title", GetTabTitle());
}

IN_PROC_BROWSER_TEST_F(SessionHistoryScrollAnchorTest,
                       LocationChangeInSubframe) {
  ASSERT_NO_FATAL_FAILURE(
      NavigateAndCheckTitle("location_redirect.html", "Default Title"));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  TestFrameNavigationObserver observer(root->child_at(0));
  shell()->LoadURL(GURL("javascript:void(frames[0].navigate())"));
  observer.Wait();
  EXPECT_EQ("foo", GetTabTitle());
  EXPECT_EQ(GetURL("location_redirect_frame2.html"),
            root->child_at(0)->current_url());

  GoBack();
  EXPECT_EQ("Default Title", GetTabTitle());
}

// http://code.google.com/p/chromium/issues/detail?id=56267
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, HistoryLength) {
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  EXPECT_EQ(2, EvalJs(shell(), "history.length"));

  // Now test that history.length is updated when the navigation is committed.
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("record_length.html")));

  EXPECT_EQ(3, EvalJs(shell(), "history.length"));

  GoBack();
  GoBack();

  // Ensure history.length is properly truncated.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
}

// Test that verifies that a cross-process transfer doesn't lose session
// history state - https://crbug.com/613004.
//
// Trigerring a cross-process transfer via embedded_test_server requires use of
// a HTTP redirect response (to preserve port number).  Therefore the test ends
// up accidentally testing redirection logic as well - in particular, the test
// uses 307 (rather than 302) redirect to preserve the body of HTTP POST across
// redirects (as mandated by https://tools.ietf.org/html/rfc7231#section-6.4.7).
IN_PROC_BROWSER_TEST_F(SessionHistoryTest, GoBackToCrossSitePostWithRedirect) {
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  GURL page_to_go_back_from(
      embedded_test_server()->GetURL("c.com", "/title1.html"));

  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "document.getElementById('text-form').submit()"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());

  // Verify that POST body got preserved by 307 redirect.  This expectation
  // comes from: https://tools.ietf.org/html/rfc7231#section-6.4.7
  EXPECT_EQ(
      "text=value\n",
      EvalJs(shell(), "document.getElementsByTagName('pre')[0].innerText"));

  // Navigate to a page from yet another site.
  EXPECT_TRUE(NavigateToURL(shell(), page_to_go_back_from));

  // Go back - this should resubmit form's post data.
  TestNavigationObserver back_nav_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().GoBack();
  back_nav_observer.Wait();

  // Again verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());

  // Again verify that POST body got preserved by 307 redirect.
  EXPECT_EQ(
      "text=value\n",
      EvalJs(shell(), "document.getElementsByTagName('pre')[0].innerText"));
}

}  // namespace content
