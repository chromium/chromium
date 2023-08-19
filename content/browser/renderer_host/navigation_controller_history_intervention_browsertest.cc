// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

namespace {

// Does a renderer-initiated location.replace navigation to |url|, replacing the
// current entry.
bool RendererLocationReplace(Shell* shell, const GURL& url) {
  WebContents* web_contents = shell->web_contents();
  WaitForLoadStop(web_contents);
  TestNavigationManager navigation_manager(web_contents, url);
  const GURL& current_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  EXPECT_TRUE(ExecJs(shell, JsReplace("window.location.replace($1)", url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
  // Observe pending entry if it's not a same-document navigation. We can't
  // observe same-document navigations because it might finish in the renderer,
  // only telling the browser side at the end.
  if (!current_url.EqualsIgnoringRef(url)) {
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(
        NavigationRequest::From(navigation_manager.GetNavigationHandle())
            ->common_params()
            .should_replace_current_entry);
  }
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL)) {
    return false;
  }
  return web_contents->GetLastCommittedURL() == url;
}

}  // namespace

class NavigationControllerHistoryInterventionBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string /* render_document_level */,
                     bool /* enable_back_forward_cache*/>> {
 public:
  NavigationControllerHistoryInterventionBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kQueueNavigationsWhileWaitingForCommit,
          {{"queueing_level", "full"}}}},
        {});
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    InitBackForwardCacheFeature(&feature_list_for_back_forward_cache_,
                                std::get<1>(GetParam()));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, enable_back_forward_cache] = info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled");
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kExposeInternalsForTesting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
};

// Tests that the navigation entry is marked as skippable on back/forward button
// if it does a renderer initiated navigation without ever getting a user
// activation.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       NoUserActivationSetSkipOnBackForward) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  EXPECT_TRUE(controller.CanGoBack());
  // Attempt to go back or forward to the skippable entry should log the
  // corresponding histogram and skip the corresponding entry.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Same as the above test except the navigation is cross-site in this case.
// Tests that the navigation entry is marked as skippable on back/forward button
// if it does a renderer initiated cross-site navigation without ever getting a
// user activation.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       NoUserActivationSetSkipOnBackForwardCrossSite) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new cross-site document from the renderer with a user
  // gesture.
  GURL redirected_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  EXPECT_TRUE(controller.CanGoBack());
  // Attempt to go back or forward to the skippable entry should log the
  // corresponding histogram and skip the corresponding entry.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Tests that when a navigation entry is not marked as skippable the first time
// it redirects because of user activation, that entry will be marked skippable
// if it does another redirect without user activation after the user has come
// back to that document again. This implies that a single user activation does
// not mean that the user can be infintely trapped.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       NoUserActivationAfterReturningSetsSkippable) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL initially_non_skippable_url(
      embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initially_non_skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Get a user activation and navigate to a new same-site document from the
  // renderer with a user gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last entry should have not been marked as skippable.
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Navigate back to the earlier document's entry.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(initially_non_skippable_url,
            controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable due to the lack of
  // activation on this visit, despite not being marked skippable last time.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Going back now should skip the entry at [1].
  ASSERT_TRUE(controller.CanGoBack());
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Tests that the navigation entry is marked as skippable on back button if it
// does a renderer initiated navigation without ever getting a user activation.
// Also tests this for an entry added using history.pushState.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       NoUserActivationSetSkippableMultipleGoBack) {
  GURL skippable_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  // Use the pushState API to add another entry without user gesture.
  GURL push_state_url(embedded_test_server()->GetURL("/title2.html"));
  std::string script("history.pushState('', '','" + push_state_url.spec() +
                     "');");
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), script, EXECUTE_SCRIPT_NO_USER_GESTURE));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last 2 entries should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // CanGoBack should return false since all previous entries are skippable.
  EXPECT_FALSE(controller.CanGoBack());
}

// Same as above but tests the metrics on going forward.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       NoUserActivationSetSkippableMultipleGoForward) {
  GURL skippable_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  // Use the pushState API to add another entry without user gesture.
  GURL push_state_url(embedded_test_server()->GetURL("/title2.html"));
  std::string script("history.pushState('', '','" + push_state_url.spec() +
                     "');");
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), script, EXECUTE_SCRIPT_NO_USER_GESTURE));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last 2 entries should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());
}

