// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/portal/portal.h"
#include "content/browser/portal/portal_navigation_throttle.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/portal/portal_activated_observer.h"
#include "content/test/portal/portal_created_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {
namespace {

// TODO(jbroman): Perhaps this would be a useful utility generally.
GURL GetServerRedirectURL(const net::EmbeddedTestServer* server,
                          const std::string& hostname,
                          const GURL& destination) {
  return server->GetURL(
      hostname,
      "/server-redirect?" +
          base::EscapeQueryParamValue(destination.spec(), /*use_plus=*/false));
}

class PortalNavigationThrottleBrowserTest : public ContentBrowserTest {
 protected:
  virtual bool ShouldEnableCrossOriginPortals() const { return false; }

  PortalNavigationThrottleBrowserTest() {
    if (ShouldEnableCrossOriginPortals()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{blink::features::kPortals,
                                blink::features::kPortalsCrossOrigin},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{blink::features::kPortals},
          /*disabled_features=*/{blink::features::kPortalsCrossOrigin});
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/notreached",
        base::BindRepeating(
            [](const net::test_server::HttpRequest& r)
                -> std::unique_ptr<net::test_server::HttpResponse> {
              ADD_FAILURE() << "/notreached was requested";
              return nullptr;
            })));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* GetWebContents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
  RenderFrameHostImpl* GetPrimaryMainFrame() {
    return GetWebContents()->GetPrimaryMainFrame();
  }

  Portal* InsertAndWaitForPortal(const GURL& url,
                                 bool expected_to_succeed = true) {
    TestNavigationObserver navigation_observer(/*web_contents=*/nullptr, 1);
    navigation_observer.StartWatchingNewWebContents();
    PortalCreatedObserver portal_created_observer(GetPrimaryMainFrame());
    EXPECT_TRUE(ExecJs(
        GetPrimaryMainFrame(),
        base::StringPrintf("var portal = document.createElement('portal');\n"
                           "portal.src = '%s';\n"
                           "document.body.appendChild(portal);",
                           url.spec().c_str())));
    Portal* portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.StopWatchingNewWebContents();
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_succeeded(),
              expected_to_succeed);
    if (expected_to_succeed)
      EXPECT_EQ(navigation_observer.last_navigation_url(), url);
    return portal;
  }

  bool NavigatePortalViaSrcAttribute(Portal* portal,
                                     const GURL& url,
                                     int number_of_navigations) {
    TestNavigationObserver navigation_observer(portal->GetPortalContents(),
                                               number_of_navigations);
    EXPECT_TRUE(
        ExecJs(GetPrimaryMainFrame(),
               base::StringPrintf(
                   "document.querySelector('body > portal').src = '%s';",
                   url.spec().c_str())));
    navigation_observer.Wait();
    return navigation_observer.last_navigation_succeeded();
  }

  bool NavigatePortalViaLocationHref(Portal* portal,
                                     const GURL& url,
                                     int number_of_navigations) {
    TestNavigationObserver navigation_observer(portal->GetPortalContents(),
                                               number_of_navigations);
    EXPECT_TRUE(ExecJs(
        portal->GetPortalContents(),
        base::StringPrintf("location.href = '%s';", url.spec().c_str())));
    navigation_observer.Wait();
    return navigation_observer.last_navigation_succeeded();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginInitialNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));
  EXPECT_NE(portal, nullptr);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginInitialNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("not.portal.test", "/title2.html"),
      /*expected_to_succeed=*/false);
  EXPECT_NE(portal, nullptr);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  EXPECT_TRUE(NavigatePortalViaSrcAttribute(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginNavigationTriggeredByPortal) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  EXPECT_TRUE(NavigatePortalViaLocationHref(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       SameOriginNavigationWithServerRedirect) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  GURL redirect_url = GetServerRedirectURL(embedded_test_server(),
                                           "portal.test", destination_url);
  EXPECT_TRUE(NavigatePortalViaSrcAttribute(portal, redirect_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(),
            destination_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  GURL referrer_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");

  Portal* portal = InsertAndWaitForPortal(referrer_url);
  EXPECT_FALSE(NavigatePortalViaSrcAttribute(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigationTriggeredByPortal) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  GURL referrer_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");

  Portal* portal = InsertAndWaitForPortal(referrer_url);
  EXPECT_FALSE(NavigatePortalViaLocationHref(portal, destination_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigationViaRedirect) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  GURL referrer_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");
  GURL redirect_url = GetServerRedirectURL(embedded_test_server(),
                                           "portal.test", destination_url);

  Portal* portal = InsertAndWaitForPortal(referrer_url);
  EXPECT_FALSE(NavigatePortalViaSrcAttribute(portal, redirect_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginRedirectLeadingBack) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));

  GURL referrer_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  GURL destination_url =
      embedded_test_server()->GetURL("portal.test", "/notreached");
  GURL redirect_url = GetServerRedirectURL(embedded_test_server(),
                                           "not.portal.test", destination_url);

  Portal* portal = InsertAndWaitForPortal(referrer_url);
  EXPECT_FALSE(NavigatePortalViaSrcAttribute(portal, redirect_url, 1));
  EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       ActivateAfterCanceledInitialNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  GURL referrer_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  GURL destination_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");

  Portal* portal =
      InsertAndWaitForPortal(destination_url, /*expected_to_succeed=*/false);
  EXPECT_NE(portal, nullptr);

  std::string result =
      EvalJs(GetPrimaryMainFrame(),
             "document.querySelector('body > portal').activate()"
             ".then(() => 'activated', e => e.message)")
          .ExtractString();
  EXPECT_THAT(result, ::testing::HasSubstr("not yet ready or was blocked"));
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(),
            embedded_test_server()->GetURL("portal.test", "/title1.html"));
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       LogsConsoleWarning) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));

  WebContentsConsoleObserver console_observer(GetWebContents());
  console_observer.SetPattern("*portal*cross-origin*");

  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("not.portal.test", "/title2.html"),
      /*expected_to_succeed=*/false);
  EXPECT_NE(portal, nullptr);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_THAT(console_observer.GetMessageAt(0u),
              ::testing::HasSubstr("http://not.portal.test"));
}

