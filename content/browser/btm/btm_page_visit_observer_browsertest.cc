// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_page_visit_observer.h"

#include "base/feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/browser/btm/btm_page_visit_observer_test_utils.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-shared.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#endif

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;

namespace content {
namespace {

using blink::mojom::StorageTypeAccessed;

class BtmPageVisitObserverBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().Start());

    ukm_recorder_.emplace();
  }

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    ukm::InitializeSourceUrlRecorderForWebContents(shell()->web_contents());
  }

  const ukm::TestAutoSetUkmRecorder& ukm_recorder() {
    return ukm_recorder_.value();
  }

 protected:
  std::optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, SmokeTest) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  EXPECT_EQ(recorder.visits()[0].prev_page.url, GURL());
  EXPECT_EQ(recorder.visits()[0].url, url1);

  EXPECT_EQ(recorder.visits()[1].prev_page.url, url1);
  EXPECT_EQ(recorder.visits()[1].url, url2);

  EXPECT_EQ(recorder.visits()[2].prev_page.url, url2);
  EXPECT_EQ(recorder.visits()[2].url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, CreatedWhileOnPage) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  // Create the recorder (and thus the observer) while already at url1.
  BtmPageVisitRecorder recorder(web_contents);
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitRecorder::VisitTuple& first_visit = recorder.visits()[0];
  // Should be the URL we were on when observation started, instead of blank.
  EXPECT_THAT(first_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_EQ(first_visit.url, url2);

  const BtmPageVisitRecorder::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_EQ(second_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest,
                       SameDocumentNavigation) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/fragment.html");
  const GURL url1b =
      embedded_https_test_server().GetURL("a.test", "/fragment.html#fragment");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/fragment.html#fragment");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  // Perform a same-document navigation to a fragment. `NavigateToURL()`
  // automatically detects that the navigation should be same-document, since
  // the URLs differ only by fragment.
  ASSERT_TRUE(NavigateToURL(web_contents, url1b));
  // Perform a cross-document navigation to a fragment
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_THAT(first_visit.navigation.server_redirects, IsEmpty());
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Same-document navigations shouldn't be reported as server redirects.
  EXPECT_THAT(first_visit.navigation.server_redirects, IsEmpty());
  // Same-document navigations shouldn't be counted as page visits.
  EXPECT_EQ(second_visit.url, url2);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, Redirects) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  // url2 redirects to url2b which redirects to url3.
  const GURL url2 = embedded_https_test_server().GetURL(
      "b.test",
      "/server-redirect-with-cookie?%2Fcross-site%3Fc.test%252Fempty.html");
  const GURL url2b = embedded_https_test_server().GetURL(
      "b.test", "/cross-site?c.test%2Fempty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2, url3));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_THAT(first_visit.navigation.server_redirects, IsEmpty());
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_EQ(second_visit.navigation.server_redirects.size(), 2u);
  EXPECT_EQ(second_visit.url, url3);

  const BtmServerRedirectInfo& first_server_redirect =
      second_visit.navigation.server_redirects[0];
  EXPECT_THAT(first_server_redirect,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_TRUE(first_server_redirect.did_write_cookies);

  const BtmServerRedirectInfo& second_server_redirect =
      second_visit.navigation.server_redirects[1];
  EXPECT_THAT(second_server_redirect,
              HasUrlAndMatchingSourceId(url2b, &ukm_recorder()));
  EXPECT_FALSE(second_server_redirect.did_write_cookies);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, IframeRedirect) {
  const GURL top_level_url1 = embedded_https_test_server().GetURL(
      "a.test", "/page_with_blank_iframe.html");
  // Set in //content/test/data/page_with_blank_iframe.html.
  const std::string kIframeId = "test_iframe";
  // Starts a chain of 2 server redirects.
  const GURL iframe_redirector_url = embedded_https_test_server().GetURL(
      "b.test", "/server-redirect?%2Fcross-site%3Fc.test%252Fempty.html");
  const GURL top_level_url2 =
      embedded_https_test_server().GetURL("d.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, top_level_url1));
  ASSERT_TRUE(
      NavigateIframeToURL(web_contents, kIframeId, iframe_redirector_url));
  ASSERT_TRUE(NavigateToURL(web_contents, top_level_url2));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_THAT(first_visit.navigation.server_redirects, IsEmpty());
  EXPECT_EQ(first_visit.url, top_level_url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(top_level_url1, &ukm_recorder()));
  // Only redirects in top-level navigations should be reported; redirects in
  // iframes should be ignored.
  EXPECT_THAT(second_visit.navigation.server_redirects, IsEmpty());
  EXPECT_EQ(second_visit.url, top_level_url2);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, DocumentCookie) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(ExecJs(web_contents, "document.cookie = 'foo=bar';"));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_FALSE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  // url2 accessed cookies; no other page did.
  EXPECT_TRUE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, UserActivation) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  SimulateUserActivation(web_contents);
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.received_user_activation);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_TRUE(second_visit.prev_page.received_user_activation);
  EXPECT_EQ(second_visit.url, url2);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, NavigationInitiation) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  // Perform a browser-initiated navigation.
  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  // Perform a renderer-initiated navigation with user gesture.
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, url2));
  // Perform a renderer-initiated navigation without user gesture.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.navigation.was_renderer_initiated);
  EXPECT_TRUE(first_visit.navigation.was_user_initiated);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_TRUE(second_visit.navigation.was_renderer_initiated);
  EXPECT_TRUE(second_visit.navigation.was_user_initiated);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_TRUE(third_visit.navigation.was_renderer_initiated);
  EXPECT_FALSE(third_visit.navigation.was_user_initiated);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, VisitDuration) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  const base::TimeDelta time_elapsed_before_first_page_visit =
      base::Microseconds(777);
  const base::TimeDelta visit_duration1 = base::Minutes(2);
  const base::TimeDelta visit_duration2 = base::Milliseconds(888);
  WebContents* web_contents = shell()->web_contents();
  base::SimpleTestClock test_clock;
  test_clock.Advance(base::Hours(1));
  BtmPageVisitRecorder recorder(web_contents, &test_clock);

  test_clock.Advance(time_elapsed_before_first_page_visit);
  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  test_clock.Advance(visit_duration1);
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  test_clock.Advance(visit_duration2);
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_EQ(first_visit.prev_page.visit_duration,
            time_elapsed_before_first_page_visit);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_EQ(second_visit.prev_page.visit_duration, visit_duration1);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_EQ(third_visit.prev_page.visit_duration, visit_duration2);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, PageTransition) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  const ui::PageTransition transition_type2 = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  const ui::PageTransition transition_type3 = ui::PAGE_TRANSITION_LINK;
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  NavigationController::LoadURLParams navigation_params2(url2);
  navigation_params2.transition_type = transition_type2;
  NavigateToURLBlockUntilNavigationsComplete(web_contents, navigation_params2,
                                             1);
  NavigationController::LoadURLParams navigation_params3(url3);
  navigation_params3.transition_type = transition_type3;
  NavigateToURLBlockUntilNavigationsComplete(web_contents, navigation_params3,
                                             1);
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_THAT(first_visit.navigation.page_transition,
              CoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_THAT(second_visit.navigation.page_transition,
              CoreTypeIs(transition_type2));
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_THAT(third_visit.navigation.page_transition,
              CoreTypeIs(transition_type3));
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest,
                       NavigationalCookieAccesses) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/set-cookie?foo=bar");
  const GURL url2 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  // Perform a navigational cookie write for a.test.
  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  // Perform a navigational cookie read for a.test.
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  // End the second page visit on a.test by navigating away.
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  BtmPageVisitRecorder::VisitTuple first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_EQ(first_visit.url, url1);

  BtmPageVisitRecorder::VisitTuple second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Navigational cookie writes are active storage accesses and should be
  // reported.
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  BtmPageVisitRecorder::VisitTuple third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  // Navigational cookie reads are passive storage accesses and shouldn't be
  // reported.
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, SubresourceCookie) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(ExecJs(web_contents,
                     JsReplace(
                         R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                         "/set-cookie?foo=bar"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest,
                       IframeNavigationCookie) {
  const GURL url1 = embedded_https_test_server().GetURL(
      "a.test", "/page_with_blank_iframe.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateIframeToURL(
      web_contents, "test_iframe",
      embedded_https_test_server().GetURL("a.test", "/set-cookie?foo=bar")));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, IframeDocumentCookie) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/page_with_iframe.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  RenderFrameHost* iframe = ChildFrameAt(web_contents, 0);
  FrameCookieAccessObserver cookie_observer(web_contents, iframe,
                                            CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(iframe, "document.cookie = 'foo=bar';"));
  if (!base::FeatureList::IsEnabled(features::kBackForwardCache)) {
    // If the bfcache is disabled, we often don't receive the cookie
    // notification unless we wait for it before navigating away. (If the
    // bfcache *is* enabled, we *don't* want to wait -- that's part of the point
    // of this test.)
    cookie_observer.Wait();
  }
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest,
                       IframeSubresourceCookie) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/page_with_iframe.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(ExecJs(ChildFrameAt(web_contents, 0),
                     JsReplace(
                         R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                         "/set-cookie?foo=bar"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

class BtmPageVisitObserverClientRedirectBrowserTest
    : public BtmPageVisitObserverBrowserTest,
      public testing::WithParamInterface<BtmClientRedirectMethod> {
 protected:
  BtmClientRedirectMethod client_redirect_type() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(BtmPageVisitObserverClientRedirectBrowserTest,
                       ClientRedirect) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(
      PerformClientRedirect(client_redirect_type(), web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  // The client redirector shouldn't be reported as a server redirector.
  EXPECT_THAT(first_visit.navigation.server_redirects, IsEmpty());
  // The client redirector should be reported as a page visit instead.
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_THAT(second_visit.navigation.server_redirects, IsEmpty());
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_THAT(third_visit.navigation.server_redirects, IsEmpty());
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_P(BtmPageVisitObserverClientRedirectBrowserTest,
                       MixOfClientAndServerRedirects) {
  const GURL url1 = embedded_https_test_server().GetURL(
      "a.test", "/cross-site?b.test%2Fempty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 = embedded_https_test_server().GetURL(
      "c.test", "/cross-site-with-cookie?d.test%2Fempty.html");
  const GURL url4 =
      embedded_https_test_server().GetURL("d.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  // Navigate to a.test, which server-redirects to b.test.
  ASSERT_TRUE(NavigateToURL(web_contents, url1, url2));
  // Client-redirect from b.test to c.test, which in turn server-redirects to
  // d.test.
  ASSERT_TRUE(
      PerformClientRedirect(client_redirect_type(), web_contents, url3, url4));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  // Server redirector should be reported with correct URL, source ID, and
  // cookie access.
  EXPECT_EQ(first_visit.navigation.server_redirects.size(), 1u);
  EXPECT_THAT(first_visit.navigation.server_redirects[0],
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  EXPECT_FALSE(first_visit.navigation.server_redirects[0].did_write_cookies);
  // Client redirectors should be reported as page visits.
  EXPECT_EQ(first_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  // Server redirector should be reported with correct URL, source ID, and
  // cookie access.
  EXPECT_EQ(second_visit.navigation.server_redirects.size(), 1u);
  EXPECT_THAT(second_visit.navigation.server_redirects[0],
              HasUrlAndMatchingSourceId(url3, &ukm_recorder()));
  EXPECT_TRUE(second_visit.navigation.server_redirects[0].did_write_cookies);
  EXPECT_EQ(second_visit.url, url4);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BtmPageVisitObserverClientRedirectBrowserTest,
    kAllBtmClientRedirectMethods,
    [](const testing::TestParamInfo<
        BtmPageVisitObserverClientRedirectBrowserTest::ParamType>& param_info) {
      BtmClientRedirectMethod client_redirect_method = param_info.param;
      return StringifyBtmClientRedirectMethod(client_redirect_method);
    });

class BtmPageVisitObserverSiteDataAccessBrowserTest
    : public BtmPageVisitObserverBrowserTest,
      public testing::WithParamInterface<StorageTypeAccessed> {
 public:
  BtmPageVisitObserverSiteDataAccessBrowserTest()
      : prerender_test_helper_(
            base::BindRepeating(&BtmPageVisitObserverSiteDataAccessBrowserTest::
                                    GetActiveWebContents,
                                base::Unretained(this))) {}

  void SetUpOnMainThread() override {
    BtmPageVisitObserverBrowserTest::SetUpOnMainThread();
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  auto* fenced_frame_test_helper() { return &fenced_frame_test_helper_; }
  auto* prerender_test_helper() { return &prerender_test_helper_; }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
  test::PrerenderTestHelper prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_P(BtmPageVisitObserverSiteDataAccessBrowserTest,
                       PrimaryMainFrameAccess) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(AccessStorage(web_contents->GetPrimaryMainFrame(), GetParam()));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Storage accesses by the primary main frame should be attributed to that
  // page's visit.
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_P(BtmPageVisitObserverSiteDataAccessBrowserTest,
                       IframeAccess) {
  const GURL url1 = embedded_https_test_server().GetURL(
      "a.test", "/page_with_blank_iframe.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  RenderFrameHost* iframe = ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(AccessStorage(iframe, GetParam()));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Storage accesses by iframes should be attributed to the top-level frame.
  EXPECT_TRUE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}

IN_PROC_BROWSER_TEST_P(BtmPageVisitObserverSiteDataAccessBrowserTest,
                       FencedFrameAccess) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url = embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title0.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  std::unique_ptr<RenderFrameHostWrapper> fenced_frame =
      std::make_unique<RenderFrameHostWrapper>(
          fenced_frame_test_helper()->CreateFencedFrame(
              web_contents->GetPrimaryMainFrame(), fenced_frame_url));
  ASSERT_NE(fenced_frame, nullptr);
  ASSERT_TRUE(AccessStorage(fenced_frame->get(), GetParam()));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Storage accesses in fenced frames should be ignored.
  EXPECT_FALSE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

IN_PROC_BROWSER_TEST_P(BtmPageVisitObserverSiteDataAccessBrowserTest,
                       PrerenderingAccess) {
  // Prerendering pages do not have access to `StorageTypeAccessed::kFileSystem`
  // until activation (AKA becoming the primary page, whose test case is already
  // covered).
  if (GetParam() == StorageTypeAccessed::kFileSystem) {
    GTEST_SKIP();
  }

  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  const GURL prerendering_url =
      embedded_https_test_server().GetURL("a.test", "/title2.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  // Navigate to url1, and prerender a page that accesses storage.
  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  const FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  test::PrerenderHostObserver observer(*web_contents, host_id);
  ASSERT_FALSE(observer.was_activated());
  RenderFrameHost* prerender_frame =
      prerender_test_helper()->GetPrerenderedMainFrameHost(host_id);
  ASSERT_NE(prerender_frame, nullptr);
  ASSERT_TRUE(AccessStorage(prerender_frame, GetParam()));
  prerender_test_helper()->CancelPrerenderedPage(host_id);
  observer.WaitForDestroyed();
  // End the page visit on url1.
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(recorder.WaitForSize(2));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Storage accesses in prerendered pages should be ignored.
  EXPECT_FALSE(second_visit.prev_page.had_qualifying_storage_access);
  EXPECT_EQ(second_visit.url, url2);

  EXPECT_EQ(recorder.visits().size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BtmPageVisitObserverSiteDataAccessBrowserTest,
    ::testing::Values(StorageTypeAccessed::kLocalStorage,
                      StorageTypeAccessed::kSessionStorage,
                      StorageTypeAccessed::kCacheStorage,
                      StorageTypeAccessed::kFileSystem,
                      StorageTypeAccessed::kIndexedDB),
    [](const testing::TestParamInfo<
        BtmPageVisitObserverSiteDataAccessBrowserTest::ParamType>& param_info) {
      // We drop the first character of ToString(param) because it's just the
      // constant-indicating 'k'.
      return base::ToString(param_info.param).substr(1);
    });

// WebAuthn tests do not work on Android because there is
// currently no way to install a virtual authenticator.
// TODO(crbug.com/40269763): Implement automated testing
// once the infrastructure permits it (Requires mocking
// the Android Platform Authenticator i.e. GMS Core).
#if !BUILDFLAG(IS_ANDROID)
class BtmPageVisitObserverWebAuthnTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Allowlist all certs for the HTTPS server.
    mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().Start());

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();

    virtual_device_factory->mutable_state()->InjectResidentKey(
        std::vector<uint8_t>{1, 2, 3, 4}, authn_hostname,
        std::vector<uint8_t>{5, 6, 7, 8}, "Foo", "Foo Bar");

    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));

    auth_env_ = std::make_unique<ScopedAuthenticatorEnvironmentForTesting>(
        std::move(virtual_device_factory));

    ukm_recorder_.emplace();
  }

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    ukm::InitializeSourceUrlRecorderForWebContents(shell()->web_contents());
  }

  void PostRunTestOnMainThread() override {
    auth_env_.reset();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  void GetWebAuthnAssertion() {
    ASSERT_EQ("OK", EvalJs(GetActiveWebContents(), R"(
    let cred_id = new Uint8Array([1,2,3,4]);
    navigator.credentials.get({
      publicKey: {
        challenge: cred_id,
        userVerification: 'preferred',
        allowCredentials: [{
          type: 'public-key',
          id: cred_id,
          transports: ['usb', 'nfc', 'ble'],
        }],
        timeout: 10000
      }
    }).then(c => 'OK',
      e => e.toString());
  )",
                           EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  const ukm::TestAutoSetUkmRecorder& ukm_recorder() {
    return ukm_recorder_.value();
  }

  ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

 protected:
  const std::string authn_hostname = std::string("a.test");
  std::optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

 private:
  ContentMockCertVerifier mock_cert_verifier_;
  std::unique_ptr<ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverWebAuthnTest, SuccessfulWAA) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  GetWebAuthnAssertion();
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  const BtmPageVisitObserver::VisitTuple& first_visit = recorder.visits()[0];
  EXPECT_THAT(first_visit.prev_page, HasUrlAndSourceIdForBlankPage());
  EXPECT_FALSE(first_visit.prev_page.had_successful_web_authn_assertion);
  EXPECT_EQ(first_visit.url, url1);

  const BtmPageVisitObserver::VisitTuple& second_visit = recorder.visits()[1];
  EXPECT_THAT(second_visit.prev_page,
              HasUrlAndMatchingSourceId(url1, &ukm_recorder()));
  // Successful WAAs should be reported.
  EXPECT_TRUE(second_visit.prev_page.had_successful_web_authn_assertion);
  EXPECT_EQ(second_visit.url, url2);

  const BtmPageVisitObserver::VisitTuple& third_visit = recorder.visits()[2];
  EXPECT_THAT(third_visit.prev_page,
              HasUrlAndMatchingSourceId(url2, &ukm_recorder()));
  EXPECT_FALSE(third_visit.prev_page.had_successful_web_authn_assertion);
  EXPECT_EQ(third_visit.url, url3);

  EXPECT_EQ(recorder.visits().size(), 3u);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace content