// Tests that if an entry is marked as skippable, it will not be reset if there
// is a navigation to this entry again (crbug.com/112129).
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       DoNotResetSkipOnBackForward) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Go back to the last entry.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  controller.GoToIndex(0);
  back_nav_load_observer.Wait();

  // Going back again to an entry should not reset its skippable flag.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());

  // Navigating away from this with a browser initiated navigation should log a
  // histogram with skippable as true.
  GURL url1(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
}

// Tests that if an entry is marked as skippable, it will not be reset if there
// is a navigation to this entry again (crbug.com/1121293) using history.back/
// forward.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       DoNotResetSkipOnHistoryBackAPI) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Go back to the last entry using history.back.
  EXPECT_TRUE(
      ExecJs(shell(), "history.back();", EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Going back again to an entry should not reset its skippable flag.
  EXPECT_TRUE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Tests that if a navigation entry is marked as skippable due to pushState then
// the flag should be reset if there is a user gesture on this document. All of
// the adjacent entries belonging to the same document will have their skippable
// bits reset.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       OnUserGestureResetSameDocumentEntriesSkipFlag) {
  GURL skippable_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Redirect to another page without a user gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));
  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());

  // Use the pushState API to add another entry without user gesture.
  GURL push_state_url1(embedded_test_server()->GetURL("/title1.html"));
  std::string script("history.pushState('', '','" + push_state_url1.spec() +
                     "');");
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), script, EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Use the pushState API to add another entry without user gesture.
  GURL push_state_url2(embedded_test_server()->GetURL("/title2.html"));
  script = "history.pushState('', '','" + push_state_url2.spec() + "');";
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), script, EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(3, controller.GetCurrentEntryIndex());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());

  // We now have
  // [skippable_url(skip), redirected_url(skip), push_state_url1(skip),
  // push_state_url2*]
  // Last 2 entries should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  EXPECT_EQ(skippable_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(redirected_url, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(push_state_url1, controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_EQ(push_state_url2, controller.GetEntryAtIndex(3)->GetURL());

  // Do another pushState so push_state_url2's entry also becomes skippable.
  GURL push_state_url3(embedded_test_server()->GetURL("/title3.html"));
  script = "history.pushState('', '','" + push_state_url3.spec() + "');";
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), script, EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  // We now have
  // [skippable_url(skip), redirected_url(skip), push_state_url1(skip),
  // push_state_url2(skip), push_state_url3*]

  // Go to index 2.
  TestNavigationObserver load_observer(shell()->web_contents());
  controller.GoToIndex(2);
  load_observer.Wait();
  EXPECT_EQ(push_state_url1, controller.GetLastCommittedEntry()->GetURL());

  // We now have (Before user gesture)
  // [skippable_url(skip), redirected_url(skip), push_state_url1(skip)*,
  // push_state_url2(skip), push_state_url3]
  // Note the entry at index 2 retains its skippable flag.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());

  // Simulate a user gesture. ExecJs internally also sends a user
  // gesture.
  script = "a=5";
  EXPECT_TRUE(content::ExecJs(shell()->web_contents(), script));

  // We now have (After user gesture)
  // [skippable_url(skip), redirected_url, push_state_url1*, push_state_url2,
  // push_state_url3]
  // All the navigations that refer to the same document should have their
  // skippable bit reset.
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());
  // The first entry is not the same document and its bit should not be reset.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());

  // goBack should now navigate to entry at index 1.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(redirected_url, controller.GetLastCommittedEntry()->GetURL());

  // Do another pushState without user gesture.
  GURL push_state_url4(embedded_test_server()->GetURL("/title3.html"));
  script = "history.pushState('', '','" + push_state_url3.spec() + "');";
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), script, EXECUTE_SCRIPT_NO_USER_GESTURE));
  // We now have
  // [skippable_url(skip), redirected_url, push_state_url4*]
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(skippable_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(redirected_url, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(push_state_url4, controller.GetEntryAtIndex(2)->GetURL());
  // The skippable flag will still be unset since this page has seen a user
  // gesture once.
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
}

// Tests that if a navigation entry is marked as skippable due to redirect to a
// new document then the flag should not be reset if there is a user gesture on
// the new document.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       OnUserGestureDoNotResetDifferentDocumentEntrySkipFlag) {
  GURL skippable_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Simulate a user gesture.
  root->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // Since the last navigations refer to a different document, a user gesture
  // here should not reset the skippable bit in the previous entries.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
}