// Ensure navigating while a portal is orphaned does not bypass cross-origin
// restrictions.
IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTest,
                       CrossOriginNavigationWhileOrphaned) {
  WebContentsImpl* predecessor_contents = GetWebContents();
  GURL predecessor_url =
      embedded_test_server()->GetURL("portal.test", "/title1.html");
  GURL orphan_navigation_url =
      embedded_test_server()->GetURL("not.portal.test", "/notreached");
  ASSERT_TRUE(NavigateToURL(predecessor_contents, predecessor_url));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  // We want the predecessor's navigation to occur before adoption, so we have
  // the successor hang to keep the predecessor in the orphaned state.
  EXPECT_TRUE(ExecJs(portal->GetPortalContents(),
                     "window.addEventListener('portalactivate', (e) => {"
                     "  while (true);"
                     "});"));

  TestNavigationObserver navigation_observer(predecessor_contents);
  EXPECT_TRUE(ExecJs(predecessor_contents,
                     JsReplace("document.querySelector('portal').activate();"
                               "location.href = $1;",
                               orphan_navigation_url)));
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(predecessor_contents->GetLastCommittedURL(), predecessor_url);
}

class PortalNavigationThrottleBrowserTestCrossOrigin
    : public PortalNavigationThrottleBrowserTest {
 protected:
  bool ShouldEnableCrossOriginPortals() const override { return true; }
};

void SleepWithRunLoop(base::TimeDelta delay, base::Location from_here) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      from_here, run_loop.QuitClosure(), delay);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleBrowserTestCrossOrigin,
                       NonHTTPSchemesBlockedEvenInCrossOriginMode) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  GURL referrer_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  Portal* portal = InsertAndWaitForPortal(referrer_url);

  // Can't use NavigatePortalViaLocationHref as the checks for data: URLs are
  // duplicated between Blink and content/browser/, and the former check aborts
  // before a navigation even begins.
  //
  // This is also why we use sleeps to watch for the navigation occurring.
  // Fortunately, because the sleep only races in the failure case this test
  // should only be flaky if there is a bug.

  {
    WebContentsConsoleObserver console_observer(portal->GetPortalContents());
    console_observer.SetPattern("*avigat*");
    EXPECT_TRUE(ExecJs(portal->GetPortalContents(),
                       "location.href = 'data:text/html,hello world';"));
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_THAT(console_observer.GetMessageAt(0u),
                ::testing::HasSubstr("data"));
    SleepWithRunLoop(base::Seconds(3), FROM_HERE);
    EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
  }

  {
    WebContentsConsoleObserver console_observer(GetWebContents());
    console_observer.SetPattern("*avigat*");
    EXPECT_TRUE(ExecJs(portal->GetPortalContents(),
                       "location.href = 'ftp://example.com/';"));
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_THAT(console_observer.GetMessageAt(0u), ::testing::HasSubstr("ftp"));
    SleepWithRunLoop(base::Seconds(3), FROM_HERE);
    EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
  }

  {
    // Credentialed subresource requests are blocked no matter what scheme is
    // used.
    WebContentsConsoleObserver console_observer(GetWebContents());
    console_observer.SetPattern(
        "*Subresource requests whose URLs contain embedded credentials (e.g. "
        "`https://user:pass@host/`) are blocked.*");
    EXPECT_TRUE(ExecJs(portal->GetPortalContents(),
                       "location.href = 'ftp://user:pass@example.com/';"));
    ASSERT_TRUE(console_observer.Wait());
    SleepWithRunLoop(base::Seconds(3), FROM_HERE);
    EXPECT_EQ(portal->GetPortalContents()->GetLastCommittedURL(), referrer_url);
  }
}

class PortalNavigationThrottleFencedFrameBrowserTest
    : public PortalNavigationThrottleBrowserTest {
 public:
  PortalNavigationThrottleFencedFrameBrowserTest() = default;
  ~PortalNavigationThrottleFencedFrameBrowserTest() override = default;
  PortalNavigationThrottleFencedFrameBrowserTest(
      const PortalNavigationThrottleFencedFrameBrowserTest&) = delete;

  PortalNavigationThrottleFencedFrameBrowserTest& operator=(
      const PortalNavigationThrottleFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PortalNavigationThrottleFencedFrameBrowserTest,
                       SameOriginInitialNavigation) {
  ASSERT_TRUE(NavigateToURL(
      GetWebContents(),
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = InsertAndWaitForPortal(
      embedded_test_server()->GetURL("portal.test", "/title2.html"));
  EXPECT_NE(portal, nullptr);

  // Create a fenced frame.
  GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_host(
      fenced_frame_test_helper().CreateFencedFrame(
          portal->GetPortalContents()->GetPrimaryMainFrame(),
          fenced_frame_url));

  // A fenced frame's FrameTree embedded inside a portal is not considered to be
  // portal frame tree.
  FrameTreeNode* fenced_frame_root_node = fenced_frame_host->frame_tree_node();
  EXPECT_FALSE(fenced_frame_root_node->frame_tree().IsPortal());
}

}  // namespace
}  // namespace content
