// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_page_visit_observer.h"

#include <memory>

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/btm/btm_page_visit_observer_test_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_web_contents.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;

namespace content {

class BtmPageVisitObserverTest : public testing::Test {
 public:
  WebContents* web_contents() { return web_contents_.get(); }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<WebContents> web_contents_ =
      WebContentsTester::CreateTestWebContents(&browser_context_,
                                               /*SiteInstance=*/nullptr);
};

TEST_F(BtmPageVisitObserverTest, PreviousPage) {
  auto* tester = WebContentsTester::For(web_contents());
  const GURL url1("http://a.test/");
  const GURL url2("http://b.test/");
  BtmPageVisitRecorder recorder(web_contents());

  tester->NavigateAndCommit(url1);
  ASSERT_TRUE(recorder.WaitForSize(1));
  EXPECT_EQ(recorder.visits()[0].prev_page.url, GURL());
  EXPECT_EQ(recorder.visits()[0].navigation.destination.url, url1);

  tester->NavigateAndCommit(url2);
  ASSERT_TRUE(recorder.WaitForSize(2));
  EXPECT_EQ(recorder.visits()[1].prev_page.url, url1);
  EXPECT_EQ(recorder.visits()[1].navigation.destination.url, url2);
}

TEST_F(BtmPageVisitObserverTest, ServerRedirects) {
  const GURL url1("http://a.test/");
  const GURL url2("http://b.test/");
  const GURL url3("http://c.test/");
  BtmPageVisitRecorder recorder(web_contents());

  // Navigate to url1.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  // Navigate to url2, but redirect to url3.
  auto nav = NavigationSimulator::CreateBrowserInitiated(url2, web_contents());
  nav->Start();
  nav->Redirect(url3);
  nav->Commit();
  ASSERT_TRUE(recorder.WaitForSize(2));

  // Two navigations are observed, the second with a redirect.
  const BtmNavigationInfo& navigation1 = recorder.visits()[0].navigation;
  EXPECT_EQ(navigation1.server_redirects.size(), 0u);
  EXPECT_EQ(navigation1.destination.url, url1);
  const BtmNavigationInfo& navigation2 = recorder.visits()[1].navigation;
  ASSERT_EQ(navigation2.server_redirects.size(), 1u);
  EXPECT_EQ(navigation2.server_redirects[0].url, url2);
  EXPECT_EQ(navigation2.destination.url, url3);
}

TEST_F(BtmPageVisitObserverTest, IgnoreUncommitted) {
  const GURL url1("http://a.test/");
  const GURL url2("http://b.test/");
  const GURL url3("http://c.test/");
  BtmPageVisitRecorder recorder(web_contents());

  // Navigate to url1 and commit.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  // Navigate to url2, but don't commit.
  auto nav = NavigationSimulator::CreateBrowserInitiated(url2, web_contents());
  nav->Start();
  nav->AbortCommit();
  // Navigate to url3 and commit.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url3);
  ASSERT_TRUE(recorder.WaitForSize(2));

  // Only url1 and url3 navigations are observed.
  EXPECT_EQ(recorder.visits()[0].navigation.destination.url, url1);
  EXPECT_EQ(recorder.visits()[1].prev_page.url, url1);
  EXPECT_EQ(recorder.visits()[1].navigation.destination.url, url3);
}

TEST_F(BtmPageVisitObserverTest, IgnoreSubframes) {
  const GURL url1("http://a.test/");
  const GURL url2("http://b.test/");
  const GURL url3("http://c.test/");
  BtmPageVisitRecorder recorder(web_contents());

  // Top-level navigation to url1.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  // Subframe navigation to url2.
  RenderFrameHost* iframe =
      RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("iframe");
  NavigationSimulator::NavigateAndCommitFromDocument(url2, iframe);
  // Top-level navigation to url3.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url3);
  ASSERT_TRUE(recorder.WaitForSize(2));

  // Only url1 and url3 navigations are observed.
  EXPECT_EQ(recorder.visits()[0].navigation.destination.url, url1);
  EXPECT_EQ(recorder.visits()[1].prev_page.url, url1);
  EXPECT_EQ(recorder.visits()[1].navigation.destination.url, url3);
}

// Same-document navigations are ignored.
TEST_F(BtmPageVisitObserverTest, IgnoreSameDocument) {
  const GURL url1a("http://a.test/");
  const GURL url1b("http://a.test/#top");
  const GURL url2("http://b.test/");
  BtmPageVisitRecorder recorder(web_contents());

  // Navigate to url1a
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1a);
  // Navigate to same-document url1b.
  auto nav = NavigationSimulator::CreateBrowserInitiated(url1b, web_contents());
  nav->Start();
  nav->CommitSameDocument();
  // Navigate to url2.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  ASSERT_TRUE(recorder.WaitForSize(2));

  // Only the url1a and url2 navigations are observed.
  EXPECT_EQ(recorder.visits()[0].navigation.destination.url, url1a);
  EXPECT_EQ(recorder.visits()[1].prev_page.url, url1a);
  EXPECT_EQ(recorder.visits()[1].navigation.destination.url, url2);
}

TEST_F(BtmPageVisitObserverTest, FlushPendingVisitsAtDestruction) {
  int counter = 0;

  {
    BtmPageVisitObserver observer(
        web_contents(),
        base::BindLambdaForTesting(
            [&counter](BtmPageVisitInfo, BtmNavigationInfo) { ++counter; }));
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                      GURL("http://a.test/"));
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                      GURL("http://b.test/"));
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                      GURL("http://c.test/"));
  }

  // If observer's pending visits isn't flushed, `counter` will (typically)
  // still be 0. But if they are, all three visits will be recorded.
  EXPECT_EQ(counter, 3);
}

}  // namespace content