// Tests that the navigation entry is not marked as skippable on back/forward
// button if it does a renderer initiated navigation after getting a user
// activation.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       UserActivationDoNotSkipOnBackForward) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer.
  // Note that NavigateToURLFromRenderer also simulates a user gesture.
  GURL user_gesture_redirected_url(
      embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), user_gesture_redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Last entry should not have been marked as skippable.
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Nothing should get skipped when back button is clicked.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  controller.GoBack();
  back_nav_load_observer.Wait();
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Tests that the navigation entry should not be marked as skippable on
// back/forward button if it is navigated away using a browser initiated
// navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       BrowserInitiatedNavigationDoNotSkipOnBackForward) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  GURL url(embedded_test_server()->GetURL("/title1.html"));

  // Note that NavigateToURL simulates a browser initiated navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Last entry should not have been marked as skippable.
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Nothing should get skipped when back button is clicked.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  controller.GoBack();
  back_nav_load_observer.Wait();
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Tests that the navigation entry that is marked as skippable on back/forward
// button does not get skipped for history.back API calls.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       SetSkipOnBackDoNotSkipForHistoryBackAPI) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // Attempt to go back to the skippable entry using the History API should
  // not skip the corresponding entry.
  TestNavigationObserver frame_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(root, "window.history.back()"));
  frame_observer.Wait();

  EXPECT_EQ(skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
}

#if BUILDFLAG(IS_ANDROID)
// Test GoToOffset with enable history intervention.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       GoToOffsetWithSkippingEnableHistoryIntervention) {
  base::HistogramTester histograms;
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  GURL skippable_url2(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url2));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url2(embedded_test_server()->GetURL("/title4.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url2));

  // CanGoToOffset should visit the skippable entries while
  // CanGoToOffsetWithSKipping will skip the skippable entries.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_TRUE(controller.CanGoToOffset(-3));
  EXPECT_TRUE(controller.CanGoToOffset(-4));
  EXPECT_FALSE(controller.CanGoToOffsetWithSkipping(-3));

  TestNavigationObserver nav_observer(shell()->web_contents());
  controller.GoToOffset(-4);
  nav_observer.Wait();
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Tests that the navigation entry that is marked as skippable on back/forward
// button does not get skipped for GoToOffset calls.
// This covers actions in the following scenario:
// [non_skippable_url, skippable_url, redirected_url, skippable_url2,
// redirected_url2]
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       SetSkipOnBackForwardDoNotSkipForGoToOffset) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  GURL skippable_url2(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url2));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url2(embedded_test_server()->GetURL("/title4.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url2));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());

  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());

  EXPECT_TRUE(controller.CanGoToOffset(-3));

  // GoToOffset should visit the skippable entries.
  TestNavigationObserver nav_observer1(shell()->web_contents());
  controller.GoToOffset(-1);
  nav_observer1.Wait();
  EXPECT_EQ(3, controller.GetCurrentEntryIndex());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(skippable_url2, controller.GetLastCommittedEntry()->GetURL());

  TestNavigationObserver nav_observer2(shell()->web_contents());
  controller.GoToOffset(1);
  nav_observer2.Wait();
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirected_url2, controller.GetLastCommittedEntry()->GetURL());

  TestNavigationObserver nav_observer3(shell()->web_contents());
  controller.GoToOffset(-4);
  nav_observer3.Wait();
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());

  EXPECT_TRUE(controller.CanGoToOffset(4));

  TestNavigationObserver nav_observer4(shell()->web_contents());
  controller.GoToOffset(4);
  nav_observer4.Wait();
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirected_url2, controller.GetLastCommittedEntry()->GetURL());
}

// Tests that the navigation entry that is marked as skippable on back/forward
// button is skipped for GoToOffset calls.
// This covers actions in the following scenario:
// [non_skippable_url, skippable_url, redirected_url, skippable_url2,
// redirected_url2]
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       SetSkipOnBackForwardDoSkipForGoToOffsetWithSkipping) {
#if BUILDFLAG(IS_ANDROID)
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  GURL skippable_url2(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url2));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url2(embedded_test_server()->GetURL("/title4.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url2));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());

  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());

  EXPECT_FALSE(controller.CanGoToOffsetWithSkipping(-3));
  EXPECT_TRUE(controller.CanGoToOffsetWithSkipping(-2));

  // GoToOffset should skip the skippable entries.
  TestNavigationObserver nav_observer1(shell()->web_contents());
  controller.GoToOffsetWithSkipping(-1);
  nav_observer1.Wait();
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirected_url, controller.GetLastCommittedEntry()->GetURL());

  TestNavigationObserver nav_observer2(shell()->web_contents());
  controller.GoToOffsetWithSkipping(1);
  nav_observer2.Wait();
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirected_url2, controller.GetLastCommittedEntry()->GetURL());

  TestNavigationObserver nav_observer3(shell()->web_contents());
  controller.GoToOffsetWithSkipping(-2);
  nav_observer3.Wait();
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());

  EXPECT_FALSE(controller.CanGoToOffsetWithSkipping(3));
  EXPECT_TRUE(controller.CanGoToOffsetWithSkipping(2));

  TestNavigationObserver nav_observer4(shell()->web_contents());
  controller.GoToOffsetWithSkipping(2);
  nav_observer4.Wait();
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirected_url2, controller.GetLastCommittedEntry()->GetURL());
#endif  // BUILDFLAG(IS_ANDROID)
}

// Tests that the navigation entry that is marked as skippable on back/forward
// button does not get skipped for history.forward API calls.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       SetSkipOnBackDoNotSkipForHistoryForwardAPI) {
  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Last entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  TestNavigationObserver nav_observer1(shell()->web_contents());
  controller.GoToIndex(0);
  nav_observer1.Wait();
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());

  // Attempt to go forward to the skippable entry using the History API should
  // not skip the corresponding entry.
  TestNavigationObserver nav_observer2(shell()->web_contents());
  EXPECT_TRUE(ExecJs(root, "window.history.forward()"));
  nav_observer2.Wait();

  EXPECT_EQ(skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
}

// Tests that the oldest navigation entry that is marked as skippable is the one
// that is pruned if max entry count is reached.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       PruneOldestSkippableEntry) {
  // Set the max entry count as 3.
  NavigationControllerImpl::set_max_entry_count_for_testing(3);

  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(non_skippable_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(skippable_url, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(redirected_url, controller.GetEntryAtIndex(2)->GetURL());

  // |skippable_url| entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());

  // A new navigation should lead to |skippable_url| to be pruned.
  GURL new_navigation_url(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_navigation_url));
  // Should still have 3 entries.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(non_skippable_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(redirected_url, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(new_navigation_url, controller.GetEntryAtIndex(2)->GetURL());
}

// Tests that we fallback to pruning the oldest entry if the last committed
// entry is the oldest skippable navigation entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       PruneOldestWhenLastCommittedIsSkippable) {
  // Set the max entry count as 2.
  NavigationControllerImpl::set_max_entry_count_for_testing(2);

  GURL non_skippable_url(
      embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Navigate to a new same-site document from the renderer without a user
  // gesture. This will mark |skippable_url| as skippable but since that is also
  // the last committed entry, it will not be pruned. Instead the oldest entry
  // will be removed.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(shell(), redirected_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(skippable_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(redirected_url, controller.GetEntryAtIndex(1)->GetURL());

  // |skippable_url| entry should have been marked as skippable.
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->should_skip_on_back_forward_ui());
}

// Tests that the navigation entry is marked as skippable on back/forward
// button if a subframe does a push state without ever getting a user
// activation.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       NoUserActivationSetSkipOnBackForwardSubframe) {
  GURL non_skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL skippable_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), skippable_url));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Invoke pushstate from a subframe.
  std::string script = "history.pushState({}, 'page 1', 'simple_page_1.html')";
  EXPECT_TRUE(
      ExecJs(root->child_at(0), script, EXECUTE_SCRIPT_NO_USER_GESTURE));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());

  EXPECT_TRUE(controller.CanGoBack());

  // Attempt to go back or forward to the skippable entry should log the
  // corresponding histogram and skip the corresponding entry.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(non_skippable_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // Go forward to the 3rd entry.
  TestNavigationObserver load_observer(shell()->web_contents());
  controller.GoToIndex(2);
  load_observer.Wait();

  // A user gesture in the main frame now will lead to all same document
  // entries to be marked as non-skippable.
  root->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(root->HasStickyUserActivation());
  EXPECT_TRUE(root->HasTransientUserActivation());
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
}

// Tests that the navigation entry is not marked as skippable on back/forward
// button if a subframe does a push state without ever getting a user
// activation on itself but there was a user gesture on the main frame.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerHistoryInterventionBrowserTest,
    UserActivationMainFrameDoNotSetSkipOnBackForwardSubframe) {
  GURL non_skippable_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), non_skippable_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL url_with_frames(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_with_frames));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Simulate user gesture in the main frame. Subframes creating entries without
  // user gesture will not lead to the last committed entry being marked as
  // skippable.
  root->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(root->HasStickyUserActivation());
  EXPECT_TRUE(root->HasTransientUserActivation());

  // Invoke pushstate from a subframe.
  std::string script = "history.pushState({}, 'page 1', 'simple_page_1.html')";
  EXPECT_TRUE(
      ExecJs(root->child_at(0), script, EXECUTE_SCRIPT_NO_USER_GESTURE));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
}

// Tests that all same document entries are marked as skippable together.
IN_PROC_BROWSER_TEST_P(NavigationControllerHistoryInterventionBrowserTest,
                       SetSkipOnBackForwardSameDocumentEntries) {
  // Consider the case:
  // 1. [Z, A, (click), A#1, A#2, A#3, A#4, B]
  // At this time all of A and A#1 through A#4 are non-skippable due to the
  // click.
  // 2. Let A#3 do a location.replace to another document
  // [Z, A, A#1, A#2, Y, A#4, B]
  // 3. Go to A#4, which is now the "current entry". All As are still
  // non-skippable.
  // 4. Let it now redirect without any user gesture to C.
  // [Z, A, A#1, A#2, Y, A#4, C]
  // At this time all of A entries should be marked as skippable.
  // 5. Go back should skip A's and go to Z.

  GURL z_url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), z_url));

  GURL a_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Add the 2 pushstate entries. Note that ExecJs also sends a user
  // gesture.
  GURL a1_url(embedded_test_server()->GetURL("/title2.html"));
  GURL a2_url(embedded_test_server()->GetURL("/title3.html"));
  GURL a3_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL a4_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  std::string script("history.pushState('', '','" + a1_url.spec() + "');");
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));
  script = "history.pushState('', '','" + a2_url.spec() + "');";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));
  script = "history.pushState('', '','" + a3_url.spec() + "');";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));
  script = "history.pushState('', '','" + a4_url.spec() + "');";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));

  EXPECT_TRUE(root->HasStickyUserActivation());
  EXPECT_TRUE(root->HasTransientUserActivation());

  // None of the entries should be skippable.
  EXPECT_EQ(6, controller.GetEntryCount());
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(5)->should_skip_on_back_forward_ui());

  // Navigate to B.
  GURL b_url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));

  // Go back to a3_url and do location.replace.
  {
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoToOffset(-2);
    load_observer.Wait();
  }
  EXPECT_EQ(a3_url, controller.GetLastCommittedEntry()->GetURL());
  GURL y_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  ASSERT_TRUE(RendererLocationReplace(shell(), y_url));

  EXPECT_EQ(7, controller.GetEntryCount());
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(5)->should_skip_on_back_forward_ui());
  EXPECT_FALSE(controller.GetEntryAtIndex(6)->should_skip_on_back_forward_ui());

  // Go forward to a4_url.
  {
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoForward();
    load_observer.Wait();
  }
  EXPECT_EQ(a4_url, controller.GetLastCommittedEntry()->GetURL());

  // Redirect without user gesture to C.
  GURL c_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), c_url));

  // All entries belonging to A should be marked skippable.
  EXPECT_EQ(7, controller.GetEntryCount());
  EXPECT_EQ(a_url, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_TRUE(controller.GetEntryAtIndex(1)->should_skip_on_back_forward_ui());

  EXPECT_EQ(a1_url, controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_TRUE(controller.GetEntryAtIndex(2)->should_skip_on_back_forward_ui());

  EXPECT_EQ(a2_url, controller.GetEntryAtIndex(3)->GetURL());
  EXPECT_TRUE(controller.GetEntryAtIndex(3)->should_skip_on_back_forward_ui());

  EXPECT_EQ(y_url, controller.GetEntryAtIndex(4)->GetURL());
  EXPECT_FALSE(controller.GetEntryAtIndex(4)->should_skip_on_back_forward_ui());

  EXPECT_EQ(a4_url, controller.GetEntryAtIndex(5)->GetURL());
  EXPECT_TRUE(controller.GetEntryAtIndex(5)->should_skip_on_back_forward_ui());

  EXPECT_EQ(c_url, controller.GetEntryAtIndex(6)->GetURL());
  EXPECT_FALSE(controller.GetEntryAtIndex(6)->should_skip_on_back_forward_ui());

  // Go back should skip all A entries and go to Y.
  {
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoBack();
    load_observer.Wait();
  }
  EXPECT_EQ(y_url, controller.GetLastCommittedEntry()->GetURL());

  // Going back again should skip all A entries and go to Z.
  {
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoBack();
    load_observer.Wait();
  }
  EXPECT_EQ(z_url, controller.GetLastCommittedEntry()->GetURL());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationControllerHistoryInterventionBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerHistoryInterventionBrowserTest::DescribeParams);

}  // namespace content
