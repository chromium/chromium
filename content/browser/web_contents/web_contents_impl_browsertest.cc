// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"

#include <array>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/resource_load_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "partition_alloc/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace content {

#define SCOPE_TRACED(statement) \
  {                             \
    SCOPED_TRACE(#statement);   \
    statement;                  \
  }

void ResizeWebContentsView(Shell* shell,
                           const gfx::Size& size,
                           bool set_start_page) {
  // Resizing the web content directly, independent of the Shell window,
  // requires the RenderWidgetHostView to exist. So we do a navigation
  // first if |set_start_page| is true.
  if (set_start_page)
    EXPECT_TRUE(NavigateToURL(shell, GURL(url::kAboutBlankURL)));

  shell->ResizeWebContentForTests(size);
}

class WebContentsImplBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImplBrowserTest() = default;
  void SetUp() override {
    RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
    ContentBrowserTest::SetUp();
  }

  WebContentsImplBrowserTest(const WebContentsImplBrowserTest&) = delete;
  WebContentsImplBrowserTest& operator=(const WebContentsImplBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsInFullscreen() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return !!web_contents->current_fullscreen_frame_id_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Starts a new navigation as soon as the current one commits, but does not
// wait for it to complete.  This allows us to observe DidStopLoading while
// a pending entry is present.
class NavigateOnCommitObserver : public WebContentsObserver {
 public:
  NavigateOnCommitObserver(Shell* shell, GURL url)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        url_(url),
        done_(false) {}

  // WebContentsObserver:
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    if (!done_) {
      done_ = true;
      shell_->LoadURL(url_);

      // There should be a pending entry.
      CHECK(shell_->web_contents()->GetController().GetPendingEntry());

      // Now that there is a pending entry, stop the load.
      shell_->Stop();
    }
  }

  raw_ptr<Shell> shell_;
  GURL url_;
  bool done_;
};

class RenderViewSizeDelegate : public WebContentsDelegate {
 public:
  void set_size_insets(const gfx::Size& size_insets) {
    size_insets_ = size_insets;
  }

  // WebContentsDelegate:
  gfx::Size GetSizeForNewRenderView(WebContents* web_contents) override {
    gfx::Size size(web_contents->GetContainerBounds().size());
    size.Enlarge(size_insets_.width(), size_insets_.height());
    return size;
  }

 private:
  gfx::Size size_insets_;
};

class RenderViewSizeObserver : public WebContentsObserver {
 public:
  RenderViewSizeObserver(Shell* shell, const gfx::Size& wcv_new_size)
      : WebContentsObserver(shell->web_contents()),
        shell_(shell),
        wcv_new_size_(wcv_new_size) {}

  // WebContentsObserver:
  void RenderFrameCreated(RenderFrameHost* rfh) override {
    if (!rfh->GetParent())
      rwhv_create_size_ = rfh->GetView()->GetViewBounds().size();
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    ResizeWebContentsView(shell_, wcv_new_size_, false);
  }

  gfx::Size rwhv_create_size() const { return rwhv_create_size_; }

 private:
  raw_ptr<Shell> shell_;  // Weak ptr.
  gfx::Size wcv_new_size_;
  gfx::Size rwhv_create_size_;
};

class LoadingStateChangedDelegate : public WebContentsDelegate {
 public:
  LoadingStateChangedDelegate() = default;

  // WebContentsDelegate:
  void LoadingStateChanged(WebContents* contents,
                           bool should_show_loading_ui) override {
    loadingStateChangedCount_++;
    if (should_show_loading_ui)
      loadingStateShowLoadingUICount_++;
  }

  int loadingStateChangedCount() const { return loadingStateChangedCount_; }
  int loadingStateShowLoadingUICount() const {
    return loadingStateShowLoadingUICount_;
  }

 private:
  int loadingStateChangedCount_ = 0;
  int loadingStateShowLoadingUICount_ = 0;
};

// Regression test for https://crbug.com/1405036
// Dumping the accessibility tree should not crash, even if it has not received
// an ID through a renderer tree yet.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DumpAccessibilityTreeWithoutTreeID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  load_observer.Wait();
  std::string expected = "-";

  std::vector<ui::AXPropertyFilter> property_filters;
  EXPECT_EQ(
      shell()->web_contents()->DumpAccessibilityTree(false, property_filters),
      expected);
}

namespace {

const char kFrameCountUMA[] = "Navigation.MainFrame.FrameCount";
const char kMaxFrameCountUMA[] = "Navigation.MainFrame.MaxFrameCount";

// Class that waits for a particular load to finish in any frame.  This happens
// after the commit event.
class LoadFinishedWaiter : public WebContentsObserver {
 public:
  LoadFinishedWaiter(WebContents* web_contents, const GURL& expected_url)
      : WebContentsObserver(web_contents),
        expected_url_(expected_url),
        run_loop_(new base::RunLoop()) {
    EXPECT_TRUE(web_contents != nullptr);
  }

  void Wait() { run_loop_->Run(); }

 private:
  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    if (url == expected_url_)
      run_loop_->Quit();
  }

  GURL expected_url_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

// Ensure that cross-site subframes always notify their parents when they finish
// loading, so that the page eventually reaches DidStopLoading.  There was a bug
// where an OOPIF would not notify its parent if (1) it finished loading, but
// (2) later added a subframe that kept the main frame in the loading state, and
// (3) all subframes then finished loading.
// Note that this test makes sense to run with and without OOPIFs.
// See https://crbug.com/822013#c12.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidStopLoadingWithNestedFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to an A(B, C) page where B is slow to load.  Wait for C to reach
  // load stop.  A will still be loading due to B.
  GURL url_a = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  GURL url_b = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b()");
  GURL url_c = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c()");
  TestNavigationManager delayer_b(web_contents, url_b);
  LoadFinishedWaiter load_waiter_c(web_contents, url_c);
  shell()->LoadURL(url_a);
  EXPECT_TRUE(delayer_b.WaitForRequestStart());
  load_waiter_c.Wait();
  EXPECT_TRUE(web_contents->IsLoading());

  // At this point, C has finished loading and B is stalled.  Add a slow D frame
  // within C.
  GURL url_d = embedded_test_server()->GetURL("d.com", "/title1.html");
  FrameTreeNode* subframe_c =
      web_contents->GetPrimaryFrameTree().root()->child_at(1);
  EXPECT_EQ(url_c, subframe_c->current_url());
  TestNavigationManager delayer_d(web_contents, url_d);
  const std::string add_d_script = base::StringPrintf(
      "var f = document.createElement('iframe');"
      "f.src='%s';"
      "document.body.appendChild(f);",
      url_d.spec().c_str());
  EXPECT_TRUE(ExecJs(subframe_c, add_d_script));
  EXPECT_TRUE(delayer_d.WaitForRequestStart());
  EXPECT_TRUE(web_contents->IsLoading());

  // Let B finish and wait for another load stop.  A will still be loading due
  // to D.
  LoadFinishedWaiter load_waiter_b(web_contents, url_b);
  ASSERT_TRUE(delayer_b.WaitForNavigationFinished());
  load_waiter_b.Wait();
  EXPECT_TRUE(web_contents->IsLoading());

  // Let D finish.  We should get a load stop in the main frame.
  LoadFinishedWaiter load_waiter_d(web_contents, url_d);
  ASSERT_TRUE(delayer_d.WaitForNavigationFinished());
  load_waiter_d.Wait();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_FALSE(web_contents->IsLoading());
}

// Test that a renderer-initiated navigation to an invalid URL does not leave
// around a pending entry that could be used in a URL spoof.  We test this in
// a browser test because our unit test framework incorrectly calls
// DidStartProvisionalLoadForFrame for in-page navigations.
// See http://crbug.com/280512.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ClearNonVisiblePendingOnFail) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Navigate to an invalid URL and make sure it doesn't leave a pending entry.
  LoadStopObserver load_observer1(shell()->web_contents());
  ASSERT_TRUE(ExecJs(shell(), "window.location.href=\"nonexistent:12121\";"));
  load_observer1.Wait();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

  LoadStopObserver load_observer2(shell()->web_contents());
  ASSERT_TRUE(ExecJs(shell(), "window.location.href=\"#foo\";"));
  load_observer2.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html#foo"),
            shell()->web_contents()->GetVisibleURL());
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || defined(THREAD_SANITIZER)
#define MAYBE_GetSizeForNewRenderView DISABLED_GetSizeForNewRenderView
#else
#define MAYBE_GetSizeForNewRenderView DISABLED_GetSizeForNewRenderView
#endif
// Test that RenderViewHost is created and updated at the size specified by
// WebContentsDelegate::GetSizeForNewRenderView().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_GetSizeForNewRenderView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create a new server with a different site.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  std::unique_ptr<RenderViewSizeDelegate> delegate(
      new RenderViewSizeDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  // When no size is set, RenderWidgetHostView adopts the size of
  // WebContentsView.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  EXPECT_EQ(shell()->web_contents()->GetContainerBounds().size(),
            shell()
                ->web_contents()
                ->GetRenderWidgetHostView()
                ->GetViewBounds()
                .size());

  // When a size is set, RenderWidgetHostView and WebContentsView honor this
  // size.
  gfx::Size size(300, 300);
  gfx::Size size_insets(10, 15);
  ResizeWebContentsView(shell(), size, true);
  delegate->set_size_insets(size_insets);
  EXPECT_TRUE(NavigateToURL(shell(), https_server.GetURL("/")));
  size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(size, shell()
                      ->web_contents()
                      ->GetRenderWidgetHostView()
                      ->GetViewBounds()
                      .size());
  // The web_contents size is set by the embedder, and should not depend on the
  // rwhv size. The behavior is correct on OSX, but incorrect on other
  // platforms.
  gfx::Size exp_wcv_size(300, 300);
#if !BUILDFLAG(IS_MAC)
  exp_wcv_size.Enlarge(size_insets.width(), size_insets.height());
#endif

  EXPECT_EQ(exp_wcv_size, shell()->web_contents()->GetContainerBounds().size());

  // If WebContentsView is resized after RenderWidgetHostView is created but
  // before pending navigation entry is committed, both RenderWidgetHostView and
  // WebContentsView use the new size of WebContentsView.
  gfx::Size init_size(200, 200);
  gfx::Size new_size(100, 100);
  size_insets = gfx::Size(20, 30);
  ResizeWebContentsView(shell(), init_size, true);
  delegate->set_size_insets(size_insets);
  RenderViewSizeObserver observer(shell(), new_size);
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  // RenderWidgetHostView is created at specified size.
  init_size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(init_size, observer.rwhv_create_size());

// Once again, the behavior is correct on OSX. The embedder explicitly sets
// the size to (100,100) during navigation. Both the wcv and the rwhv should
// take on that size.
#if !BUILDFLAG(IS_MAC)
  new_size.Enlarge(size_insets.width(), size_insets.height());
#endif
  gfx::Size actual_size = shell()
                              ->web_contents()
                              ->GetRenderWidgetHostView()
                              ->GetViewBounds()
                              .size();

  EXPECT_EQ(new_size, actual_size);
  EXPECT_EQ(new_size, shell()->web_contents()->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, SetTitleOnPagehide) {
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL url(
      "data:text/html,"
      "<title>A</title>"
      "<body onpagehide=\"document.title = 'B'\"></body>");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ(1, shell()->web_contents()->GetController().GetEntryCount());
  NavigationEntryImpl* entry1 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance1 = entry1->site_instance();
  EXPECT_EQ(u"A", entry1->GetTitle());

  // Force a process switch by going to a privileged page.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));

  RenderFrameHostImplWrapper rfh(
      shell()->web_contents()->GetPrimaryMainFrame());
  rfh->DisableUnloadTimerForTesting();
  ASSERT_TRUE(NavigateToURL(shell(), web_ui_page));

  // Wait for the page with unload to be deleted.
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());

  // Verify that the site instance changed.
  NavigationEntryImpl* entry2 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance2 = entry2->site_instance();
  EXPECT_NE(site_instance1, site_instance2);

  // Verify that the title changed.
  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(u"B", entry1->GetTitle());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, OpenURLSubframe) {
  // Navigate to a page with frames and grab a subframe's FrameTreeNode ID.
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();
  ASSERT_EQ(3UL, root->child_count());
  FrameTreeNodeId frame_tree_node_id = root->child_at(0)->frame_tree_node_id();
  EXPECT_TRUE(frame_tree_node_id);

  // Navigate with the subframe's FrameTreeNode ID.
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  OpenURLParams params(url, Referrer(), frame_tree_node_id,
                       WindowOpenDisposition::CURRENT_TAB,
                       ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin = wc->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  shell()->web_contents()->OpenURL(params, /*navigation_handle_callback=*/{});

  // Make sure the NavigationEntry ends up with the FrameTreeNode ID.
  NavigationController* controller = &shell()->web_contents()->GetController();
  EXPECT_TRUE(controller->GetPendingEntry());
  EXPECT_EQ(frame_tree_node_id, NavigationEntryImpl::FromNavigationEntry(
                                    controller->GetPendingEntry())
                                    ->frame_tree_node_id());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, OpenURLNonExistentSubframe) {
  // Navigate to a page with frames and grab a subframe's FrameTreeNode ID.
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());

  // Take a FrameTreeNodeID that doesn't represent any frames.
  FrameTreeNodeId frame_tree_node_id = FrameTreeNodeId(100);
  ASSERT_FALSE(FrameTreeNode::GloballyFindByID(frame_tree_node_id));

  // Navigate with the invalid FrameTreeNode ID.
  const GURL url(embedded_test_server()->GetURL("/title2.html"));
  OpenURLParams params(url, Referrer(), frame_tree_node_id,
                       WindowOpenDisposition::CURRENT_TAB,
                       ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin = wc->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  WebContents* new_web_contents = shell()->web_contents()->OpenURL(
      params, /*navigation_handle_callback=*/{});

  // The navigation should have been ignored.
  EXPECT_EQ(new_web_contents, nullptr);
  NavigationController* controller = &shell()->web_contents()->GetController();
  EXPECT_EQ(controller->GetPendingEntry(), nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       AppendingFrameInWebUIDoesNotCrash) {
  const GURL kWebUIUrl(GetWebUIURL("gpu"));
  const char kJSCodeForAppendingFrame[] =
      "document.body.appendChild(document.createElement('iframe'));";

  EXPECT_TRUE(NavigateToURL(shell(), kWebUIUrl));

  EXPECT_TRUE(content::ExecJs(shell(), kJSCodeForAppendingFrame));
}

// Test that creation of new RenderFrameHost objects sends the correct object
// to the WebContentObservers. See http://crbug.com/347339.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderFrameCreatedCorrectProcessForObservers) {
  static const char kFooCom[] = "foo.com";
  GURL::Replacements replace_host;
  net::HostPortPair foo_host_port;
  GURL cross_site_url;

  ASSERT_TRUE(embedded_test_server()->Start());

  foo_host_port = embedded_test_server()->host_port_pair();
  foo_host_port.set_host(kFooCom);

  GURL initial_url(embedded_test_server()->GetURL("/title1.html"));

  cross_site_url = embedded_test_server()->GetURL("/title2.html");
  replace_host.SetHostStr(kFooCom);
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);

  // Navigate to the initial URL and capture the RenderFrameHost for later
  // comparison.
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  RenderFrameHost* orig_rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Install the observer and navigate cross-site.
  RenderFrameHostCreatedObserver observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));

  // The observer should've seen a RenderFrameCreated call for the new frame
  // and not the old one.
  EXPECT_NE(observer.last_rfh(), orig_rfh);
  EXPECT_EQ(observer.last_rfh(),
            shell()->web_contents()->GetPrimaryMainFrame());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingStateChangedForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<LoadingStateChangedDelegate> delegate(
      new LoadingStateChangedDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());

  LoadStopObserver load_observer(shell()->web_contents());
  TitleWatcher title_watcher(shell()->web_contents(), u"pushState");
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/push_state.html")));
  load_observer.Wait();
  std::u16string title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, u"pushState");

  // LoadingStateChanged should be called 4 times: start and stop for the
  // initial load of push_state.html, and start and stop for
  // the "navigation" triggered by history.pushState(). However, the start
  // notification for the history.pushState() navigation should set
  // should_show_loading_ui to false, as should all stop notifications.
  EXPECT_EQ("pushState", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_EQ(4, delegate->loadingStateChangedCount());
  EXPECT_EQ(1, delegate->loadingStateShowLoadingUICount());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ResourceLoadComplete) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());
  // Load a page with an image and an image.
  GURL page_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  base::TimeTicks before = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  base::TimeTicks after = base::TimeTicks::Now();
  ASSERT_EQ(3U, observer.resource_load_entries().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET",
      network::mojom::RequestDestination::kDocument,
      FILE_PATH_LITERAL("page_with_iframe.html"), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/true, before, after));
  SCOPE_TRACED(observer.CheckResourceLoaded(
      embedded_test_server()->GetURL("/image.jpg"),
      /*referrer=*/page_url, "GET", network::mojom::RequestDestination::kImage,
      FILE_PATH_LITERAL("image.jpg"), "image/jpeg", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
  SCOPE_TRACED(observer.CheckResourceLoaded(
      embedded_test_server()->GetURL("/title1.html"),
      /*referrer=*/page_url, "GET", network::mojom::RequestDestination::kIframe,
      FILE_PATH_LITERAL("title1.html"), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
}

// Same as WebContentsImplBrowserTest.ResourceLoadComplete but with resources
// retrieved from the network cache.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteFromNetworkCache) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page_url(
      embedded_test_server()->GetURL("/page_with_cached_subresource.html"));
  base::TimeTicks before = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  base::TimeTicks after = base::TimeTicks::Now();

  GURL resource_url = embedded_test_server()->GetURL("/cachetime");
  ASSERT_EQ(2U, observer.resource_load_entries().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET",
      network::mojom::RequestDestination::kDocument,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false,
      /*first_network_request=*/true, before, after));

  SCOPE_TRACED(observer.CheckResourceLoaded(
      resource_url, /*referrer=*/page_url, "GET",
      network::mojom::RequestDestination::kScript,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
  EXPECT_TRUE(observer.resource_load_entries()[1]
                  .resource_load_info->network_info->network_accessed);
  EXPECT_TRUE(observer.memory_cached_loaded_urls().empty());
  observer.Reset();

  // Loading again should serve the request out of the in-memory cache.
  before = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  after = base::TimeTicks::Now();
  ASSERT_EQ(1U, observer.resource_load_entries().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET",
      network::mojom::RequestDestination::kDocument,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
  ASSERT_EQ(1U, observer.memory_cached_loaded_urls().size());
  EXPECT_EQ(resource_url, observer.memory_cached_loaded_urls()[0]);
  observer.Reset();

  // Kill the renderer process so when the navigate again, it will be a fresh
  // renderer with an empty in-memory cache.
  ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
  EXPECT_FALSE(NavigateToURL(shell(), GetWebUIURL("crash")));

  // Reload that URL, the subresource should be served from the network cache.
  before = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  after = base::TimeTicks::Now();
  ASSERT_EQ(2U, observer.resource_load_entries().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET",
      network::mojom::RequestDestination::kDocument,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/true, before, after));
  SCOPE_TRACED(observer.CheckResourceLoaded(
      resource_url, /*referrer=*/page_url, "GET",
      network::mojom::RequestDestination::kScript,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/true, /*first_network_request=*/false, before, after));
  EXPECT_TRUE(observer.memory_cached_loaded_urls().empty());
  EXPECT_FALSE(observer.resource_load_entries()[1]
                   .resource_load_info->network_info->network_accessed);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteFromLocalResource) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL(embedded_test_server()->GetURL("/page_with_image.html"))));
  ASSERT_EQ(2U, observer.resource_load_entries().size());
  EXPECT_TRUE(observer.resource_load_entries()[0]
                  .resource_load_info->network_info->network_accessed);
  EXPECT_TRUE(observer.resource_load_entries()[1]
                  .resource_load_info->network_info->network_accessed);
  observer.Reset();

  EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("gpu")));
  ASSERT_LE(1U, observer.resource_load_entries().size());
  for (auto& resource_load_entry : observer.resource_load_entries()) {
    EXPECT_FALSE(
        resource_load_entry.resource_load_info->network_info->network_accessed);
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteWithRedirect) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL page_destination_url(
      embedded_test_server()->GetURL("/page_with_image_redirect.html"));
  GURL page_original_url(embedded_test_server()->GetURL(
      "/server-redirect?" + page_destination_url.spec()));
  EXPECT_TRUE(NavigateToURL(shell(), page_original_url,
                            page_destination_url /* expected_commit_url */));

  ASSERT_EQ(2U, observer.resource_load_entries().size());
  const blink::mojom::ResourceLoadInfoPtr& page_load_info =
      observer.resource_load_entries()[0].resource_load_info;
  EXPECT_EQ(page_destination_url, page_load_info->final_url);
  EXPECT_EQ(page_original_url, page_load_info->original_url);

  GURL image_destination_url(embedded_test_server()->GetURL("/blank.jpg"));
  GURL image_original_url(
      embedded_test_server()->GetURL("/server-redirect?blank.jpg"));
  const blink::mojom::ResourceLoadInfoPtr& image_load_info =
      observer.resource_load_entries()[1].resource_load_info;
  EXPECT_EQ(image_destination_url, image_load_info->final_url);
  EXPECT_EQ(image_original_url, image_load_info->original_url);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteNetError) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL page_url(embedded_test_server()->GetURL("/page_with_image.html"));
  GURL image_url(embedded_test_server()->GetURL("/blank.jpg"));

  // Load the page without errors.
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  const std::vector<ResourceLoadObserver::ResourceLoadEntry>& entries =
      observer.resource_load_entries();
  ASSERT_EQ(2U, entries.size());
  EXPECT_EQ(net::OK, entries[0].resource_load_info->net_error);
  EXPECT_EQ(net::OK, entries[1].resource_load_info->net_error);
  observer.Reset();

  // Load the page and simulate a network error.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [image_url](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != image_url)
          return false;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_ADDRESS_UNREACHABLE;
        params->client->OnComplete(status);
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  ASSERT_EQ(2U, entries.size());
  // A ResourceLoadInfo is added when the load for the resource is complete,
  // and hence the order is undeterministic.
  if (entries[0].resource_load_info->final_url == page_url) {
    EXPECT_EQ(net::OK, entries[0].resource_load_info->net_error);
    EXPECT_EQ(image_url, entries[1].resource_load_info->final_url);
    EXPECT_EQ(net::ERR_ADDRESS_UNREACHABLE,
              entries[1].resource_load_info->net_error);
  } else {
    EXPECT_EQ(image_url, entries[0].resource_load_info->final_url);
    EXPECT_EQ(net::ERR_ADDRESS_UNREACHABLE,
              entries[0].resource_load_info->net_error);
    EXPECT_EQ(page_url, entries[1].resource_load_info->final_url);
    EXPECT_EQ(net::OK, entries[1].resource_load_info->net_error);
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteAlwaysAccessNetwork) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL cacheable_url(embedded_test_server()->GetURL("/set-header"));
  EXPECT_TRUE(NavigateToURL(shell(), cacheable_url));
  ASSERT_EQ(1U, observer.resource_load_entries().size());
  EXPECT_FALSE(observer.resource_load_entries()[0]
                   .resource_load_info->network_info->always_access_network);
  observer.Reset();

  std::array<std::string, 3> headers = {
      "cache-control: no-cache", "cache-control: no-store", "pragma: no-cache"};
  for (const std::string& header : headers) {
    GURL no_cache_url(embedded_test_server()->GetURL("/set-header?" + header));
    EXPECT_TRUE(NavigateToURL(shell(), no_cache_url));
    ASSERT_EQ(1U, observer.resource_load_entries().size());
    EXPECT_TRUE(observer.resource_load_entries()[0]
                    .resource_load_info->network_info->always_access_network);
    observer.Reset();
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteWithRedirects) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL target_url(embedded_test_server()->GetURL("/title1.html"));
  GURL intermediate_url(
      embedded_test_server()->GetURL("/server-redirect?" + target_url.spec()));
  GURL start_url(embedded_test_server()->GetURL("/server-redirect?" +
                                                intermediate_url.spec()));

  EXPECT_TRUE(
      NavigateToURL(shell(), start_url, target_url /* expected_commit_url */));

  ASSERT_EQ(1U, observer.resource_load_entries().size());
  EXPECT_EQ(target_url,
            observer.resource_load_entries()[0].resource_load_info->final_url);

  ASSERT_EQ(2U, observer.resource_load_entries()[0]
                    .resource_load_info->redirect_info_chain.size());
  EXPECT_EQ(url::Origin::Create(intermediate_url),
            observer.resource_load_entries()[0]
                .resource_load_info->redirect_info_chain[0]
                ->origin_of_new_url);
  EXPECT_TRUE(observer.resource_load_entries()[0]
                  .resource_load_info->redirect_info_chain[0]
                  ->network_info->network_accessed);
  EXPECT_FALSE(observer.resource_load_entries()[0]
                   .resource_load_info->redirect_info_chain[0]
                   ->network_info->always_access_network);
  EXPECT_EQ(url::Origin::Create(target_url),
            observer.resource_load_entries()[0]
                .resource_load_info->redirect_info_chain[1]
                ->origin_of_new_url);
  EXPECT_TRUE(observer.resource_load_entries()[0]
                  .resource_load_info->redirect_info_chain[1]
                  ->network_info->network_accessed);
  EXPECT_FALSE(observer.resource_load_entries()[0]
                   .resource_load_info->redirect_info_chain[1]
                   ->network_info->always_access_network);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteIsMainFrame) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  const std::vector<ResourceLoadObserver::ResourceLoadEntry>& entries =
      observer.resource_load_entries();
  ASSERT_EQ(2U, entries.size());
  EXPECT_EQ(url, entries[0].resource_load_info->original_url);
  EXPECT_EQ(url, entries[0].resource_load_info->final_url);
  EXPECT_TRUE(entries[0].resource_is_associated_with_main_frame);
  EXPECT_TRUE(entries[1].resource_is_associated_with_main_frame);
  observer.Reset();

  // Load that same page inside an iframe.
  GURL data_url("data:text/html,<iframe src='" + url.spec() + "'></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));
  ASSERT_EQ(3U, entries.size());
  EXPECT_EQ(data_url, entries[0].resource_load_info->original_url);
  EXPECT_EQ(data_url, entries[0].resource_load_info->final_url);
  EXPECT_EQ(url, entries[1].resource_load_info->original_url);
  EXPECT_EQ(url, entries[1].resource_load_info->final_url);
  EXPECT_TRUE(entries[0].resource_is_associated_with_main_frame);
  EXPECT_FALSE(entries[1].resource_is_associated_with_main_frame);
  EXPECT_FALSE(entries[2].resource_is_associated_with_main_frame);
}

struct LoadProgressObserver : public WebContentsObserver {
  explicit LoadProgressObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        did_start_loading(false),
        did_stop_loading(false) {}

  // WebContentsObserver:
  void DidStartLoading() override {
    EXPECT_FALSE(did_start_loading);
    EXPECT_EQ(0U, progresses.size());
    EXPECT_FALSE(did_stop_loading);
    did_start_loading = true;
  }

  void DidStopLoading() override {
    EXPECT_TRUE(did_start_loading);
    EXPECT_GE(progresses.size(), 1U);
    EXPECT_FALSE(did_stop_loading);
    did_stop_loading = true;
  }

  void LoadProgressChanged(double progress) override {
    EXPECT_TRUE(did_start_loading);
    EXPECT_FALSE(did_stop_loading);
    progresses.push_back(progress);
  }

  bool did_start_loading;
  std::vector<double> progresses;
  bool did_stop_loading;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, LoadProgress) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto delegate = std::make_unique<LoadProgressObserver>(shell());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  const std::vector<double>& progresses = delegate->progresses;
  // All updates should be in order ...
  if (base::ranges::adjacent_find(progresses, std::greater<>()) !=
      progresses.end()) {
    ADD_FAILURE() << "Progress values should be in order: "
                  << ::testing::PrintToString(progresses);
  }

  // ... and the last one should be 1.0, meaning complete.
  ASSERT_GE(progresses.size(), 1U)
      << "There should be at least one progress update";
  EXPECT_EQ(1.0, *progresses.rbegin());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, LoadProgressWithFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto delegate = std::make_unique<LoadProgressObserver>(shell());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  const std::vector<double>& progresses = delegate->progresses;
  // All updates should be in order ...
  if (base::ranges::adjacent_find(progresses, std::greater<>()) !=
      progresses.end()) {
    ADD_FAILURE() << "Progress values should be in order: "
                  << ::testing::PrintToString(progresses);
  }

  // ... and the last one should be 1.0, meaning complete.
  ASSERT_GE(progresses.size(), 1U)
      << "There should be at least one progress update";
  EXPECT_EQ(1.0, *progresses.rbegin());
}

// Ensure that a new navigation that interrupts a pending one will still fire
// a DidStopLoading.  See http://crbug.com/429399.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadProgressAfterInterruptedNav) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Start at a real page.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Simulate a navigation that has not completed.
  const GURL kURL2 = embedded_test_server()->GetURL("/title2.html");
  TestNavigationManager navigation(shell()->web_contents(), kURL2);
  auto delegate = std::make_unique<LoadProgressObserver>(shell());
  shell()->LoadURL(kURL2);
  EXPECT_TRUE(navigation.WaitForResponse());
  EXPECT_TRUE(delegate->did_start_loading);
  EXPECT_FALSE(delegate->did_stop_loading);

  // Also simulate a DidChangeLoadProgress, but not a DidStopLoading.
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  main_frame->DidChangeLoadProgress(1.0);
  EXPECT_TRUE(delegate->did_start_loading);
  EXPECT_FALSE(delegate->did_stop_loading);

  // Now interrupt with a new cross-process navigation.
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  GURL url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  shell()->LoadURL(url);
  tab_observer.Wait();
  EXPECT_EQ(url, shell()->web_contents()->GetLastCommittedURL());

  // We should have gotten to DidStopLoading.
  EXPECT_TRUE(delegate->did_stop_loading);
}

struct FirstVisuallyNonEmptyPaintObserver : public WebContentsObserver {
  explicit FirstVisuallyNonEmptyPaintObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        did_fist_visually_non_empty_paint_(false) {}

  void DidFirstVisuallyNonEmptyPaint() override {
    did_fist_visually_non_empty_paint_ = true;
    std::move(on_did_first_visually_non_empty_paint_).Run();
  }

  void WaitForDidFirstVisuallyNonEmptyPaint() {
    if (did_fist_visually_non_empty_paint_)
      return;
    base::RunLoop run_loop;
    on_did_first_visually_non_empty_paint_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::OnceClosure on_did_first_visually_non_empty_paint_;
  bool did_fist_visually_non_empty_paint_;
};

// See: http://crbug.com/395664
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FirstVisuallyNonEmptyPaint DISABLED_FirstVisuallyNonEmptyPaint
#else
// http://crbug.com/398471
#define MAYBE_FirstVisuallyNonEmptyPaint DISABLED_FirstVisuallyNonEmptyPaint
#endif
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_FirstVisuallyNonEmptyPaint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<FirstVisuallyNonEmptyPaintObserver> observer(
      new FirstVisuallyNonEmptyPaintObserver(shell()));

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  observer->WaitForDidFirstVisuallyNonEmptyPaint();
  ASSERT_TRUE(observer->did_fist_visually_non_empty_paint_);
}

namespace {

class WebDisplayModeDelegate : public WebContentsDelegate {
 public:
  explicit WebDisplayModeDelegate(blink::mojom::DisplayMode mode)
      : mode_(mode) {}
  ~WebDisplayModeDelegate() override = default;
  WebDisplayModeDelegate(const WebDisplayModeDelegate&) = delete;
  WebDisplayModeDelegate& operator=(const WebDisplayModeDelegate&) = delete;

  blink::mojom::DisplayMode GetDisplayMode(const WebContents* source) override {
    return mode_;
  }
  void set_mode(blink::mojom::DisplayMode mode) { mode_ = mode; }

 private:
  blink::mojom::DisplayMode mode_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangeDisplayMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebDisplayModeDelegate delegate(blink::mojom::DisplayMode::kMinimalUi);
  shell()->web_contents()->SetDelegate(&delegate);

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  ASSERT_TRUE(ExecJs(shell(),
                     "document.title = "
                     " window.matchMedia('(display-mode:"
                     " minimal-ui)').matches"));
  EXPECT_EQ(u"true", shell()->web_contents()->GetTitle());

  delegate.set_mode(blink::mojom::DisplayMode::kFullscreen);
  // Simulate widget is entering fullscreen (changing size is enough).
  shell()
      ->web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->SynchronizeVisualProperties();

  ASSERT_TRUE(ExecJs(shell(),
                     "document.title = "
                     " window.matchMedia('(display-mode:"
                     " fullscreen)').matches"));
  EXPECT_EQ(u"true", shell()->web_contents()->GetTitle());
}

// Observer class used to verify that WebContentsObservers are notified
// when the page scale factor changes.
// See WebContentsImplBrowserTest.ChangePageScale.
class MockPageScaleObserver : public WebContentsObserver {
 public:
  explicit MockPageScaleObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        got_page_scale_update_(false) {
    // Once OnPageScaleFactorChanged is called, quit the run loop.
    ON_CALL(*this, OnPageScaleFactorChanged(::testing::_))
        .WillByDefault(::testing::InvokeWithoutArgs(
            this, &MockPageScaleObserver::GotPageScaleUpdate));
  }

  MOCK_METHOD1(OnPageScaleFactorChanged, void(float page_scale_factor));

  void WaitForPageScaleUpdate() {
    if (!got_page_scale_update_) {
      base::RunLoop run_loop;
      on_page_scale_update_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    got_page_scale_update_ = false;
  }

 private:
  void GotPageScaleUpdate() {
    got_page_scale_update_ = true;
    std::move(on_page_scale_update_).Run();
  }

  base::OnceClosure on_page_scale_update_;
  bool got_page_scale_update_;
};

// When the page scale factor is set in the renderer it should send
// a notification to the browser so that WebContentsObservers are notified.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangePageScale) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  MockPageScaleObserver observer(shell());
  ::testing::InSequence expect_call_sequence;

  shell()->web_contents()->SetPageScale(1.5);
  EXPECT_CALL(observer, OnPageScaleFactorChanged(::testing::FloatEq(1.5)));
  observer.WaitForPageScaleUpdate();

  if (!CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // Navigate to reset the page scale factor. We'll only get the
    // OnPageScaleFactorChanged if we reuse the same RenderFrameHost, which will
    // not happen if ProactivelySwapBrowsingInstance or RenderDocument is
    // enabled for same-site main frame navigations.
    shell()->LoadURL(embedded_test_server()->GetURL("/title2.html"));
    EXPECT_CALL(observer, OnPageScaleFactorChanged(::testing::_));
    observer.WaitForPageScaleUpdate();
  }
}

#if BUILDFLAG(IS_ANDROID)
// Test that when navigating between pages with the same non-one initial scale,
// the browser tracks the correct scale value.
// This test is only relevant for Android, since desktop would always have one
// as the initial scale.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SameInitialScaleAcrossNavigations) {
  // Scale value comparisons don't need to be precise.
  constexpr double kEpsilon = 0.01;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  auto* contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to a page with a non-one initial scale, then determine what the
  // renderer and browser each think the scale is.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  double initial_renderer_scale_1 =
      EvalJs(contents, "window.visualViewport.scale").ExtractDouble();
  double initial_browser_scale_1 =
      contents->GetPrimaryPage().GetPageScaleFactor();

  // Now navigate to another page and record the scales again. Note that this
  // navigation could reuse the RenderFrameHost and in that case the renderer
  // will not inform the browser of the scale again. This was the case in
  // https://crbug.com/1301879
  const auto rfh_id_1 = contents->GetPrimaryMainFrame()->GetGlobalId();
  EXPECT_TRUE(NavigateToURL(shell(), url));
  const auto rfh_id_2 = contents->GetPrimaryMainFrame()->GetGlobalId();
  SCOPED_TRACE(testing::Message()
               << "CanSameSiteMainFrameNavigationsChangeRenderFrameHosts = "
               << CanSameSiteMainFrameNavigationsChangeRenderFrameHosts());
  SCOPED_TRACE(testing::Message()
               << "Did change RenderFrameHost? " << (rfh_id_1 != rfh_id_2));
  double initial_renderer_scale_2 =
      EvalJs(contents, "window.visualViewport.scale").ExtractDouble();
  double initial_browser_scale_2 =
      contents->GetPrimaryPage().GetPageScaleFactor();

  // Ensure both pages are scaled to the same non-one value.
  ASSERT_LT(initial_renderer_scale_1, 1.0 - kEpsilon);
  ASSERT_NEAR(initial_renderer_scale_1, initial_renderer_scale_2, kEpsilon);

  // Test that the browser and renderer agree on the scale.
  EXPECT_NEAR(initial_browser_scale_1, initial_renderer_scale_1, kEpsilon);
  EXPECT_NEAR(initial_browser_scale_2, initial_renderer_scale_2, kEpsilon);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test that a direct navigation to a view-source URL works.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ViewSourceDirectNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  EXPECT_TRUE(NavigateToURL(shell(), kViewSourceURL));
  // Displayed view-source URLs don't include the scheme of the effective URL if
  // the effective URL is HTTP. (e.g. view-source:example.com is displayed
  // instead of view-source:http://example.com).
  EXPECT_EQ(base::ASCIIToUTF16(std::string("view-source:") + kUrl.host() + ":" +
                               kUrl.port() + kUrl.path()),
            shell()->web_contents()->GetTitle());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsViewSourceMode());
}

// Test that window.open to a view-source URL is blocked.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ViewSourceWindowOpen_ShouldBeBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  EXPECT_TRUE(NavigateToURL(shell(), kUrl));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Not allowed to load local resource: view-source:*");
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "window.open('" + kViewSourceURL.spec() + "');"));
  ASSERT_TRUE(console_observer.Wait());
  // Original page shouldn't navigate away, no new tab should be opened.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1u, Shell::windows().size());
}

// Test that a content initiated navigation to a view-source URL is blocked.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ViewSourceRedirect_ShouldBeBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  EXPECT_TRUE(NavigateToURL(shell(), kUrl));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Not allowed to load local resource: view-source:*");

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "window.location = '" + kViewSourceURL.spec() + "';"));
  ASSERT_TRUE(console_observer.Wait());
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->IsViewSourceMode());
}

// Test that view source mode for a webui page can be opened.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ViewSourceWebUI) {
  const std::string kUrl = "view-source:" + GetWebUIURLString(kChromeUIGpuHost);
  // To ensure that NavigateToURL succeeds, append a slash to the view-source:
  // URL, since the slash would be appended anyway as part of the navigation.
  const GURL kGURL(kUrl + "/");
  EXPECT_TRUE(NavigateToURL(shell(), kGURL));
  EXPECT_EQ(base::ASCIIToUTF16(kUrl), shell()->web_contents()->GetTitle());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsViewSourceMode());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, NewNamedWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/click-noreferrer-links.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    ShellAddedObserver new_shell_observer;

    // Open a new, named window.
    EXPECT_TRUE(ExecJs(shell(), "window.open('about:blank','new_window');"));

    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    EXPECT_EQ("new_window",
              static_cast<WebContentsImpl*>(new_shell->web_contents())
                  ->GetPrimaryFrameTree()
                  .root()
                  ->frame_name());

    EXPECT_EQ(true, EvalJs(new_shell, "window.name == 'new_window';"));
  }

  {
    ShellAddedObserver new_shell_observer;

    // Test clicking a target=foo link.
    EXPECT_EQ(true, EvalJs(shell(), "clickSameSiteTargetedLink();"));

    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    EXPECT_EQ("foo", static_cast<WebContentsImpl*>(new_shell->web_contents())
                         ->GetPrimaryFrameTree()
                         .root()
                         ->frame_name());
  }
}

// Test that HasOriginalOpener() tracks provenance through closed WebContentses.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       HasOriginalOpenerTracksThroughClosedWebContents) {
  const GURL blank_url = GURL("about:blank");

  Shell* shell1 = shell();
  EXPECT_TRUE(NavigateToURL(shell1, blank_url));

  Shell* shell2 = OpenPopup(shell1, blank_url, "window2");
  Shell* shell3 = OpenPopup(shell2, blank_url, "window3");

  EXPECT_EQ(
      shell2->web_contents(),
      shell3->web_contents()->GetFirstWebContentsInLiveOriginalOpenerChain());
  EXPECT_EQ(
      shell1->web_contents(),
      shell2->web_contents()->GetFirstWebContentsInLiveOriginalOpenerChain());

  shell2->Close();

  EXPECT_EQ(
      shell1->web_contents(),
      shell3->web_contents()->GetFirstWebContentsInLiveOriginalOpenerChain());
}

// Test that if a BeforeUnload dialog is destroyed due to the commit of a
// cross-site navigation, it will not reset the loading state.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NoResetOnBeforeUnloadCanceledOnCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kStartURL(
      embedded_test_server()->GetURL("/hang_before_unload.html"));
  const GURL kCrossSiteURL(
      embedded_test_server()->GetURL("bar.com", "/title1.html"));

  // Navigate to a first web page with a BeforeUnload event listener.
  EXPECT_TRUE(NavigateToURL(shell(), kStartURL));

  // Start a cross-site navigation that will not commit for the moment.
  // This intentionally does not trigger a BeforeUnload dialog because the
  // main frame has never had user activation.
  TestNavigationManager cross_site_delayer(shell()->web_contents(),
                                           kCrossSiteURL);
  shell()->LoadURL(kCrossSiteURL);
  EXPECT_TRUE(cross_site_delayer.WaitForRequestStart());

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(shell()->web_contents());

  // Click on a link in the page. This will show the BeforeUnload dialog.
  // Ensure the dialog is not dismissed, which will cause it to still be
  // present when the cross-site navigation later commits.
  // Note: the javascript function executed will not do the link click but
  // schedule it for afterwards. Since the BeforeUnload event is synchronous,
  // clicking on the link right away would cause the ExecJs to never
  // return.
  SetShouldProceedOnBeforeUnload(shell(), false, false);
  AppModalDialogWaiter dialog_waiter(shell());
  EXPECT_TRUE(ExecJs(shell(), "clickLinkSoon()"));
  dialog_waiter.Wait();

  // Have the cross-site navigation commit. The main RenderFrameHost should
  // still be loading after that.
  ASSERT_TRUE(cross_site_delayer.WaitForNavigationFinished());
  EXPECT_TRUE(shell()->web_contents()->IsLoading());
}

namespace {
void NavigateToDataURLAndCheckForTerminationDisabler(
    Shell* shell,
    const std::string& html,
    bool expect_unload,
    bool expect_beforeunload,
    bool expect_pagehide,
    bool expect_visibilitychange) {
  EXPECT_TRUE(NavigateToURL(shell, GURL("data:text/html," + html)));
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(expect_unload || expect_beforeunload || expect_pagehide ||
                expect_visibilitychange,
            shell->web_contents()->NeedToFireBeforeUnloadOrUnloadEvents());
  EXPECT_EQ(expect_unload,
            rfh->GetSuddenTerminationDisablerState(
                blink::mojom::SuddenTerminationDisablerType::kUnloadHandler));
  EXPECT_EQ(
      expect_beforeunload,
      rfh->GetSuddenTerminationDisablerState(
          blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler));
  EXPECT_EQ(expect_pagehide,
            rfh->GetSuddenTerminationDisablerState(
                blink::mojom::SuddenTerminationDisablerType::kPageHideHandler));
  EXPECT_EQ(expect_visibilitychange,
            rfh->GetSuddenTerminationDisablerState(
                blink::mojom::SuddenTerminationDisablerType::
                    kVisibilityChangeHandler));
}
}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerNone) {
  const std::string NO_HANDLERS_HTML = "<html><body>foo</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), NO_HANDLERS_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTest,
    SuddenTerminationDisablerNoneProcessTerminationDisallowed) {
  const std::string NO_HANDLERS_HTML = "<html><body>foo</body></html>";
  // The WebContents termination disabler should be independent of the
  // RenderProcessHost termination disabler, as process termination can depend
  // on more than the presence of a beforeunload/unload handler.
  shell()
      ->web_contents()
      ->GetPrimaryMainFrame()
      ->GetProcess()
      ->SetSuddenTerminationAllowed(false);
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), NO_HANDLERS_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnUnload) {
  const std::string UNLOAD_HTML =
      "<html><body><script>window.onunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), UNLOAD_HTML, true /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnPagehide) {
  const std::string PAGEHIDE_HTML =
      "<html><body><script>window.onpagehide=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), PAGEHIDE_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, true /* expect_pagehide */,
      false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnVisibilityChangeDocument) {
  const std::string VISIBILITYCHANGE_HTML =
      "<html><body><script>"
      "document.addEventListener('visibilitychange', (e) => {});"
      "</script></body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), VISIBILITYCHANGE_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      true /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnVisibilityChangeWindow) {
  const std::string VISIBILITYCHANGE_HTML =
      "<html><body><script>"
      "window.addEventListener('visibilitychange', (e) => {});"
      "</script></body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), VISIBILITYCHANGE_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      true /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTest,
    SuddenTerminationDisablerOnVisibilityChangeRemoveDocumentListener) {
  const std::string VISIBILITYCHANGE_HTML =
      "<html><body><script>"
      "function handleVisibilityChange(e) {}"
      "window.addEventListener('visibilitychange', handleVisibilityChange);"
      "document.addEventListener('visibilitychange', handleVisibilityChange);"
      "document.removeEventListener('visibilitychange', "
      "handleVisibilityChange);"
      "</script></body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), VISIBILITYCHANGE_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      true /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTest,
    SuddenTerminationDisablerOnVisibilityChangeRemoveWindowListener) {
  const std::string VISIBILITYCHANGE_HTML =
      "<html><body><script>"
      "function handleVisibilityChange(e) {}"
      "window.onvisibilitychange = handleVisibilityChange;"
      "document.onvisibilitychange = handleVisibilityChange;"
      "window.removeEventListener('visibilitychange', handleVisibilityChange);"
      "</script></body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), VISIBILITYCHANGE_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      true /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnBeforeUnload) {
  const std::string BEFORE_UNLOAD_HTML =
      "<html><body><script>window.onbeforeunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), BEFORE_UNLOAD_HTML, false /* expect_unload */,
      true /* expect_beforeunload */, false /* expect_pagehide */,
      false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerAllThenNavigate) {
  const std::string ALL_HANDLERS_HTML =
      "<html><body><script>window.onunload=function(e) {};"
      "window.onpagehide=function(e) {};"
      "document.onvisibilitychange=function(e) {}; "
      "window.onbeforeunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), ALL_HANDLERS_HTML, true /* expect_unload */,
      true /* expect_beforeunload */, true /* expect_pagehide */,
      true /* expect_visibilitychange*/);
  // After navigation to empty page, the values should be reset to false.
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), "", false /* expect_unload */, false /* expect_beforeunload */,
      false /* expect_pagehide */, false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerAllThenRemove) {
  const std::string ALL_HANDLERS_ADDED_THEN_REMOVED_HTML =
      "<html><body><script>"
      "function handleEverything(e) {}"
      "window.addEventListener('unload', handleEverything);"
      "window.addEventListener('beforeunload', handleEverything);"
      "window.addEventListener('pagehide', handleEverything);"
      "window.addEventListener('visibilitychange', handleEverything);"
      "document.addEventListener('visibilitychange', handleEverything);"
      "window.removeEventListener('unload', handleEverything);"
      "window.removeEventListener('beforeunload', handleEverything);"
      "window.removeEventListener('pagehide', handleEverything);"
      "window.removeEventListener('visibilitychange', handleEverything);"
      "document.removeEventListener('visibilitychange', handleEverything);"
      "</script></body></html>";
  // After the handlers were added, they got deleted, so we should treat them as
  // non-existent in the end.
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), ALL_HANDLERS_ADDED_THEN_REMOVED_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      false /* expect_visibilitychange */);
}

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTest,
    SuddenTerminationDisablerWhenTabIsHiddenOnVisibilityChange) {
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  const std::string VISIBILITYCHANGE_HTML =
      "<html><body><script>document.onvisibilitychange=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), VISIBILITYCHANGE_HTML, false /* expect_unload */,
      false /* expect_beforeunload */, false /* expect_pagehide */,
      true /* expect_visibilitychange */);
  web_contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(shell()->web_contents()->NeedToFireBeforeUnloadOrUnloadEvents());

  // The visibilitychange handler won't block sudden termination if the tab is
  // already hidden.
  web_contents->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_TRUE(
      static_cast<WebContentsImpl*>(shell()->web_contents())->IsHidden());
  EXPECT_FALSE(shell()->web_contents()->NeedToFireBeforeUnloadOrUnloadEvents());

  // The visibilitychange handler will block sudden termination if the tab
  // becomes visible again.
  web_contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(shell()->web_contents()->NeedToFireBeforeUnloadOrUnloadEvents());

  // The visibilitychange handler won't block sudden termination if the tab is
  // occluded (because we treat it as hidden), unless when occlusion is
  // disabled, in which case we treat it the same as being visible.
  const bool occlusion_is_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackgroundingOccludedWindowsForTesting);
  web_contents->UpdateWebContentsVisibility(Visibility::OCCLUDED);
  EXPECT_EQ(occlusion_is_disabled,
            shell()->web_contents()->NeedToFireBeforeUnloadOrUnloadEvents());
}

class TestWCDelegateForDialogsAndFullscreen : public JavaScriptDialogManager,
                                              public WebContentsDelegate {
 public:
  explicit TestWCDelegateForDialogsAndFullscreen(WebContentsImpl* web_contents)
      : web_contents_(web_contents) {
    old_delegate_ = web_contents_->GetDelegate();
    web_contents_->SetDelegate(this);
  }
  ~TestWCDelegateForDialogsAndFullscreen() override {
    web_contents_->SetJavaScriptDialogManagerForTesting(nullptr);
    web_contents_->SetDelegate(old_delegate_);
  }

  TestWCDelegateForDialogsAndFullscreen(
      const TestWCDelegateForDialogsAndFullscreen&) = delete;
  TestWCDelegateForDialogsAndFullscreen& operator=(
      const TestWCDelegateForDialogsAndFullscreen&) = delete;

  void WillWaitForDialog() { waiting_for_ = kDialog; }
  void WillWaitForNewContents() { waiting_for_ = kNewContents; }
  void WillWaitForFullscreenEnter() { waiting_for_ = kFullscreenEnter; }
  void WillWaitForFullscreenExit() { waiting_for_ = kFullscreenExit; }
  void WillWaitForFullscreenOption() { waiting_for_ = kFullscreenOptions; }

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  std::string last_message() { return last_message_; }

  WebContents* last_popup() { return popups_.back().get(); }

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    fullscreen_mode_ = WebContents::FromRenderFrameHost(requesting_frame)
                               ->IsBeingVisiblyCaptured()
                           ? FullscreenMode::kPseudoContent
                           : FullscreenMode::kContent;
    fullscreen_options_ = options;

    if (waiting_for_ == kFullscreenEnter) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  void FullscreenStateChangedForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    fullscreen_options_ = options;

    if (waiting_for_ == kFullscreenOptions) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  void ExitFullscreenModeForTab(WebContents*) override {
    fullscreen_mode_ = FullscreenMode::kWindowed;
    fullscreen_options_ = blink::mojom::FullscreenOptions();

    if (waiting_for_ == kFullscreenExit) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return fullscreen_mode_ == FullscreenMode::kContent ||
           fullscreen_mode_ == FullscreenMode::kPseudoContent;
  }

  FullscreenState GetFullscreenState(
      const WebContents* web_contents) const override {
    FullscreenState state;
    state.target_mode = fullscreen_mode_;
    state.target_display_id = fullscreen_options_.display_id;
    return state;
  }

  const blink::mojom::FullscreenOptions& fullscreen_options() {
    return fullscreen_options_;
  }

  WebContents* AddNewContents(
      WebContents* source,
      std::unique_ptr<WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    popups_.push_back(std::move(new_contents));

    if (waiting_for_ == kNewContents) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
    return nullptr;
  }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    last_message_ = base::UTF16ToUTF8(message_text);
    *did_suppress_message = true;

    if (waiting_for_ == kDialog) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {
    std::move(callback).Run(true, std::u16string());

    if (waiting_for_ == kDialog) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {}

 private:
  raw_ptr<WebContentsImpl> web_contents_;
  raw_ptr<WebContentsDelegate> old_delegate_;

  enum {
    kNothing,
    kDialog,
    kNewContents,
    kFullscreenEnter,
    kFullscreenExit,
    kFullscreenOptions,
  } waiting_for_ = kNothing;

  std::string last_message_;

  FullscreenMode fullscreen_mode_ = FullscreenMode::kWindowed;
  blink::mojom::FullscreenOptions fullscreen_options_;

  std::vector<std::unique_ptr<WebContents>> popups_;

  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
};

class MockFileSelectListener : public FileChooserImpl::FileSelectListenerImpl {
 public:
  MockFileSelectListener() : FileChooserImpl::FileSelectListenerImpl(nullptr) {
    SetListenerFunctionCalledTrueForTesting();
  }
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {}
  void FileSelectionCanceled() override { cancelled_ = true; }

  bool cancelled() const { return cancelled_; }

 private:
  ~MockFileSelectListener() override = default;
  bool cancelled_ = false;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       JavaScriptDialogsInMainAndSubframes) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(WaitForLoadStop(wc));

  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();
  ASSERT_EQ(0U, root->child_count());

  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  GURL::Replacements clear_port;
  clear_port.ClearPort();

  // A dialog from the main frame.
  std::string alert_location = "alert(document.location)";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ("about:blank", test_delegate.last_message());

  // These is a different origin iframe, so alerts won't work if the feature
  // is enabled. Ideally we would test they don't show, but there is no way
  // to check for a lack of dialog window.
  if (!base::FeatureList::IsEnabled(
          features::kSuppressDifferentOriginSubframeJSDialogs)) {
    // A dialog from the subframe.
    // Navigate the subframe cross-site.
    EXPECT_TRUE(NavigateToURLFromRenderer(
        frame, embedded_test_server()->GetURL("b.com", "/title2.html")));
    EXPECT_TRUE(WaitForLoadStop(wc));

    // A dialog from the subframe.
    test_delegate.WillWaitForDialog();
    EXPECT_TRUE(ExecJs(frame->current_frame_host(), alert_location));
    test_delegate.Wait();
    EXPECT_EQ(GURL("http://b.com/title2.html"),
              GURL(test_delegate.last_message()).ReplaceComponents(clear_port));
  }

  // Navigate the subframe to the same origin as the main frame; ensure
  // dialogs work.
  // Navigate the subframe cross-site.
  GURL same_origin_url =
      embedded_test_server()->GetURL("a.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(frame, same_origin_url));
  EXPECT_TRUE(WaitForLoadStop(wc));

  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(same_origin_url.spec(), test_delegate.last_message());

  // A dialog from the main frame.
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // Navigate the top frame cross-site; ensure that dialogs work.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("c.com", "/title3.html")));
  EXPECT_TRUE(WaitForLoadStop(wc));
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://c.com/title3.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // Navigate back; ensure that dialogs work.
  wc->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(wc));
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       JavaScriptDialogsNormalizeText) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // A dialog with mixed linebreaks.
  std::string alert = "alert('1\\r2\\r\\n3\\n4')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(wc, alert));
  test_delegate.Wait();
  EXPECT_EQ("1\n2\n3\n4", test_delegate.last_message());
}

class WebContentsImplBrowserTestWithDifferentOriginSubframeDialogSuppression
    : public WebContentsImplBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(
        features::kSuppressDifferentOriginSubframeJSDialogs);
    WebContentsImplBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTestWithDifferentOriginSubframeDialogSuppression,
    OriginTrialDisablesSuppression) {
  // Generated with tools/origin_trials/generate_token.py --expire-days 5000
  // http://allowdialogs.test:9999
  // DisableDifferentOriginSubframeDialogSuppression
  std::string origin_trial_token =
      "AwcVbxsLRzn8IXBNaeCrK7amKs211vWkv5oCYo+gssujKeltEtcIaQD+O9hWO+"
      "GT3WtKUFhEA30+QuqyU3TUvQkAAAB/"
      "eyJvcmlnaW4iOiAiaHR0cDovL2FsbG93ZGlhbG9ncy50ZXN0Ojk5OTkiLCAiZmVhdHVyZSI6"
      "ICJEaXNhYmxlRGlmZmVyZW50T3JpZ2luU3ViZnJhbWVEaWFsb2dTdXBwcmVzc2lvbiIsICJl"
      "eHBpcnkiOiAyMDU0NzU5MTcyfQ==";
  GURL origin_trial_url = GURL("http://allowdialogs.test:9999");

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(WaitForLoadStop(wc));

  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();
  ASSERT_EQ(0U, root->child_count());

  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  // We need to use a URLLoaderInterceptor for the subframe since origin trial
  // is origin bound, and embedded test server randomizes ports.
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != origin_trial_url)
          return false;
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Origin-Trial: " +
                origin_trial_token + "\n\n",
            "", params->client.get());
        return true;
      }));

  // A dialog from the subframe.
  // Navigate the subframe to the site with the origin trial meta tag.
  EXPECT_TRUE(NavigateToURLFromRenderer(frame, origin_trial_url));
  EXPECT_TRUE(WaitForLoadStop(wc));

  // A dialog from the subframe, which should show even though different origin
  // subframe dialog suppression is enabled, since the origin trial overrides
  // it.
  std::string alert_location = "alert(document.location)";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(origin_trial_url, GURL(test_delegate.last_message()));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       CreateWebContentsWithRendererProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* base_web_contents = shell()->web_contents();
  ASSERT_TRUE(base_web_contents);

  WebContents::CreateParams create_params(
      base_web_contents->GetBrowserContext());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kInitializeAndWarmupRendererProcess;
  std::unique_ptr<WebContents> web_contents(WebContents::Create(create_params));
  ASSERT_TRUE(web_contents);

  // There is no navigation (to about:blank or something like that).
  EXPECT_FALSE(web_contents->IsLoading());

  // The WebContents have an associated main frame and a renderer process that
  // has either already launched (or is in the process of being launched).
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
  RenderProcessHost* process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  int renderer_id = process->GetID();
  ASSERT_TRUE(process);
  EXPECT_TRUE(process->IsInitializedAndNotDead());

  // Navigate the WebContents.
  GURL url(embedded_test_server()->GetURL("c.com", "/title3.html"));
  TestNavigationObserver same_tab_observer(web_contents.get());
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();
  EXPECT_TRUE(same_tab_observer.last_navigation_succeeded());

  // Check that pre-warmed process is used.
  EXPECT_EQ(process, web_contents->GetPrimaryMainFrame()->GetProcess());
  EXPECT_EQ(renderer_id,
            web_contents->GetPrimaryMainFrame()->GetProcess()->GetID());
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
}

// Regression test for https://crbug.com/840409.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       CreateWebContentsWithoutRendererProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* base_web_contents = shell()->web_contents();
  ASSERT_TRUE(base_web_contents);

  for (int i = 1; i <= 2; i++) {
    SCOPED_TRACE(testing::Message() << "Iteration #" << i);

    WebContents::CreateParams create_params(
        base_web_contents->GetBrowserContext());
    create_params.desired_renderer_state =
        WebContents::CreateParams::kNoRendererProcess;
    std::unique_ptr<WebContents> web_contents(
        WebContents::Create(create_params));
    ASSERT_TRUE(web_contents);
    base::RunLoop().RunUntilIdle();

    // There is no navigation (to about:blank or something like that) yet.
    EXPECT_FALSE(web_contents->IsLoading());

    // The WebContents have an associated main frame and a RenderProcessHost
    // object, but no actual OS process has been launched yet.
    ASSERT_TRUE(web_contents->GetPrimaryMainFrame());
    EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
    EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
    RenderProcessHost* process =
        web_contents->GetPrimaryMainFrame()->GetProcess();
    int renderer_id = process->GetID();
    ASSERT_TRUE(process);
    EXPECT_FALSE(process->IsInitializedAndNotDead());
    EXPECT_EQ(base::kNullProcessHandle, process->GetProcess().Handle());

    // Navigate the WebContents.
    GURL url(embedded_test_server()->GetURL("c.com", "/title3.html"));
    TestNavigationObserver same_tab_observer(web_contents.get());
    NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    web_contents->GetController().LoadURLWithParams(params);
    same_tab_observer.Wait();
    EXPECT_TRUE(same_tab_observer.last_navigation_succeeded());

    // The process should be launched now.
    EXPECT_TRUE(process->IsInitializedAndNotDead());
    EXPECT_NE(base::kNullProcessHandle, process->GetProcess().Handle());

    // Check that the RenderProcessHost and its ID didn't change.
    EXPECT_EQ(process, web_contents->GetPrimaryMainFrame()->GetProcess());
    EXPECT_EQ(renderer_id,
              web_contents->GetPrimaryMainFrame()->GetProcess()->GetID());

    // Verify that the navigation succeeded.
    EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
    NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(url, entry->GetURL());
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NavigatingToWebUIUsesPreWarmedProcess) {
  GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                  std::string(kChromeUIGpuHost));
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* base_web_contents = shell()->web_contents();
  ASSERT_TRUE(base_web_contents);

  WebContents::CreateParams create_params(
      base_web_contents->GetBrowserContext());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kInitializeAndWarmupRendererProcess;
  std::unique_ptr<WebContents> web_contents(WebContents::Create(create_params));
  ASSERT_TRUE(web_contents);

  // There is no navigation (to about:blank or something like that).
  EXPECT_FALSE(web_contents->IsLoading());

  ASSERT_TRUE(web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
  int renderer_id = web_contents->GetPrimaryMainFrame()->GetProcess()->GetID();

  TestNavigationObserver same_tab_observer(web_contents.get(), 1);
  NavigationController::LoadURLParams params(web_ui_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();

  // Check that pre-warmed process was used.  This is possible because the
  // initial RenderFrameHost is allowed to be reused for WebUI, even if it has a
  // live RenderFrame, as long as its SiteInstance is unassigned and its process
  // is unused.
  EXPECT_EQ(renderer_id,
            web_contents->GetPrimaryMainFrame()->GetProcess()->GetID());
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(web_ui_url, entry->GetURL());
}

namespace {

class DownloadImageObserver {
 public:
  MOCK_METHOD5(OnFinishDownloadImage,
               void(int id,
                    int status_code,
                    const GURL& image_url,
                    const std::vector<SkBitmap>& bitmap,
                    const std::vector<gfx::Size>& sizes));
  ~DownloadImageObserver() = default;
};

void DownloadImageTestInternal(Shell* shell,
                               const GURL& image_url,
                               int expected_http_status,
                               int expected_number_of_images) {
  using ::testing::_;
  using ::testing::InvokeWithoutArgs;
  using ::testing::SizeIs;

  // Set up everything.
  DownloadImageObserver download_image_observer;
  scoped_refptr<MessageLoopRunner> loop_runner = new MessageLoopRunner();

  // Set up expectation and stub.
  EXPECT_CALL(download_image_observer,
              OnFinishDownloadImage(_, expected_http_status, _,
                                    SizeIs(expected_number_of_images), _));
  ON_CALL(download_image_observer, OnFinishDownloadImage(_, _, _, _, _))
      .WillByDefault(
          InvokeWithoutArgs(loop_runner.get(), &MessageLoopRunner::Quit));

  shell->LoadURL(GURL("about:blank"));
  shell->web_contents()->DownloadImage(
      image_url, false, gfx::Size(), 1024, false,
      base::BindOnce(&DownloadImageObserver::OnFinishDownloadImage,
                     base::Unretained(&download_image_observer)));

  // Wait for response.
  loop_runner->Run();
}

void ExpectNoValidImageCallback(base::OnceClosure quit_closure,
                                int id,
                                int status_code,
                                const GURL& image_url,
                                const std::vector<SkBitmap>& bitmap,
                                const std::vector<gfx::Size>& sizes) {
  EXPECT_EQ(200, status_code);
  EXPECT_TRUE(bitmap.empty());
  EXPECT_TRUE(sizes.empty());
  std::move(quit_closure).Run();
}

void ExpectSingleValidImageCallback(base::OnceClosure quit_closure,
                                    int expected_width,
                                    int expected_height,
                                    int id,
                                    int status_code,
                                    const GURL& image_url,
                                    const std::vector<SkBitmap>& bitmap,
                                    const std::vector<gfx::Size>& sizes) {
  EXPECT_EQ(200, status_code);
  ASSERT_EQ(bitmap.size(), 1u);
  EXPECT_EQ(bitmap[0].width(), expected_width);
  EXPECT_EQ(bitmap[0].height(), expected_height);
  ASSERT_EQ(sizes.size(), 1u);
  EXPECT_EQ(sizes[0].width(), expected_width);
  EXPECT_EQ(sizes[0].height(), expected_height);
  std::move(quit_closure).Run();
}

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_HttpImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/single_face.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 200, 1);
}

// Disabled due to flakiness: https://crbug.com/1124349.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_DownloadImage_Deny_FileImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));

  const GURL kImageUrl = GetTestUrl("", "single_face.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

// Disabled due to flakiness: https://crbug.com/1124349.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_DownloadImage_Allow_FileImage) {
  shell()->LoadURL(GetTestUrl("", "simple_page.html"));

  const GURL kImageUrl = GetTestUrl("", "image.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_NoValidImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/invalid.ico");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(), 2, false,
      base::BindOnce(&ExpectNoValidImageCallback, run_loop.QuitClosure()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_DataImage) {
  const GURL kImageUrl = GURL(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHE"
      "lEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_InvalidDataImage) {
  const GURL kImageUrl = GURL("data:image/png;invalid");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_DataImageSVG) {
  const GURL kImageUrl(
      "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' "
      "width='64' height='64'></svg>");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(30, 30), 1024, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     30, 30));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredSizeZero) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(), 1024, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     90, 90));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredSizeClampedByMaxSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(60, 60), 30, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     30, 30));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredWidthClampedByMaxSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(60, 30), 30, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     30, 15));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredHeightClampedByMaxSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(30, 60), 30, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     15, 30));

  run_loop.Run();
}

namespace {

void ExpectTwoValidImageCallback(base::OnceClosure quit_closure,
                                 const std::vector<gfx::Size>& expected_sizes,
                                 int id,
                                 int status_code,
                                 const GURL& image_url,
                                 const std::vector<SkBitmap>& bitmap,
                                 const std::vector<gfx::Size>& sizes) {
  EXPECT_EQ(200, status_code);
  ASSERT_EQ(bitmap.size(), expected_sizes.size());
  ASSERT_EQ(sizes.size(), expected_sizes.size());
  for (size_t i = 0; i < expected_sizes.size(); ++i) {
    EXPECT_EQ(gfx::Size(bitmap[i].width(), bitmap[i].height()),
              expected_sizes[i]);
    EXPECT_EQ(sizes[i], expected_sizes[i]);
  }
  std::move(quit_closure).Run();
}

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_MultipleImagesNoMaxSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl =
      embedded_test_server()->GetURL("/icon-with-two-entries.ico");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  std::vector<gfx::Size> expected_sizes{{16, 16}, {32, 32}};
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, gfx::Size(), 0, false,
      base::BindOnce(&ExpectTwoValidImageCallback, run_loop.QuitClosure(),
                     expected_sizes));

  run_loop.Run();
}

class PointerLockDelegate : public WebContentsDelegate {
 public:
  // WebContentsDelegate:
  void RequestPointerLock(WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override {
    request_pointer_lock_called_ = true;
  }
  bool request_pointer_lock_called_ = false;
};

// TODO(crbug.com/41422519): This test is flaky.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_RenderWidgetDeletedWhileMouseLockPending) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<PointerLockDelegate> delegate(new PointerLockDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Try to request pointer lock. WebContentsDelegate should get a notification.
  ASSERT_TRUE(ExecJs(shell(),
                     "window.domAutomationController.send(document.body."
                     "requestPointerLock());"));
  EXPECT_TRUE(delegate.get()->request_pointer_lock_called_);

  // Make sure that the renderer didn't get the pointer lock, since the
  // WebContentsDelegate didn't approve the notification.
  EXPECT_EQ(true, EvalJs(shell(), "document.pointerLockElement == null;"));

  // Try to request the pointer lock again. Since there's a pending request in
  // WebContentsDelelgate, the WebContents shouldn't ask again.
  delegate.get()->request_pointer_lock_called_ = false;
  ASSERT_TRUE(ExecJs(shell(),
                     "window.domAutomationController.send(document.body."
                     "requestPointerLock());"));
  EXPECT_FALSE(delegate.get()->request_pointer_lock_called_);

  // Force a cross-process navigation so that the RenderWidgetHost is deleted.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // Make sure the WebContents cleaned up the previous pending request. A new
  // request should be forwarded to the WebContentsDelegate.
  delegate.get()->request_pointer_lock_called_ = false;
  ASSERT_TRUE(ExecJs(shell(),
                     "window.domAutomationController.send(document.body."
                     "requestPointerLock());"));
  EXPECT_TRUE(delegate.get()->request_pointer_lock_called_);
}

// Checks that user agent override string is only used when it's overridden.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, UserAgentOverride) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string kHeaderPath =
      std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
  const GURL kUrl(embedded_test_server()->GetURL(kHeaderPath));
  const std::string kUserAgentOverride = "foo";

  EXPECT_TRUE(NavigateToURL(shell(), kUrl));
  EXPECT_NE(kUserAgentOverride,
            EvalJs(shell()->web_contents(), "document.body.textContent;"));

  shell()->web_contents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly("foo"), false);
  EXPECT_TRUE(NavigateToURL(shell(), kUrl));
  EXPECT_NE(kUserAgentOverride,
            EvalJs(shell()->web_contents(), "document.body.textContent;"));

  shell()
      ->web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  tab_observer.Wait();
  EXPECT_EQ(kUserAgentOverride,
            EvalJs(shell()->web_contents(), "document.body.textContent;"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       UserAgentOverrideDuringDeferredNavigation) {
  // Validates that when a deferred navigation is pending (e.g. in the case of
  // Android WebView popups), we respect any user agent overrides that are set
  // to apply to new tabs during this time.
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string kHeaderPath =
      base::StrCat({"/echoheader?", net::HttpRequestHeaders::kUserAgent});
  const GURL kUrl(embedded_test_server()->GetURL(kHeaderPath));
  const std::string kUserAgentOverride = "foo";

  shell()->set_delay_popup_contents_delegate_for_testing(true);

  EXPECT_TRUE(NavigateToURL(shell(), kUrl));

  // Make a popup.
  Shell* new_shell = nullptr;
  WebContents* new_contents = nullptr;
  {
    ShellAddedObserver new_shell_observer;
    // Set `noopener` to force a deferred navigation (setting
    // `delayed_load_url_params_` rather than triggering the navigation from the
    // renderer).
    std::string popup_script = JsReplace("window.open($1,'','noopener')", kUrl);
    EXPECT_TRUE(ExecJs(shell(), popup_script));
    new_shell = new_shell_observer.GetShell();
    new_contents = new_shell->web_contents();
    // Delaying popup holds the initial load of `url`.
    EXPECT_TRUE(WaitForLoadStop(new_contents));
    EXPECT_TRUE(new_contents->GetController()
                    .GetLastCommittedEntry()
                    ->IsInitialEntry());
  }
  EXPECT_TRUE(
      static_cast<WebContentsImpl*>(new_contents)->delayed_load_url_params_);
  EXPECT_EQ(static_cast<WebContentsImpl*>(new_contents)
                ->delayed_load_url_params_->override_user_agent,
            NavigationController::UA_OVERRIDE_FALSE);

  // After the popup has been created - but before the navigation is made -
  // override the UA, akin to AwSettings application of user agent overrides.
  new_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly("foo"), true);

  // Validate that the pending request has been updated.
  EXPECT_EQ(static_cast<WebContentsImpl*>(new_contents)
                ->delayed_load_url_params_->override_user_agent,
            NavigationController::UA_OVERRIDE_TRUE);

  // Resume loading by setting the delegate (via
  // `set_delay_popup_contents_delegate_for_testing()`).
  EXPECT_FALSE(new_contents->GetDelegate());
  new_contents->SetDelegate(new_shell);
  new_contents->ResumeLoadingCreatedWebContents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // Ensure that the override was properly set, even when a navigation was
  // enqueued.
  EXPECT_EQ(kUserAgentOverride,
            EvalJs(new_contents, "document.body.textContent;"));
}

// Verifies the user-agent string may be changed in DidStartNavigation().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SetUserAgentOverrideFromDidStartNavigation) {
  net::test_server::ControllableHttpResponse http_response(
      embedded_test_server(), "", true);
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string user_agent_override = "foo";
  UserAgentInjector injector(shell()->web_contents(), user_agent_override);
  shell()->web_contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          embedded_test_server()->GetURL("/test.html")));
  http_response.WaitForRequest();
  http_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html>");
  http_response.Done();
  EXPECT_EQ(user_agent_override, http_response.http_request()->headers.at(
                                     net::HttpRequestHeaders::kUserAgent));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(user_agent_override,
            EvalJs(shell()->web_contents(), "navigator.userAgent;"));
}

// Used by SetIsOverridingUserAgent(), adding assertions unique to it.
class NoEntryUserAgentInjector : public UserAgentInjector {
 public:
  NoEntryUserAgentInjector(WebContents* web_contents,
                           const std::string& user_agent)
      : UserAgentInjector(web_contents, user_agent) {}

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    UserAgentInjector::DidStartNavigation(navigation_handle);
    // DidStartNavigation() should only be called once for this test.
    ASSERT_FALSE(was_did_start_navigation_called_);
    was_did_start_navigation_called_ = true;

    // This test expects to exercise the code where thee NavigationRequest is
    // created before the NavigationEntry.
    EXPECT_EQ(
        0, static_cast<NavigationRequest*>(navigation_handle)->nav_entry_id());
  }

 private:
  bool was_did_start_navigation_called_ = false;
};

// Verifies the user-agent string may be changed for a NavigationRequest whose
// NavigationEntry is created after the NavigationRequest is.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SetIsOverridingUserAgentNoEntry) {
  net::test_server::ControllableHttpResponse http_response1(
      embedded_test_server(), "", true);
  net::test_server::ControllableHttpResponse http_response2(
      embedded_test_server(), "", true);
  net::test_server::ControllableHttpResponse http_response3(
      embedded_test_server(), "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  shell()->web_contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          embedded_test_server()->GetURL("/test.html")));
  http_response1.WaitForRequest();
  http_response1.Send(net::HTTP_OK, "text/html", "<html>");
  http_response1.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  shell()->web_contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          embedded_test_server()->GetURL("/test2.html")));
  http_response2.WaitForRequest();
  http_response2.Send(net::HTTP_OK, "text/html", "<html>");
  http_response2.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Register a WebContentsObserver that changes the user-agent.
  const std::string user_agent_override = "foo";
  NoEntryUserAgentInjector injector(shell()->web_contents(),
                                    user_agent_override);

  // This tests executes two JS statements. The second statement (reload())
  // results in a particular NavigationEntry being created. This only works
  // if there is an IPC to the renderer, which was historically always called,
  // but now only called if a before-unload handler is present. Force the extra
  // IPC by making RenderFrameHost believe there is a before-unload handler.
  static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame())
      ->SuddenTerminationDisablerChanged(
          true,
          blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);

  // This triggers creating a NavigationRequest without a NavigationEntry. More
  // specifically back() triggers creating a pending entry, and because back()
  // does not complete, the reload() call results in a NavigationRequest with no
  // NavigationEntry.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), "history.back(); location.reload();"));

  http_response3.WaitForRequest();
  http_response3.Send(net::HTTP_OK, "text/html", "<html>");
  http_response3.Done();
  EXPECT_EQ(user_agent_override, http_response3.http_request()->headers.at(
                                     net::HttpRequestHeaders::kUserAgent));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  auto* controller = &(shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller->GetLastCommittedEntryIndex());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetIsOverridingUserAgent());
  EXPECT_EQ(user_agent_override,
            EvalJs(shell()->web_contents(), "navigator.userAgent;"));
}

class WebContentsImplBrowserTestClientHintsEnabled
    : public WebContentsImplBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.Reset();
    WebContentsImplBrowserTest::SetUp();
  }
};

// Verifies client hints are updated when the user-agent is changed in
// DidStartNavigation().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestClientHintsEnabled,
                       SetUserAgentOverrideFromDidStartNavigation) {
  MockClientHintsControllerDelegate client_hints_controller_delegate(
      content::GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);
  net::test_server::ControllableHttpResponse http_response(
      embedded_test_server(), "", true);
  ASSERT_TRUE(embedded_test_server()->Start());
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = "x";
  ua_override.ua_metadata_override.emplace();
  ua_override.ua_metadata_override->brand_version_list.emplace_back("x", "y");
  ua_override.ua_metadata_override->brand_full_version_list.emplace_back("x1",
                                                                         "y1");
  ua_override.ua_metadata_override->mobile = true;
  UserAgentInjector injector(shell()->web_contents(), ua_override);
  shell()->web_contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(
          embedded_test_server()->GetURL("/test.html")));
  http_response.WaitForRequest();
  http_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html>");
  http_response.Done();
  const std::string mobile_id = network::GetClientHintToNameMap().at(
      network::mojom::WebClientHintsType::kUAMobile);
  ASSERT_TRUE(base::Contains(http_response.http_request()->headers, mobile_id));
  // "?!" corresponds to "mobile=true".
  EXPECT_EQ("?1", http_response.http_request()->headers.at(mobile_id));
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(nullptr);
}

// Verifies client hints are updated when the user-agent is changed in
// DidStartNavigation().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestClientHintsEnabled,
                       SetUserAgentOverrideWithAcceptCHRestart) {
  net::EmbeddedTestServer http2_server(
      net::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2);

  MockClientHintsControllerDelegate client_hints_controller_delegate(
      content::GetShellUserAgentMetadata());
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  std::vector<std::string> accept_ch_tokens;
  for (const auto& pair : network::GetClientHintToNameMap())
    accept_ch_tokens.push_back(pair.second);
  http2_server.SetAlpsAcceptCH("", base::JoinString(accept_ch_tokens, ","));
  http2_server.ServeFilesFromSourceDirectory("content/test/data");

  base::RunLoop run_loop;
  http2_server.RegisterRequestMonitor(base::BindRepeating(
      [](base::RunLoop* run_loop,
         const net::test_server::HttpRequest& request) {
        for (auto header : request.headers)
          LOG(INFO) << header.first << ": " << header.second;
        if (request.relative_url.compare("/empty.html") == 0) {
          EXPECT_EQ(request.headers.at("User-Agent"), "x");
          run_loop->Quit();
        }
      },
      &run_loop));

  auto handle = http2_server.StartAndReturnHandle();

  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = "x";
  // Do NOT set `ua_metadata_override`, so the UA-CH headers are *removed*
  ua_override.ua_metadata_override = std::nullopt;
  UserAgentInjector injector(shell()->web_contents(), ua_override);
  EXPECT_TRUE(NavigateToURL(shell(), http2_server.GetURL("/empty.html")));

  run_loop.Run();
  // This test fails if the browser hangs
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(nullptr);
}

class WebContentsImplBrowserTestReduceAcceptLanguageOn
    : public WebContentsImplBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {network::features::kReduceAcceptLanguage}, {});
    WebContentsImplBrowserTest::SetUp();
  }

  void VerifyAcceptLanguageHeader(net::EmbeddedTestServer& http_server) {
    ReduceAcceptLanguageControllerDelegate* delegate =
        ShellContentBrowserClient::Get()
            ->browser_context()
            ->GetReduceAcceptLanguageControllerDelegate();

    ASSERT_EQ(delegate->GetUserAcceptLanguages(),
              std::vector<std::string>({"en-us", "en"}));

    http_server.ServeFilesFromSourceDirectory("content/test/data");

    base::RunLoop run_loop;
    http_server.RegisterRequestMonitor(base::BindRepeating(
        [](base::RunLoop* run_loop,
           const net::test_server::HttpRequest& request) {
          if (request.relative_url.compare("/empty.html") == 0) {
            // Default mock user language is "en-us,en", see
            // content/shell/browser/shell_content_browser_client.h
            ASSERT_EQ(request.headers.at("Accept-Language"), "en-us,en;q=0.9");
            run_loop->Quit();
          }
        },
        &run_loop));

    auto handle = http_server.StartAndReturnHandle();
    EXPECT_TRUE(NavigateToURL(shell(), http_server.GetURL("/empty.html")));

    run_loop.Run();
  }

  void VerifyPersistAndGetReduceAcceptLanguage(
      const GURL& url,
      const std::string& persist_lang,
      const std::optional<std::string>& expect_lang) {
    ReduceAcceptLanguageControllerDelegate* delegate =
        ShellContentBrowserClient::Get()
            ->browser_context()
            ->GetReduceAcceptLanguageControllerDelegate();

    url::Origin origin = url::Origin::Create(url);
    delegate->PersistReducedLanguage(origin, persist_lang);
    const std::optional<std::string>& language =
        delegate->GetReducedLanguage(origin);
    EXPECT_EQ(expect_lang, language);

    delegate->ClearReducedLanguage(origin);
    EXPECT_FALSE(delegate->GetReducedLanguage(origin).has_value());
  }
};

// Verifies accept-language are updated when DidStartNavigation().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestReduceAcceptLanguageOn,
                       HttpsReduceAcceptLanguageInNavigation) {
  net::EmbeddedTestServer http2_server(
      net::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2);
  VerifyAcceptLanguageHeader(http2_server);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestReduceAcceptLanguageOn,
                       HttpReduceAcceptLanguageInNavigation) {
  net::EmbeddedTestServer http_server_http(net::EmbeddedTestServer::TYPE_HTTP);
  VerifyAcceptLanguageHeader(http_server_http);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestReduceAcceptLanguageOn,
                       PersistAndGetReduceAcceptLanguage) {
  std::string test_lang("en-us");
  VerifyPersistAndGetReduceAcceptLanguage(/*url=*/GURL("https://example.com/"),
                                          /*persist_lang=*/test_lang,
                                          /*expect_lang=*/test_lang);
  VerifyPersistAndGetReduceAcceptLanguage(/*url=*/GURL("http://example.com/"),
                                          /*persist_lang=*/test_lang,
                                          /*expect_lang=*/test_lang);
  VerifyPersistAndGetReduceAcceptLanguage(/*url=*/GURL("ws://example.com/"),
                                          /*persist_lang=*/test_lang,
                                          /*expect_lang=*/std::nullopt);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DialogsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // alert
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  std::string script = "alert('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());

  // confirm
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  script = "confirm('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());

  // prompt
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  script = "prompt('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());

  // beforeunload
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  // Disable the hang monitor (otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer) and give the page a
  // gesture to allow dialogs.
  wc->GetPrimaryMainFrame()->DisableBeforeUnloadHangMonitorForTesting();
  wc->GetPrimaryMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
  script = "window.onbeforeunload=function(e){ return 'x' };";
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(NavigateToURL(shell(), url));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DialogsFromJavaScriptEndFullscreenEvenInInnerWC) {
  WebContentsImpl* top_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen top_test_delegate(top_contents);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = top_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(0U, root->child_count());

  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1U, root->child_count());
  RenderFrameHost* frame = root->child_at(0)->current_frame_host();
  ASSERT_NE(nullptr, frame);

  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(frame));
  TestWCDelegateForDialogsAndFullscreen inner_test_delegate(inner_contents);

  // A dialog from the inner WebContents should make the outer contents lose
  // fullscreen.
  top_contents->EnterFullscreenMode(top_contents->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(top_contents->IsFullscreen());
  script = "alert('hi')";
  inner_test_delegate.WillWaitForDialog();
  EXPECT_TRUE(ExecJs(inner_contents, script));
  inner_test_delegate.Wait();
  EXPECT_FALSE(top_contents->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FileChooserEndsFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());

  auto [chooser, remote] =
      FileChooserImpl::CreateForTesting(wc->GetPrimaryMainFrame());
  wc->RunFileChooser(chooser->GetWeakPtr(), wc->GetPrimaryMainFrame(),
                     base::MakeRefCounted<MockFileSelectListener>(),
                     blink::mojom::FileChooserParams());
  EXPECT_FALSE(wc->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // popup
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  std::string script = "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupsFromJavaScriptDoNotEndFullscreenWithinTab) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // capture
  base::ScopedClosureRunner capture_closure =
      wc->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/false,
                                 /*stay_awake=*/false, /*is_activity=*/true);
  EXPECT_TRUE(wc->IsBeingVisiblyCaptured());
  // popup
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  EXPECT_EQ(wc->GetDelegate()->GetFullscreenState(wc).target_mode,
            FullscreenMode::kPseudoContent);
  std::string script = "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();
  EXPECT_TRUE(wc->IsFullscreen());
  capture_closure.RunAndReset();
}

// Tests that if a popup is opened, a WebContents *up* the opener chain is
// kicked out of fullscreen.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupsOfPopupsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Make a popup.
  std::string popup_script = "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(wc, popup_script));
  test_delegate.Wait();
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(test_delegate.last_popup());

  // Put the original page into fullscreen.
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());

  // Have the popup open a popup.
  TestWCDelegateForDialogsAndFullscreen popup_test_delegate(popup);
  popup_test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(popup, popup_script));
  popup_test_delegate.Wait();

  // Ensure the original page, being in the opener chain, loses fullscreen.
  EXPECT_FALSE(wc->IsFullscreen());
}

// Tests that if a popup is opened, a WebContents *down* the opener chain is
// kicked out of fullscreen.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupsFromJavaScriptEndFullscreenDownstream) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Make a popup.
  std::string popup_script = "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(wc, popup_script));
  test_delegate.Wait();
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(test_delegate.last_popup());

  // Put the popup into fullscreen.
  TestWCDelegateForDialogsAndFullscreen popup_test_delegate(popup);
  popup->EnterFullscreenMode(popup->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(popup->IsFullscreen());

  // Have the original page open a new popup.
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(wc, popup_script));
  test_delegate.Wait();

  // Ensure the popup, being downstream from the opener, loses fullscreen.
  EXPECT_FALSE(popup->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       FocusFromJavaScriptEndsFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Make a popup.
  std::string script =
      "window.FocusFromJavaScriptEndsFullscreen = "
      "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();

  // Put the main contents into fullscreen ...
  wc->EnterFullscreenMode(wc->GetPrimaryMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());

  // ... and ensure that a call to window.focus() from it causes loss of
  // ... fullscreen.
  script = "window.FocusFromJavaScriptEndsFullscreen.focus()";
  test_delegate.WillWaitForFullscreenExit();
  EXPECT_TRUE(ExecJs(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       FileChooserBlockedFromHiddenWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()->set_hold_file_chooser();

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->WasHidden();
  EXPECT_EQ(shell()->web_contents()->GetVisibility(), Visibility::HIDDEN);

  auto [chooser, remote] =
      FileChooserImpl::CreateForTesting(wc->GetPrimaryMainFrame());
  auto file_select_listener = base::MakeRefCounted<MockFileSelectListener>();
  wc->RunFileChooser(chooser->GetWeakPtr(), wc->GetPrimaryMainFrame(),
                     file_select_listener, blink::mojom::FileChooserParams());
  EXPECT_TRUE(file_select_listener->cancelled());
  EXPECT_EQ(shell()->run_file_chooser_count(), 0u);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       EnumerateDirectoryBlockedFromHiddenWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()->set_hold_file_chooser();

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->WasHidden();
  EXPECT_EQ(shell()->web_contents()->GetVisibility(), Visibility::HIDDEN);

  auto [chooser, remote] =
      FileChooserImpl::CreateForTesting(wc->GetPrimaryMainFrame());
  auto file_select_listener = base::MakeRefCounted<MockFileSelectListener>();
  wc->EnumerateDirectory(chooser->GetWeakPtr(), wc->GetPrimaryMainFrame(),
                         file_select_listener, base::FilePath());
  EXPECT_TRUE(file_select_listener->cancelled());
  EXPECT_EQ(shell()->run_file_chooser_count(), 0u);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NewWindowBlockedForActiveFileChooser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()->set_hold_file_chooser();

  GURL url = embedded_test_server()->GetURL("/click-noreferrer-links.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  auto [chooser, remote] =
      FileChooserImpl::CreateForTesting(wc->GetPrimaryMainFrame());
  auto file_select_listener = base::MakeRefCounted<MockFileSelectListener>();
  wc->RunFileChooser(chooser->GetWeakPtr(), wc->GetPrimaryMainFrame(),
                     file_select_listener, blink::mojom::FileChooserParams());
  EXPECT_FALSE(file_select_listener->cancelled());

  // Open a new, named window.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "window.open blocked due to active file chooser.");
  EXPECT_TRUE(ExecJs(shell(), "window.open('about:blank','new_window');"));
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1u, Shell::windows().size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       FrameDetachInCopyDoesNotCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("a.com", "/detach_frame_in_copy.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // Focus the child frame before sending it a copy command: the child frame
  // will detach itself upon getting a 'copy' event.
  ASSERT_TRUE(ExecJs(web_contents, "window[0].focus();"));
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(root->child_at(0),
            web_contents->GetPrimaryFrameTree().GetFocusedFrame());
  shell()->web_contents()->Copy();

  TitleWatcher title_watcher(web_contents, u"done");
  std::u16string title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, u"done");
}

class UpdateTargetURLWaiter : public WebContentsDelegate {
 public:
  explicit UpdateTargetURLWaiter(WebContents* web_contents) {
    web_contents->SetDelegate(this);
  }

  UpdateTargetURLWaiter(const UpdateTargetURLWaiter&) = delete;
  UpdateTargetURLWaiter& operator=(const UpdateTargetURLWaiter&) = delete;

  const GURL& WaitForUpdatedTargetURL() {
    if (updated_target_url_.has_value())
      return updated_target_url_.value();

    runner_ = new MessageLoopRunner();
    runner_->Run();
    return updated_target_url_.value();
  }

 private:
  void UpdateTargetURL(WebContents* source, const GURL& url) override {
    updated_target_url_ = url;
    if (runner_.get())
      runner_->QuitClosure().Run();
  }

  std::optional<GURL> updated_target_url_;
  scoped_refptr<MessageLoopRunner> runner_;
};

// Verifies that focusing a link in a cross-site frame will correctly tell
// WebContentsDelegate to show a link status bubble.  This is a regression test
// for https://crbug.com/807776.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, UpdateTargetURL) {
  // Navigate to a test page.
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* subframe =
      web_contents->GetPrimaryFrameTree().root()->child_at(0);
  GURL subframe_url =
      embedded_test_server()->GetURL("b.com", "/simple_links.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(subframe, subframe_url));

  // Focusing the link should fire the UpdateTargetURL notification.
  UpdateTargetURLWaiter target_url_waiter(web_contents);
  EXPECT_TRUE(
      ExecJs(subframe, "document.getElementById('cross_site_link').focus();"));
  EXPECT_EQ(GURL("http://foo.com/title2.html"),
            target_url_waiter.WaitForUpdatedTargetURL());
}

namespace {

class LoadStateWaiter : public WebContentsDelegate {
 public:
  explicit LoadStateWaiter(content::WebContents* contents)
      : web_contents_(contents) {
    contents->SetDelegate(this);
  }
  ~LoadStateWaiter() override = default;
  LoadStateWaiter(const LoadStateWaiter&) = delete;
  LoadStateWaiter& operator=(const LoadStateWaiter&) = delete;

  // Waits until the WebContents changes its LoadStateHost to |host|.
  void Wait(net::LoadState load_state, const std::u16string& host) {
    waiting_host_ = host;
    waiting_state_ = load_state;
    if (!LoadStateMatches(web_contents_)) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
      DCHECK(LoadStateMatches(web_contents_));
    }
  }

  // WebContentsDelegate:
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    if (!quit_closure_)
      return;
    if (!(changed_flags & INVALIDATE_TYPE_LOAD))
      return;
    if (LoadStateMatches(source))
      std::move(quit_closure_).Run();
  }

 private:
  bool LoadStateMatches(content::WebContents* contents) {
    DCHECK(contents == web_contents_);
    return waiting_host_ == contents->GetLoadStateHost() &&
           waiting_state_ == contents->GetLoadState().state;
  }
  base::OnceClosure quit_closure_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::u16string waiting_host_;
  net::LoadState waiting_state_;
};

}  // namespace

// TODO(csharrison,mmenke):  Beef up testing of LoadState a little. In
// particular, check upload progress and check the LoadState param.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DISABLED_UpdateLoadState) {
  std::u16string a_host = url_formatter::IDNToUnicode("a.com");
  std::u16string b_host = url_formatter::IDNToUnicode("b.com");
  std::u16string paused_host = url_formatter::IDNToUnicode("paused.com");

  // Controlled responses for image requests made in the test. They will
  // alternate being the "most interesting" for the purposes of notifying the
  // WebContents.
  auto a_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/a_img");
  auto b_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/b_img");

  LoadStateWaiter waiter(shell()->web_contents());
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* a_frame = web_contents->GetPrimaryFrameTree().root();
  FrameTreeNode* b_frame = a_frame->child_at(0);

  // Start loading the respective resources in each frame.
  auto load_resource = [](FrameTreeNode* frame, const std::string url) {
    const char kLoadResourceScript[] = R"(
      var img = new Image();
      img.src = '%s';
      document.body.appendChild(img);
    )";
    std::string script = base::StringPrintf(kLoadResourceScript, url.c_str());
    EXPECT_TRUE(ExecJs(frame, script));
  };

  // There should be no outgoing requests, so the load state should be empty.
  waiter.Wait(net::LOAD_STATE_IDLE, std::u16string());

  // The |frame_pauser| pauses the navigation after every step. It will only
  // finish by calling WaitForNavigationFinished or ResumeNavigation.
  GURL paused_url(embedded_test_server()->GetURL("paused.com", "/title1.html"));
  TestNavigationManager frame_pauser(web_contents, paused_url);
  const char kLoadFrameScript[] = R"(
    var frame = document.createElement('iframe');
    frame.src = "%s";
    document.body.appendChild(frame);
  )";
  EXPECT_TRUE(
      ExecJs(web_contents,
             base::StringPrintf(kLoadFrameScript, paused_url.spec().c_str())));

  // Wait for the response to be ready, but never finish it.
  EXPECT_TRUE(frame_pauser.WaitForResponse());
  EXPECT_FALSE(frame_pauser.was_successful());
  // Note: the pausing only works for the non-network service path because of
  // http://crbug.com/791049.
  waiter.Wait(net::LOAD_STATE_IDLE, std::u16string());

  load_resource(a_frame, "/a_img");
  a_response->WaitForRequest();
  waiter.Wait(net::LOAD_STATE_WAITING_FOR_RESPONSE, a_host);

  // Start loading b_img and have it pass a_img by providing one byte of data.
  load_resource(b_frame, "/b_img");
  b_response->WaitForRequest();

  const char kPartialResponse[] = "HTTP/1.1 200 OK\r\n\r\nx";
  b_response->Send(kPartialResponse);
  waiter.Wait(net::LOAD_STATE_READING_RESPONSE, b_host);

  // Finish b_img and expect that a_img is back to being most interesting.
  b_response->Done();
  waiter.Wait(net::LOAD_STATE_WAITING_FOR_RESPONSE, a_host);

  // Advance and finish a_img.
  a_response->Send(kPartialResponse);
  waiter.Wait(net::LOAD_STATE_READING_RESPONSE, a_host);
  a_response->Done();
}

namespace {

// Watches if all title changes in the WebContents match the expected title
// changes, in the order given.
class TitleChecker : public WebContentsDelegate {
 public:
  explicit TitleChecker(content::WebContents* contents,
                        std::queue<std::u16string> expected_title_changes)
      : web_contents_(contents),
        expected_title_changes_(expected_title_changes) {
    contents->SetDelegate(this);
  }
  ~TitleChecker() override = default;
  TitleChecker(const LoadStateWaiter&) = delete;
  TitleChecker& operator=(const TitleChecker&) = delete;

  // Waits until the WebContents has gone through all the titles in
  // `expected_title_changes_`.
  void WaitForAllTitles() {
    if (expected_title_changes_.empty())
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // WebContentsDelegate:
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    // We only care about title changes.
    if (!(changed_flags & INVALIDATE_TYPE_TITLE))
      return;
    // See if this title change is the next thing on the queue.
    DCHECK(!expected_title_changes_.empty());
    DCHECK_EQ(expected_title_changes_.front(), source->GetTitle());
    expected_title_changes_.pop();
    // If `expected_title_changes_` is empty, we have gone through all the
    // expected title changes.
    if (quit_closure_ && expected_title_changes_.empty())
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::queue<std::u16string> expected_title_changes_;
};

}  // namespace

// Tests that restoring a NavigationEntry on a new tab restores the title
// correctly after the page is parsed again, and that the empty title update
// from the initial empty document created by the new tab won't affect anything
// (won't change the restored NavigationEntry's title or the WebContents' title)
// as the tab is not on the initial NavigationEntry.
// Regression test for https://crbug.com/1275392.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, TitleUpdateOnRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  std::u16string main_title = u"Title Of Awesomeness";
  std::u16string main_url_as_title = url_formatter::FormatUrl(main_url);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  // Before any navigation is initiated, the WebContents starts with an empty
  // title.
  EXPECT_EQ(u"", web_contents->GetTitle());

  // Set up all the expected title change in the original WebContents.
  std::queue<std::u16string> original_expected_title_changes;
  // The first "title change" is not an actual title change, it's triggered by a
  // INVALIDATE_TYPE_ALL NotifyNavigationStateChanged call from
  // NavigationControllerImpl::DiscardNonCommittedEntries().
  original_expected_title_changes.push(u"");
  // When the navigation to `main_url` commits, the document title is not set
  // yet, so we use the URL as the title.
  original_expected_title_changes.push(main_url_as_title);
  // Finally, after the committed `main_url` document finished parsing, the
  // final title is set.
  original_expected_title_changes.push(main_title);
  TitleChecker original_web_contents_title_checker(
      web_contents, original_expected_title_changes);

  // Navigate to the page with the expected title.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  original_web_contents_title_checker.WaitForAllTitles();
  EXPECT_EQ(main_title, web_contents->GetTitle());

  // Create a NavigationEntry with the same PageState and title as the last
  // committed entry. We are simulating the condition where the restore has
  // started, but the empty title update from the initial empty document created
  // when the new tab is created came later. When this happens, we already have
  // the restored entry as the "last committed entry" and uses the entry's title
  // (or URL) as the WebContents title. The initial empty document's title
  // update is not for the restored entry, so we should not use it to overwrite
  // the title of the restored entry and the WebContents.
  NavigationControllerImpl& controller = web_contents->GetController();
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url, Referrer(), /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  NavigationEntryRestoreContextImpl context;
  restored_entry->SetPageState(
      controller.GetLastCommittedEntry()->GetPageState(), &context);
  restored_entry->SetTitle(controller.GetLastCommittedEntry()->GetTitle());

  // Create a new tab.
  Shell* new_shell = Shell::CreateNewWindow(controller.GetBrowserContext(),
                                            GURL(), nullptr, gfx::Size());
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  // Before the restore is initiated, the WebContents starts with an empty
  // title.
  EXPECT_EQ(u"", new_contents->GetTitle());

  // Set up all the expected title change in the new WebContents.
  std::queue<std::u16string> new_expected_title_changes;
  // Similar to the original WebContents' case above, the first "title change"
  // is not an actual title change, but instead triggered by a
  // INVALIDATE_TYPE_ALL NotifyNavigationStateChanged call from
  // NavigationControllerImpl::DiscardNonCommittedEntries(). For the
  // original WebContents' case we expect an empty title because there's no
  // entries and GetNavigationEntryForTitle() returns null. However, in the new
  // WebContents we already have the restored entry, so we will use the entry's
  // title.
  new_expected_title_changes.push(main_title);
  // When the navigation to `main_url` commits, we also got another "update"
  // that is not really a title change, but it is triggered by a
  // INVALIDATE_TYPE_ALL NotifyNavigationStateChanged call from
  // NavigationControllerImpl::NotifyNavigationEntryCommitted().
  new_expected_title_changes.push(main_title);
  // Finally, after the committed `main_url` document finished parsing again,
  // the final title is set, but since the title didn't actually change, no
  // "title change" call was dispatched.
  TitleChecker new_web_contents_title_checker(new_contents,
                                              new_expected_title_changes);

  // Restore the new entry in the new tab.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
  NavigationControllerImpl& new_controller = new_contents->GetController();
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());
  // Load the restored entry.
  {
    TestNavigationObserver restore_observer(new_contents);
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }

  // Test that the URL and title are restored correctly.
  new_web_contents_title_checker.WaitForAllTitles();
  EXPECT_EQ(main_url, new_root->current_url());
  EXPECT_EQ(main_title, new_contents->GetTitle());
}

namespace {

class OutgoingSetRendererPrefsMojoWatcher {
 public:
  explicit OutgoingSetRendererPrefsMojoWatcher(RenderViewHostImpl* rvh)
      : rvh_(rvh), outgoing_message_seen_(false) {
    rvh_->SetWillSendRendererPreferencesCallbackForTesting(base::BindRepeating(
        &OutgoingSetRendererPrefsMojoWatcher::OnRendererPreferencesSent,
        base::Unretained(this)));
  }
  ~OutgoingSetRendererPrefsMojoWatcher() {
    rvh_->SetWillSendRendererPreferencesCallbackForTesting(
        base::RepeatingCallback<void(const blink::RendererPreferences&)>());
  }

  void WaitForIPC() {
    if (outgoing_message_seen_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  const blink::RendererPreferences& renderer_preferences() const {
    return renderer_preferences_;
  }

 private:
  void OnRendererPreferencesSent(
      const blink::RendererPreferences& preferences) {
    outgoing_message_seen_ = true;
    renderer_preferences_ = preferences;
    if (run_loop_)
      run_loop_->Quit();
  }

  raw_ptr<RenderViewHostImpl> rvh_;
  bool outgoing_message_seen_;
  std::unique_ptr<base::RunLoop> run_loop_;
  blink::RendererPreferences renderer_preferences_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, SyncRendererPrefs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Navigate to a site with two iframes in different origins.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Retrieve an arbitrary renderer preference.
  blink::RendererPreferences* renderer_preferences =
      web_contents->GetMutableRendererPrefs();
  const bool use_custom_colors_old = renderer_preferences->use_custom_colors;

  // Retrieve all unique render view hosts.
  std::vector<RenderViewHostImpl*> render_view_hosts;
  for (FrameTreeNode* frame_tree_node :
       web_contents->GetPrimaryFrameTree().Nodes()) {
    RenderViewHostImpl* render_view_host = static_cast<RenderViewHostImpl*>(
        frame_tree_node->current_frame_host()->GetRenderViewHost());
    ASSERT_NE(nullptr, render_view_host);
    DLOG(INFO) << "render_view_host=" << render_view_host;

    // Multiple frame hosts can be associated to the same RenderViewHost.
    if (!base::Contains(render_view_hosts, render_view_host)) {
      render_view_hosts.push_back(render_view_host);
    }
  }

  // Set up watchers for SetRendererPreferences message being sent from unique
  // render process hosts.
  std::vector<std::unique_ptr<OutgoingSetRendererPrefsMojoWatcher>>
      mojo_watchers;
  for (auto* render_view_host : render_view_hosts) {
    mojo_watchers.push_back(
        std::make_unique<OutgoingSetRendererPrefsMojoWatcher>(
            render_view_host));

    // Make sure the Mojo watchers have the same default value for the arbitrary
    // preference.
    EXPECT_EQ(use_custom_colors_old,
              mojo_watchers.back()->renderer_preferences().use_custom_colors);
  }

  // Change the arbitrary renderer preference.
  const bool use_custom_colors_new = !use_custom_colors_old;
  renderer_preferences->use_custom_colors = use_custom_colors_new;
  web_contents->SyncRendererPrefs();

  // Ensure Mojo messages are sent to each frame.
  for (auto& mojo_watcher : mojo_watchers) {
    mojo_watcher->WaitForIPC();
    EXPECT_EQ(use_custom_colors_new,
              mojo_watcher->renderer_preferences().use_custom_colors);
  }

  renderer_preferences->use_custom_colors = use_custom_colors_old;
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, SetPageFrozen) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/pause_schedule_task.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  int text_length;
  while (true) {
    text_length =
        EvalJs(shell(), "document.getElementById('textfield').value.length")
            .ExtractInt();

    // Wait until |text_length| exceed 0.
    if (text_length > 0)
      break;
  }

  // Freeze the blink page.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(true);

  // Make the javascript work.
  for (int i = 0; i < 10; i++) {
    text_length =
        EvalJs(shell(), "document.getElementById('textfield').value.length")
            .ExtractInt();
  }

  // Check if |next_text_length| is equal to |text_length|.
  int next_text_length =
      EvalJs(shell(), "document.getElementById('textfield').value.length")
          .ExtractInt();
  EXPECT_EQ(text_length, next_text_length);

  // Wake the frozen page up.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(false);

  // Wait for an amount of time in order to give the javascript time to
  // work again. If the javascript doesn't work again, the test will fail due to
  // the time out.
  while (true) {
    next_text_length =
        EvalJs(shell(), "document.getElementById('textfield').value.length")
            .ExtractInt();
    if (next_text_length > text_length)
      break;
  }

  // Check if |next_text_length| exceeds |text_length| because the blink
  // schedule tasks have resumed.
  next_text_length =
      EvalJs(shell(), "document.getElementById('textfield').value.length")
          .ExtractInt();
  EXPECT_GT(next_text_length, text_length);
}

// Checks that UnfreezableFrameMsg IPCs are executed even when the page is
// frozen.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FrozenAndUnfrozenIPC) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->current_frame_host();

  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_a->child_at(1)->current_frame_host();
  RenderFrameDeletedObserver delete_rfh_b(rfh_b);
  RenderFrameDeletedObserver delete_rfh_c(rfh_c);

  // Delete an iframe when the page is active(not frozen), which should succeed.
  rfh_b->GetMojomFrameInRenderer()->Delete(
      mojom::FrameDeleteIntention::kNotMainFrame);
  delete_rfh_b.WaitUntilDeleted();
  EXPECT_TRUE(delete_rfh_b.deleted());
  EXPECT_FALSE(delete_rfh_c.deleted());

  // Freeze the blink page.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(true);

  // Try to delete an iframe, and succeeds because the message is unfreezable.
  rfh_c->GetMojomFrameInRenderer()->Delete(
      mojom::FrameDeleteIntention::kNotMainFrame);
  delete_rfh_c.WaitUntilDeleted();
  EXPECT_TRUE(delete_rfh_c.deleted());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuppressedPopupWindowBrowserNavResumeLoad) {
  // This test verifies a suppressed pop up that requires navigation from
  // browser side works with a delegate that delays navigations of pop ups.
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  base::FilePath simple_links_path =
      test_data_dir.Append(GetTestDataFilePath())
          .Append(FILE_PATH_LITERAL("simple_links.html"));
  GURL url("file://" + simple_links_path.AsUTF8Unsafe());

  shell()->set_delay_popup_contents_delegate_for_testing(true);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  Shell* new_shell = nullptr;
  WebContents* new_contents = nullptr;
  {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(shell(), "clickLinkToSelfNoOpener();"));
    new_shell = new_shell_observer.GetShell();
    new_contents = new_shell->web_contents();
    // Delaying popup holds the initial load of |url|.
    EXPECT_TRUE(WaitForLoadStop(new_contents));
    EXPECT_TRUE(new_contents->GetController()
                    .GetLastCommittedEntry()
                    ->IsInitialEntry());
    EXPECT_NE(url, new_contents->GetLastCommittedURL());
  }

  EXPECT_FALSE(new_contents->GetDelegate());
  new_contents->SetDelegate(new_shell);
  EXPECT_TRUE(
      static_cast<WebContentsImpl*>(new_contents)->delayed_load_url_params_);
  new_contents->ResumeLoadingCreatedWebContents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_EQ(url, new_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupWindowBrowserNavResumeLoad) {
  // This test verifies a pop up that requires navigation from browser side
  // works with a delegate that delays navigations of pop ups.
  // Create a file: scheme non-suppressed pop up from a file: scheme page will
  // be blocked and wait for the renderer to signal.
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  base::FilePath simple_links_path =
      test_data_dir.Append(GetTestDataFilePath())
          .Append(FILE_PATH_LITERAL("simple_links.html"));
  GURL url("file://" + simple_links_path.AsUTF8Unsafe());

  shell()->set_delay_popup_contents_delegate_for_testing(true);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  Shell* new_shell = nullptr;
  WebContents* new_contents = nullptr;
  {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(shell(), "clickLinkToSelf();"));
    new_shell = new_shell_observer.GetShell();
    new_contents = new_shell->web_contents();
    // Delaying popup holds the initial load of |url|.
    EXPECT_TRUE(WaitForLoadStop(new_contents));
    EXPECT_TRUE(new_contents->GetController()
                    .GetLastCommittedEntry()
                    ->IsInitialEntry());
    EXPECT_NE(url, new_contents->GetLastCommittedURL());
  }

  EXPECT_FALSE(new_contents->GetDelegate());
  new_contents->SetDelegate(new_shell);
  EXPECT_FALSE(
      static_cast<WebContentsImpl*>(new_contents)->delayed_load_url_params_);
  EXPECT_FALSE(
      static_cast<WebContentsImpl*>(new_contents)->delayed_open_url_params_);
  EXPECT_TRUE(static_cast<WebContentsImpl*>(new_contents)->is_resume_pending_);
  new_contents->ResumeLoadingCreatedWebContents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_EQ(url, new_contents->GetLastCommittedURL());
}

namespace {

class FullscreenWebContentsObserver : public WebContentsObserver {
 public:
  FullscreenWebContentsObserver(WebContents* web_contents,
                                RenderFrameHost* wanted_rfh)
      : WebContentsObserver(web_contents), wanted_rfh_(wanted_rfh) {}

  FullscreenWebContentsObserver(const FullscreenWebContentsObserver&) = delete;
  FullscreenWebContentsObserver& operator=(
      const FullscreenWebContentsObserver&) = delete;

  // WebContentsObserver override.
  void DidAcquireFullscreen(RenderFrameHost* rfh) override {
    EXPECT_EQ(wanted_rfh_, rfh);
    EXPECT_FALSE(found_value_);

    if (rfh == wanted_rfh_) {
      found_value_ = true;
      run_loop_.Quit();
    }
  }

  void Wait() {
    if (!found_value_)
      run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  bool found_value_ = false;
  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> wanted_rfh_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, NotifyFullscreenAcquired) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b{allowfullscreen})");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* main_frame = web_contents->GetPrimaryMainFrame();
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));

  std::set<raw_ptr<RenderFrameHostImpl, SetExperimental>> fullscreen_frames;
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(main_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecJs(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(child_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(child_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);

  // Exit fullscreen on the child frame.
  // This will not work with --site-per-process until crbug.com/617369
  // is fixed.
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    {
      FullscreenWebContentsObserver observer(web_contents, main_frame);
      EXPECT_TRUE(ExecJs(child_frame, "document.webkitExitFullscreen();"));
      observer.Wait();
    }

    fullscreen_frames.erase(child_frame);
    EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
    EXPECT_EQ(main_frame->GetGlobalId(),
              web_contents->current_fullscreen_frame_id_);
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, RejectFullscreenIfBlocked) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* main_frame = web_contents->GetPrimaryMainFrame();

  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.onfullscreenchange = "
             "function (event) { document.title = 'onfullscreenchange' };"));
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.onfullscreenerror = "
             "function (event) { document.title = 'onfullscreenerror' };"));

  TitleWatcher title_watcher(web_contents, u"onfullscreenchange");
  title_watcher.AlsoWaitForTitle(u"onfullscreenerror");

  // While the |fullscreen_block| is in scope, fullscreen should fail with an
  // error.
  base::ScopedClosureRunner fullscreen_block =
      web_contents->ForSecurityDropFullscreen(
          /*display_id=*/display::kInvalidDisplayId);

  EXPECT_TRUE(ExecJs(main_frame, "document.body.requestFullscreen();",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  std::u16string title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, u"onfullscreenerror");
}

// Regression test for https://crbug.com/855018.
// RenderFrameHostImpls exit fullscreen as soon as they are unloaded.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FullscreenAfterFrameUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  // 1) Navigate. There is initially no fullscreen frame.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
  EXPECT_EQ(0u, web_contents->fullscreen_frames_.size());

  // 2) Make it fullscreen.
  FullscreenWebContentsObserver observer(web_contents, main_frame);
  EXPECT_TRUE(ExecJs(main_frame, "document.body.webkitRequestFullscreen();"));
  observer.Wait();
  EXPECT_EQ(1u, web_contents->fullscreen_frames_.size());

  // 3) Navigate cross origin. Act as if the old frame was very slow delivering
  //    the unload ack and stayed in pending deletion for a while. Even if the
  //    frame is still present, it must be removed from the list of frame in
  //    fullscreen immediately.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  main_frame->SetUnloadACKCallbackForTesting(unload_ack_filter);
  main_frame->DisableUnloadTimerForTesting();
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(0u, web_contents->fullscreen_frames_.size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NotifyFullscreenAcquired_Navigate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);
  test_delegate.WillWaitForFullscreenExit();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b{allowfullscreen})");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* main_frame = web_contents->GetPrimaryMainFrame();
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));

  std::set<raw_ptr<RenderFrameHostImpl, SetExperimental>> nodes;
  EXPECT_EQ(nodes, web_contents->fullscreen_frames_);
  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  nodes.insert(main_frame);
  EXPECT_EQ(nodes, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecJs(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  nodes.insert(child_frame);
  EXPECT_EQ(nodes, web_contents->fullscreen_frames_);
  EXPECT_EQ(child_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);

  // Perform a cross origin navigation on the main frame.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(
                                 "c.com", "/cross_site_iframe_factory.html")));
  EXPECT_EQ(0u, web_contents->fullscreen_frames_.size());
  EXPECT_FALSE(IsInFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NotifyFullscreenAcquired_SameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a{allowfullscreen})");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* main_frame = web_contents->GetPrimaryMainFrame();
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));

  std::set<raw_ptr<RenderFrameHostImpl, SetExperimental>> fullscreen_frames;
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(main_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecJs(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(child_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(child_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);

  // Exit fullscreen on the child frame.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(child_frame, "document.webkitExitFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.erase(child_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame->GetGlobalId(),
            web_contents->current_fullscreen_frame_id_);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, PropagateFullscreenOptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);

  GURL url = embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents, url));
  RenderFrameHostImpl* main_frame = web_contents->GetPrimaryMainFrame();

  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen with system navigation ui.
  {
    test_delegate.WillWaitForFullscreenEnter();
    TitleWatcher title_watcher(web_contents, u"main_fullscreen_fulfilled");
    EXPECT_TRUE(ExecJs(
        main_frame,
        "document.body.requestFullscreen({ navigationUI: 'show' }).then(() => "
        "{document.title = 'main_fullscreen_fulfilled'});"));
    test_delegate.Wait();

    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"main_fullscreen_fulfilled");
  }

  EXPECT_TRUE(test_delegate.fullscreen_options().prefers_navigation_bar);

  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));
  // Make the child frame fullscreen without system navigation ui.
  {
    test_delegate.WillWaitForFullscreenOption();
    TitleWatcher title_watcher(web_contents, u"child_fullscreen_fulfilled");
    EXPECT_TRUE(ExecJs(
        child_frame,
        "document.body.requestFullscreen({ navigationUI: 'hide' }).then(() => "
        "{parent.document.title = 'child_fullscreen_fulfilled'});"));
    test_delegate.Wait();

    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"child_fullscreen_fulfilled");
  }

  EXPECT_FALSE(test_delegate.fullscreen_options().prefers_navigation_bar);

  // Exit fullscreen on the child frame and restore system navigation ui for the
  // top page.
  {
    test_delegate.WillWaitForFullscreenOption();
    EXPECT_TRUE(ExecJs(
        main_frame,
        "document.body.onfullscreenchange = "
        "function (event) { document.title = 'main_in_fullscreen_again' };"));
    TitleWatcher title_watcher(web_contents, u"main_in_fullscreen_again");
    EXPECT_TRUE(ExecJs(child_frame, "document.exitFullscreen();"));
    test_delegate.Wait();

    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"main_in_fullscreen_again");
  }

  EXPECT_TRUE(test_delegate.fullscreen_options().prefers_navigation_bar);
}

// Tests that when toggling EnterFullscreen/ExitFullscreen that each state
// properly synchronizes with the Renderer, fulfilling the Promises. Even when
// there has been no layout changes, such as when the Renderer is already
// embedded in a fullscreen context, with no OS nor Browser control insets.
//
// Also confirms that each state change does not block the subsequent one.
// Finally on Android, which supports full browser ScreenOrientation locks, that
// we can successfully apply the lock.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ToggleFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);

  GURL url = embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents, url));
  RenderFrameHostImpl* main_frame = web_contents->GetPrimaryMainFrame();

  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen with system navigation ui.
  {
    test_delegate.WillWaitForFullscreenEnter();
    TitleWatcher title_watcher(web_contents, u"main_fullscreen_fulfilled");
    EXPECT_TRUE(ExecJs(
        main_frame,
        "document.body.requestFullscreen({ navigationUI: 'show' }).then(() => "
        "{document.title = 'main_fullscreen_fulfilled'});"));
    test_delegate.Wait();

    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"main_fullscreen_fulfilled");
  }
  EXPECT_TRUE(IsInFullscreen());

  // Full document orientation lock is only available on Android.
#if BUILDFLAG(IS_ANDROID)
  {
    TitleWatcher title_watcher(web_contents, u"portrait_lock_fulfilled");
    EXPECT_TRUE(ExecJs(main_frame,
                       "screen.orientation.lock('portrait').then(() => "
                       "{document.title = 'portrait_lock_fulfilled'});"));
    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"portrait_lock_fulfilled");
  }
#endif

  // Exiting fullscreen should update the title. This should not block
  // subsequent request to re-enter fullscreen.
  {
    test_delegate.WillWaitForFullscreenExit();
    TitleWatcher title_watcher(web_contents, u"main_exit_fullscreen_fulfilled");
    EXPECT_TRUE(
        ExecJs(main_frame,
               "document.exitFullscreen().then(() => "
               "{document.title = 'main_exit_fullscreen_fulfilled'});"));
    test_delegate.Wait();

    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"main_exit_fullscreen_fulfilled");
  }

  // Make the top page fullscreen with system navigation ui.
  {
    test_delegate.WillWaitForFullscreenEnter();
    TitleWatcher title_watcher(web_contents, u"main_fullscreen_fulfilled");
    EXPECT_TRUE(ExecJs(
        main_frame,
        "document.body.requestFullscreen({ navigationUI: 'show' }).then(() => "
        "{document.title = 'main_fullscreen_fulfilled'});"));
    test_delegate.Wait();

    std::u16string title = title_watcher.WaitAndGetTitle();
    ASSERT_EQ(title, u"main_fullscreen_fulfilled");
  }
}

class MockDidOpenRequestedURLObserver : public WebContentsObserver {
 public:
  explicit MockDidOpenRequestedURLObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()) {}

  MockDidOpenRequestedURLObserver(const MockDidOpenRequestedURLObserver&) =
      delete;
  MockDidOpenRequestedURLObserver& operator=(
      const MockDidOpenRequestedURLObserver&) = delete;

  MOCK_METHOD8(DidOpenRequestedURL,
               void(WebContents* new_contents,
                    RenderFrameHost* source_render_frame_host,
                    const GURL& url,
                    const Referrer& referrer,
                    WindowOpenDisposition disposition,
                    ui::PageTransition transition,
                    bool started_from_context_menu,
                    bool renderer_initiated));
};

// Test WebContentsObserver::DidOpenRequestedURL for ctrl-click-ed links.
// This is a regression test for https://crbug.com/864736 (although it also
// covers slightly more ground than just the |is_renderer_initiated| value).
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, CtrlClickSubframeLink) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page with a subframe link.
  GURL main_url(
      embedded_test_server()->GetURL("/ctrl-click-subframe-link.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Start intercepting the DidOpenRequestedURL callback.
  MockDidOpenRequestedURLObserver mock_observer(shell());
  WebContents* new_web_contents1 = nullptr;
  RenderFrameHost* subframe =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_CALL(mock_observer,
              DidOpenRequestedURL(
                  ::testing::_,  // new_contents (captured via SaveArg below)
                  subframe,      // source_render_frame_host
                  embedded_test_server()->GetURL("/title1.html"),
                  ::testing::Field(&Referrer::url, main_url),
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  ::testing::Truly([](ui::PageTransition arg) {
                    return ui::PageTransitionCoreTypeIs(
                        arg, ui::PAGE_TRANSITION_LINK);
                  }),
                  false,  // started_from_context_menu
                  true))  // is_renderer_initiated
      .WillOnce(testing::SaveArg<0>(&new_web_contents1));

  // Simulate a ctrl click on the link and ask GMock to verify that the
  // MockDidOpenRequestedURLObserver got called with the expected args.
  WebContentsAddedObserver new_web_contents_observer;
  EXPECT_TRUE(
      ExecJs(shell(), "window.domAutomationController.send(ctrlClickLink());"));
  WebContents* new_web_contents2 = new_web_contents_observer.GetWebContents();
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_observer));
  EXPECT_EQ(new_web_contents1, new_web_contents2);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, SetVisibilityBeforeLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/hello.html"));

  WebContents* attached_web_contents = shell()->web_contents();

  // Create a WebContents detached from native windows so that visibility of
  // the WebContents is fully controlled by the app.
  WebContents::CreateParams create_params(
      attached_web_contents->GetBrowserContext());
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  EXPECT_EQ(Visibility::VISIBLE, web_contents->GetVisibility());

  web_contents->WasHidden();
  EXPECT_EQ(Visibility::HIDDEN, web_contents->GetVisibility());

  EXPECT_TRUE(NavigateToURL(web_contents.get(), url));
  EXPECT_TRUE(EvalJs(web_contents.get(), "document.hidden").ExtractBool());
}

// This test verifies that if we attach an inner WebContents that has
// descendants in the WebContentsTree, that the descendants also have their
// views registered with the top-level WebContents' InputEventRouter. This
// ensures the descendants will receive events that should be routed to them.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       AttachNestedInnerWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  const GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  auto* root_web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Create a child WebContents but don't attach it to the root contents yet.
  WebContents::CreateParams inner_params(
      root_web_contents->GetBrowserContext());
  std::unique_ptr<WebContents> child_contents_ptr =
      WebContents::Create(inner_params);
  WebContents* child_contents = child_contents_ptr.get();
  // Navigate the child to a page with a subframe, at which we will attach the
  // grandchild.
  ASSERT_TRUE(NavigateToURL(child_contents, url_b));

  // Create and attach grandchild to child.
  std::unique_ptr<WebContents> grandchild_contents_ptr =
      WebContents::Create(inner_params);
  WebContents* grandchild_contents = grandchild_contents_ptr.get();
  RenderFrameHost* child_contents_subframe =
      ChildFrameAt(child_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_contents_subframe);
  child_contents->AttachInnerWebContents(std::move(grandchild_contents_ptr),
                                         child_contents_subframe,
                                         /*is_full_page=*/false);

  // At this point the child hasn't been attached to the root.
  {
    auto* root_view = static_cast<RenderWidgetHostViewBase*>(
        root_web_contents->GetRenderWidgetHostView());
    ASSERT_TRUE(root_view);
    auto* root_event_router = root_web_contents->GetInputEventRouter();
    EXPECT_EQ(1U, root_event_router->RegisteredViewCountForTesting());
    EXPECT_TRUE(root_event_router->IsViewInMap(root_view));
  }

  // Attach child+grandchild subtree to root.
  RenderFrameHost* root_contents_subframe =
      ChildFrameAt(root_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(root_contents_subframe);
  root_web_contents->AttachInnerWebContents(std::move(child_contents_ptr),
                                            root_contents_subframe,
                                            /*is_full_page=*/false);

  // Verify views registered for both child and grandchild.
  {
    auto* root_view = static_cast<RenderWidgetHostViewBase*>(
        root_web_contents->GetRenderWidgetHostView());
    auto* child_view = static_cast<RenderWidgetHostViewBase*>(
        child_contents->GetRenderWidgetHostView());
    auto* grandchild_view = static_cast<RenderWidgetHostViewBase*>(
        grandchild_contents->GetRenderWidgetHostView());
    ASSERT_TRUE(root_view);
    ASSERT_TRUE(child_view);
    ASSERT_TRUE(grandchild_view);
    auto* root_event_router = root_web_contents->GetInputEventRouter();
    EXPECT_EQ(3U, root_event_router->RegisteredViewCountForTesting());
    EXPECT_TRUE(root_event_router->IsViewInMap(root_view));
    EXPECT_TRUE(root_event_router->IsViewInMap(child_view));
    EXPECT_TRUE(root_event_router->IsViewInMap(grandchild_view));
    auto* text_input_manager = root_web_contents->GetTextInputManager();
    ASSERT_TRUE(text_input_manager);
    EXPECT_EQ(3U, text_input_manager->GetRegisteredViewsCountForTesting());
    EXPECT_TRUE(text_input_manager->IsRegistered(root_view));
    EXPECT_TRUE(text_input_manager->IsRegistered(child_view));
    EXPECT_TRUE(text_input_manager->IsRegistered(grandchild_view));
  }
}

// Non-regression test for crbug.com/336843455.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, InnerWebContentsVisibility) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  auto* root_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          ChildFrameAt(root_contents->GetPrimaryMainFrame(), 0)));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  root_contents->WasShown();
  EXPECT_EQ(Visibility::VISIBLE, root_contents->GetVisibility());
  EXPECT_EQ(PageVisibilityState::kVisible,
            root_contents->GetPrimaryMainFrame()->GetVisibilityState());
  EXPECT_EQ(Visibility::VISIBLE, inner_contents->GetVisibility());

  root_contents->WasHidden();
  EXPECT_EQ(Visibility::HIDDEN, root_contents->GetVisibility());
  EXPECT_EQ(PageVisibilityState::kHidden,
            root_contents->GetPrimaryMainFrame()->GetVisibilityState());
  EXPECT_EQ(Visibility::HIDDEN, inner_contents->GetVisibility());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ShutdownDuringSpeculativeNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/hello.html"));

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "b.com" so we are guaranteed to get a different process
    // for navigations to this origin. Doing this ensures that a
    // speculative RenderFrameHost is used.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"b.com"});
  }

  WebContents* attached_web_contents = shell()->web_contents();

  WebContents::CreateParams create_params(
      attached_web_contents->GetBrowserContext());
  std::unique_ptr<WebContents> public_web_contents =
      WebContents::Create(create_params);
  auto* web_contents = static_cast<WebContentsImpl*>(public_web_contents.get());

  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // Complete a navigation.
  GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(web_contents, url1));

  // Start navigating to a second page.
  GURL url2 = embedded_test_server()->GetURL("b.com", "/title2.html");
  TestNavigationManager manager(web_contents, url2);
  web_contents->GetController().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  manager.WaitForSpeculativeRenderFrameHostCreation();

  // While there is a speculative RenderFrameHost in the root FrameTreeNode...
  ASSERT_TRUE(root->render_manager()->speculative_frame_host());

  // Add an observer to ensure that the speculative RenderFrameHost gets
  // deleted.
  RenderFrameDeletedObserver frame_deletion_observer(
      root->render_manager()->speculative_frame_host());

  // ...shutdown the WebContents.
  public_web_contents.reset();

  // What should have happened is the speculative RenderFrameHost deletes the
  // provisional RenderFrame. The |frame_deletion_observer| verifies that this
  // happened.
  EXPECT_TRUE(frame_deletion_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, MouseButtonsNavigate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  {
    TestNavigationObserver back_observer(web_contents);
    web_contents->GetRenderWidgetHostWithPageFocus()->ForwardMouseEvent(
        blink::WebMouseEvent(
            blink::WebInputEvent::Type::kMouseUp, gfx::PointF(51, 50),
            gfx::PointF(51, 50), blink::WebPointerProperties::Button::kBack, 0,
            blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
    back_observer.Wait();
    ASSERT_EQ(url_a, web_contents->GetLastCommittedURL());
  }

  {
    TestNavigationObserver forward_observer(web_contents);
    web_contents->GetRenderWidgetHostWithPageFocus()->ForwardMouseEvent(
        blink::WebMouseEvent(
            blink::WebInputEvent::Type::kMouseUp, gfx::PointF(51, 50),
            gfx::PointF(51, 50), blink::WebPointerProperties::Button::kForward,
            0, blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
    forward_observer.Wait();
    ASSERT_EQ(url_b, web_contents->GetLastCommittedURL());
  }
}

// https://crbug.com/1042128 started flaking after Field Trial Testing Config
// was enabled for content_browsertests. Most likely due to the BFCache
// experiment that got enabled.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_MouseButtonsDontNavigate) {
  // This test injects mouse event listeners in javascript that will
  // preventDefault the action causing the default navigation action not to be
  // taken.

  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Prevent default the action.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.addEventListener('mouseup', "
                     "event => event.preventDefault());"));

  RenderWidgetHostImpl* render_widget_host =
      web_contents->GetRenderWidgetHostWithPageFocus();
  render_widget_host->ForwardMouseEvent(blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseUp, gfx::PointF(51, 50),
      gfx::PointF(51, 50), blink::WebPointerProperties::Button::kBack, 0,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
  RunUntilInputProcessed(render_widget_host);

  // Wait an action timeout and assert the URL is correct.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }
  ASSERT_EQ(url_b, web_contents->GetLastCommittedURL());

  // Move back so we have a forward entry in the history stack so we
  // can test forward getting canceled.
  {
    TestNavigationObserver back_observer(web_contents);
    web_contents->GetController().GoBack();
    back_observer.Wait();
    ASSERT_EQ(url_a, web_contents->GetLastCommittedURL());
  }

  // Now test the forward button by going back, and injecting the prevention
  // script.
  // Prevent default the action.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.addEventListener('mouseup', "
                     "event => event.preventDefault());"));

  render_widget_host = web_contents->GetRenderWidgetHostWithPageFocus();
  render_widget_host->ForwardMouseEvent(blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseUp, gfx::PointF(51, 50),
      gfx::PointF(51, 50), blink::WebPointerProperties::Button::kForward, 0,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
  RunUntilInputProcessed(render_widget_host);
  // Wait an action timeout and assert the URL is correct.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }
  ASSERT_EQ(url_a, web_contents->GetLastCommittedURL());
}

#if !BUILDFLAG(IS_ANDROID)
// https://crbug.com/1402816. This test verifies that when mouse down is on main
// frame and mouse up is on OOF iframe, the mouse up event is delivered to the
// main frame as well to clear cached mouse states including autoscroll
// selection state.
IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTest,
    MouseUpInOOPIframeShouldCancelMainFrameAutoscrollSelection) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL data_url(R"HTML(
    data:text/html,
    <!DOCTYPE html>
    <html lang='en'>
    <head>
    <style>
      %23parentDiv {
        display: flex;
      }
      %23input1 {
        border: 1px solid black;
        margin: 20px;
      }
      %23iframe1 {
        height: 200vh;
      }
    </style>
    </head>
    <body>
      <div id='parentDiv'>
        <iframe src='https://b.com/title1.html' id='iframe1'></iframe>
        <div>
          <input type='text' id='input1'>
        </div>
      </div>
      </body>
      </html>)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), data_url));
  WaitForLoadStop(web_contents);

  const double input_center_y =
      EvalJs(web_contents,
             "document.getElementById('input1').offsetTop + "
             "document.getElementById('input1').offsetHeight / 2")
          .ExtractDouble();
  const double input_center_x =
      EvalJs(web_contents,
             "document.getElementById('input1').offsetLeft + "
             "document.getElementById('input1').offsetWidth / 2")
          .ExtractDouble();

  // Click the input element and start typing.
  SimulateMouseClickAt(web_contents, 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(input_center_x, input_center_y));
  RunUntilInputProcessed(web_contents->GetRenderWidgetHostWithPageFocus());
  EXPECT_TRUE(ExecJs(web_contents,
                     "var inputElement = document.getElementById('input1');"
                     "new Promise(function(resolve) {"
                     "  if (document.activeElement == inputElement)"
                     "    resolve(true);"
                     "  inputElement.addEventListener('focus', () => {"
                     "    resolve(true);"
                     "  });"
                     "});"));
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('A'),
                   ui::DomCode::US_A, ui::VKEY_A, false, false, false, false);
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('B'),
                   ui::DomCode::US_B, ui::VKEY_B, false, false, false, false);
  RunUntilInputProcessed(web_contents->GetRenderWidgetHostWithPageFocus());
  EXPECT_TRUE(ExecJs(web_contents,
                     "var inputElement = document.getElementById('input1');"
                     "new Promise(function(resolve) {"
                     "  if (inputElement.value == 'AB')"
                     "    resolve(true);"
                     "  inputElement.addEventListener('change', () => {"
                     "    if (inputElement.value == 'AB')"
                     "      resolve(true);"
                     "  });"
                     "});"));
  EXPECT_EQ("AB",
            EvalJs(web_contents, "document.getElementById('input1').value")
                .ExtractString());

  EXPECT_TRUE(ExecJs(web_contents,
                     "document.addEventListener('mousedown', () => { "
                     "window.receivedMouseDown = true; });"));
  EXPECT_TRUE(ExecJs(web_contents,
                     "document.addEventListener('mouseup', () => { "
                     "window.receivedMouseUp = true; });"));

  // Now, start the drag from input element to the OOP iframe.
  SimulateMouseEvent(web_contents, blink::WebMouseEvent::Type::kMouseDown,
                     blink::WebMouseEvent::Button::kLeft,
                     gfx::Point(input_center_x, input_center_y));
  SimulateMouseEvent(web_contents, blink::WebMouseEvent::Type::kMouseMove,
                     blink::WebMouseEvent::Button::kLeft,
                     gfx::Point(input_center_x - 5, input_center_y));
  SimulateMouseEvent(web_contents, blink::WebMouseEvent::Type::kMouseMove,
                     blink::WebMouseEvent::Button::kLeft,
                     gfx::Point(input_center_x - 10, input_center_y));

  const double iframe_center_x =
      EvalJs(web_contents,
             "document.getElementById('iframe1').offsetLeft + "
             "document.getElementById('iframe1').offsetWidth / 2")
          .ExtractDouble();

  SimulateMouseEvent(web_contents, blink::WebMouseEvent::Type::kMouseMove,
                     blink::WebMouseEvent::Button::kLeft,
                     gfx::Point(iframe_center_x, input_center_y));
  RunUntilInputProcessed(web_contents->GetRenderWidgetHostWithPageFocus());
  EXPECT_TRUE(ExecJs(web_contents,
                     "new Promise(resolve => setTimeout(() => {"
                     "  resolve(window.receivedMouseDown);"
                     "}));"));
  EXPECT_TRUE(web_contents->GetInputEventRouter()
                  ->root_view_receive_additional_mouse_up_);

  SimulateMouseEvent(web_contents, blink::WebMouseEvent::Type::kMouseUp,
                     blink::WebMouseEvent::Button::kLeft,
                     gfx::Point(iframe_center_x, input_center_y));
  RunUntilInputProcessed(web_contents->GetRenderWidgetHostWithPageFocus());
  // Main frame should receive mouse up event.
  EXPECT_TRUE(ExecJs(web_contents,
                     "new Promise(resolve => setTimeout(() => {"
                     "  resolve(window.receivedMouseUp);"
                     "}));"));
  EXPECT_FALSE(web_contents->GetInputEventRouter()
                   ->root_view_receive_additional_mouse_up_);

  // Type again in input element, insert text should be left to right.
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('E'),
                   ui::DomCode::US_E, ui::VKEY_E, false, false, false, false);
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('F'),
                   ui::DomCode::US_F, ui::VKEY_F, false, false, false, false);
  RunUntilInputProcessed(web_contents->GetRenderWidgetHostWithPageFocus());
  EXPECT_TRUE(ExecJs(web_contents,
                     "var inputElement = document.getElementById('input1');"
                     "new Promise(function(resolve) {"
                     "  if (inputElement.value == 'EF')"
                     "    resolve(true);"
                     "  inputElement.addEventListener('change', () => {"
                     "    if (inputElement.value == 'EF')"
                     "      resolve(true);"
                     "  });"
                     "});"));
  EXPECT_EQ("EF",
            EvalJs(web_contents, "document.getElementById('input1').value")
                .ExtractString());
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FrameCount) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  GURL url_with_iframes =
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_with_iframes));
  shell()->Close();

  // Number of samples should be only one.
  histogram_tester.ExpectTotalCount(kFrameCountUMA, 1);
  histogram_tester.ExpectTotalCount(kMaxFrameCountUMA, 1);

  histogram_tester.ExpectBucketCount(kFrameCountUMA, /* bucket */ 2,
                                     /* count */ 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, 2, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MaxFrameCountForCrossProcessNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 1u);

  GURL url_with_iframes_out_of_process =
      embedded_test_server()->GetURL("b.com", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_with_iframes_out_of_process));
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 2u);

  // There should be two samples for kFrameCountUMA.
  histogram_tester.ExpectTotalCount(kFrameCountUMA, 2);
  histogram_tester.ExpectBucketCount(kFrameCountUMA, /* bucket */ 2,
                                     /* count */ 1);
  histogram_tester.ExpectBucketCount(kFrameCountUMA, /* bucket */ 1,
                                     /* count */ 1);

  // There should be only one record for KMaxFrameCountUMA as it is recorded
  // either when a frame is destroyed or when a new page is loaded.
  histogram_tester.ExpectTotalCount(kMaxFrameCountUMA, 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, /* bucket */ 1,
                                     /* count */ 1);

  // Same site navigation with multiple cross process iframes.
  GURL url_with_multiple_iframes = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a,c(b),d,b)");
  EXPECT_TRUE(NavigateToURL(shell(), url_with_multiple_iframes));
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 6u);

  histogram_tester.ExpectTotalCount(kMaxFrameCountUMA, 2);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, 1, 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, 2, 1);

  // Simulate tab close to check that |kMaxFrameCountUMA| gets recorded.
  shell()->Close();

  // When the shell closes, the web contents is destroyed, as a result the main
  // frame will be destroyed. When the main frame is destroyed, the
  // kMaxFrameCountUMA gets recorded.
  histogram_tester.ExpectTotalCount(kMaxFrameCountUMA, 3);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, 1, 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, 2, 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, 6, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MaxFrameCountInjectedIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::HistogramTester histogram_tester;
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url_with_iframes =
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_with_iframes));
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 2u);

  // |url_with_iframes| contains another iframe inside it. This means that we
  // have 4 iframes inside.
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      CreateSubframe(web_contents, "" /* frame_id */, url_with_iframes,
                     true /* wait_for_navigation */));

  EXPECT_EQ(web_contents->max_loaded_frame_count_, 4u);
  ASSERT_NE(rfh, nullptr);

  shell()->Close();

  // There should be one sample for kFrameCountUMA.
  histogram_tester.ExpectTotalCount(kFrameCountUMA, 1);
  histogram_tester.ExpectBucketCount(kFrameCountUMA, /* bucket */ 2,
                                     /* count */ 1);

  // There should be one sample for kMaxFrameCountUMA.
  histogram_tester.ExpectTotalCount(kMaxFrameCountUMA, 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, /* bucket */ 4u,
                                     /* count */ 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MaxFrameCountRemovedIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::HistogramTester histogram_tester;
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url_with_iframes =
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_with_iframes));
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 2u);

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  auto* rfh = static_cast<RenderFrameHostImpl*>(CreateSubframe(
      web_contents, "" /* frame_id */, url, true /* wait_for_navigation */));
  ;
  ASSERT_NE(rfh, nullptr);
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 3u);

  // Let's remove the first child.
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  auto* node_to_remove = main_frame->child_at(0);
  FrameDeletedObserver observer(node_to_remove->current_frame_host());
  EXPECT_TRUE(ExecJs(main_frame,
                     "document.body.removeChild(document.querySelector('"
                     "iframe').parentNode);"));
  observer.Wait();

  EXPECT_EQ(web_contents->max_loaded_frame_count_, 3u);

  // Let's remove the second child.
  node_to_remove = main_frame->child_at(0);
  FrameDeletedObserver observer_second(node_to_remove->current_frame_host());
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.removeChild(document.querySelector('iframe'));"));
  observer_second.Wait();

  EXPECT_EQ(web_contents->max_loaded_frame_count_, 3u);

  shell()->Close();

  // There should be one sample for kFrameCountUMA.
  histogram_tester.ExpectTotalCount(kFrameCountUMA, 1);
  histogram_tester.ExpectBucketCount(kFrameCountUMA, /* bucket */ 2,
                                     /* count */ 1);

  // There should be one sample for kMaxFrameCountUMA
  histogram_tester.ExpectTotalCount(kMaxFrameCountUMA, 1);
  histogram_tester.ExpectBucketCount(kMaxFrameCountUMA, /* bucket */ 3,
                                     /* count */ 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ForEachRenderFrameHost) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url =
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  // In the absence of any pages besides the primary page (e.g. nothing in
  // bfcache, no prerendered pages), iterating over the RenderFrameHosts of the
  // WebContents would just produce the RenderFrameHosts of the primary page.
  EXPECT_THAT(CollectAllRenderFrameHosts(web_contents),
              testing::ContainerEq(CollectAllRenderFrameHosts(
                  web_contents->GetPrimaryMainFrame())));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ForEachRenderFrameHostInnerContents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  const GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  auto* inner_contents = CreateAndAttachInnerContents(
      web_contents->GetPrimaryMainFrame()->child_at(0)->current_frame_host());
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  // Calling |WebContents::ForEachRenderFrameHost| on an inner contents does not
  // add much value over |RenderFrameHost::ForEachRenderFrameHost|, since we
  // don't have any pages besides the primary page, however for completeness, we
  // allow it to be called and confirm that it just returns the RenderFrameHosts
  // of the primary page.
  EXPECT_THAT(CollectAllRenderFrameHosts(inner_contents),
              testing::ContainerEq(CollectAllRenderFrameHosts(
                  inner_contents->GetPrimaryMainFrame())));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ForEachFrameTreeInnerContents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  const GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  auto* inner_contents = static_cast<WebContentsImpl*>(
      CreateAndAttachInnerContents(web_contents->GetPrimaryMainFrame()
                                       ->child_at(0)
                                       ->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  // Intentionally exclude inner frame trees based on multi-WebContents.
  web_contents->ForEachFrameTree(
      base::BindLambdaForTesting([&](FrameTree& frame_tree) {
        EXPECT_NE(&frame_tree, &inner_contents->GetPrimaryFrameTree());
      }));
}

namespace {

class LoadingObserver : public WebContentsObserver {
 public:
  explicit LoadingObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  std::vector<std::string>& GetEvents() { return events_; }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    events_.push_back("DidStartNavigation");
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    events_.push_back("DidFinishNavigation");
  }

  void DidStartLoading() override { events_.push_back("DidStartLoading"); }

  void DidStopLoading() override {
    events_.push_back("DidStopLoading");
    run_loop_.Quit();
  }

  void PrimaryMainDocumentElementAvailable() override {
    events_.push_back("PrimaryMainDocumentElementAvailable");
  }

  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    events_.push_back("DocumentOnLoadCompletedInPrimaryMainFrame");
  }

  void DOMContentLoaded(RenderFrameHost* render_frame_host) override {
    events_.push_back("DOMContentLoaded");
  }

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    events_.push_back("DidFinishLoad");
  }

  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& url,
                   int error_code) override {
    events_.push_back("DidFailLoad");
  }

  void Wait() { run_loop_.Run(); }

 private:
  std::vector<std::string> events_;
  base::RepeatingClosure completion_callback_;
  base::RunLoop run_loop_;
};

}  // namespace

// These tests provide a reference points for simulating the navigation events
// for unittests.
//
// Keep in sync with TestRenderFrameHostTest.LoadingCallbacksOrder_*.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingCallbacksOrder_CrossDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  LoadingObserver loading_observer(web_contents);

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  loading_observer.Wait();

  EXPECT_THAT(loading_observer.GetEvents(),
              testing::ElementsAre("DidStartLoading", "DidStartNavigation",
                                   "DidFinishNavigation",
                                   "PrimaryMainDocumentElementAvailable",
                                   "DOMContentLoaded",
                                   "DocumentOnLoadCompletedInPrimaryMainFrame",
                                   "DidFinishLoad", "DidStopLoading"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingCallbacksOrder_SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url2 = embedded_test_server()->GetURL("a.com", "/title1.html#foo");

  LoadingObserver loading_observer1(web_contents);
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  loading_observer1.Wait();

  LoadingObserver loading_observer2(web_contents);
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  loading_observer2.Wait();

  EXPECT_THAT(loading_observer2.GetEvents(),
              testing::ElementsAre("DidStartLoading", "DidStartNavigation",
                                   "DidFinishNavigation", "DidStopLoading"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingCallbacksOrder_AbortedNavigation) {
  const char kPageURL[] = "/controlled_page_load.html";
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      kPageURL);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("a.com", kPageURL);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  LoadingObserver loading_observer(web_contents);
  shell()->LoadURL(url);
  response.WaitForRequest();
  response.Send(net::HttpStatusCode::HTTP_NO_CONTENT);
  response.Done();

  loading_observer.Wait();

  EXPECT_THAT(loading_observer.GetEvents(),
              testing::ElementsAre("DidStartLoading", "DidStartNavigation",
                                   "DidFinishNavigation", "DidStopLoading"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingCallbacksOrder_ErrorPage_EmptyBody) {
  const char kPageURL[] = "/controlled_page_load.html";
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      kPageURL);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("a.com", kPageURL);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  LoadingObserver loading_observer(web_contents);
  shell()->LoadURL(url);
  response.WaitForRequest();
  response.Send(net::HttpStatusCode::HTTP_REQUEST_TIMEOUT);
  response.Done();

  loading_observer.Wait();

  EXPECT_THAT(loading_observer.GetEvents(),
              testing::ElementsAre("DidStartLoading", "DidStartNavigation",
                                   "DidFinishNavigation",
                                   "PrimaryMainDocumentElementAvailable",
                                   "DOMContentLoaded",
                                   "DocumentOnLoadCompletedInPrimaryMainFrame",
                                   "DidFinishLoad", "DidStopLoading"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingCallbacksOrder_ErrorPage_NonEmptyBody) {
  const char kPageURL[] = "/controlled_page_load.html";
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      kPageURL);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("a.com", kPageURL);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  LoadingObserver loading_observer(web_contents);
  shell()->LoadURL(url);
  response.WaitForRequest();
  response.Send(net::HTTP_NOT_FOUND, "text/html", "<html><body>foo</body>");
  response.Done();

  loading_observer.Wait();
  EXPECT_THAT(loading_observer.GetEvents(),
              testing::ElementsAre("DidStartLoading", "DidStartNavigation",
                                   "DidFinishNavigation",
                                   "PrimaryMainDocumentElementAvailable",
                                   "DOMContentLoaded",
                                   "DocumentOnLoadCompletedInPrimaryMainFrame",
                                   "DidFinishLoad", "DidStopLoading"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ThemeColorIsResetWhenNavigatingAway) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a(
      embedded_test_server()->GetURL("a.com", "/theme_color.html"));
  const GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(shell()->web_contents()->GetThemeColor(), 0xFFFF0000u);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(shell()->web_contents()->GetThemeColor(), std::nullopt);

  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetThemeColor(), 0xFFFF0000u);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MimeTypeResetWhenNavigatingAway) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url_b(embedded_test_server()->GetURL("b.com", "/single_face.jpg"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(shell()->web_contents()->GetContentsMimeType(), "text/html");

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(shell()->web_contents()->GetContentsMimeType(), "image/jpeg");
}

namespace {

// A WebContentsObserver which caches the total number of calls to
// DidChangeVerticalScrollDirection as well as the most recently provided value.
class DidChangeVerticalScrollDirectionObserver : public WebContentsObserver {
 public:
  explicit DidChangeVerticalScrollDirectionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  DidChangeVerticalScrollDirectionObserver(
      const DidChangeVerticalScrollDirectionObserver&) = delete;
  DidChangeVerticalScrollDirectionObserver& operator=(
      const DidChangeVerticalScrollDirectionObserver&) = delete;

  // WebContentsObserver:
  void DidChangeVerticalScrollDirection(
      viz::VerticalScrollDirection scroll_direction) override {
    ++call_count_;
    last_value_ = scroll_direction;
  }

  int call_count() const { return call_count_; }
  viz::VerticalScrollDirection last_value() const { return last_value_; }

 private:
  int call_count_ = 0;
  viz::VerticalScrollDirection last_value_ =
      viz::VerticalScrollDirection::kNull;
};

}  // namespace

// Tests that DidChangeVerticalScrollDirection is called only when the vertical
// scroll direction has changed and that it includes the correct details.
// TODO(crbug.com/40862270): This is flaky on the Mac10.14 bot.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DidChangeVerticalScrollDirection \
  DISABLED_DidChangeVerticalScrollDirection
#else
#define MAYBE_DidChangeVerticalScrollDirection DidChangeVerticalScrollDirection
#endif
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_DidChangeVerticalScrollDirection) {
  net::EmbeddedTestServer* server = embedded_test_server();
  EXPECT_TRUE(server->Start());

  // Set up the DOM.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL(server->GetURL("/scrollable_page_with_content.html"))));

  // Size our view so that we can scroll both horizontally and vertically while
  // the content is visible.
  ResizeWebContentsView(shell(), gfx::Size(20, 20), /*set_start_page=*/false);

  // Set up observers to watch the web contents and render frame submissions.
  auto* web_contents = shell()->web_contents();
  DidChangeVerticalScrollDirectionObserver web_contents_observer(web_contents);
  RenderFrameSubmissionObserver render_frame_observer(web_contents);

  // Verify that we are starting our test without scroll offset.
  render_frame_observer.WaitForScrollOffset(gfx::PointF());

  // Assert initial state.
  EXPECT_EQ(0, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            web_contents_observer.last_value());

  // Scroll offset is dependent upon device pixel ratio which can vary across
  // devices. To account for this, we extract the |devicePixelRatio| from our
  // content window and for the remainder of this test use a scaled |Vector2dF|,
  // which takes pixel ratio into consideration, when waiting for scroll offset.
  // Note that this is primarily being done to satisfy tests running on Android.
  const double device_pixel_ratio =
      EvalJs(web_contents, "window.devicePixelRatio").ExtractDouble();
  auto ScaledPointF = [device_pixel_ratio](float x, float y) {
    return gfx::PointF(std::floor(x * device_pixel_ratio),
                       std::floor(y * device_pixel_ratio));
  };

  // Scroll down.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5)"));
  render_frame_observer.WaitForScrollOffset(ScaledPointF(0.f, 5.f));

  // Assert that we are notified of the scroll down event.
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll down again.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 10)"));
  render_frame_observer.WaitForScrollOffset(ScaledPointF(0.f, 10.f));

  // Assert that we are *not* notified of the scroll down event given that no
  // change in scroll direction occurred (as our previous scroll was also down).
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll right.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(10, 10)"));
  render_frame_observer.WaitForScrollOffset(ScaledPointF(10.f, 10.f));

  // Assert that we are *not* notified of the scroll right event given that no
  // change occurred in the vertical direction.
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll left.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 10)"));
  render_frame_observer.WaitForScrollOffset(ScaledPointF(0.f, 10.f));

  // Assert that we are *not* notified of the scroll left event given that no
  // change occurred in the vertical direction.
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll up.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5)"));
  render_frame_observer.WaitForScrollOffset(ScaledPointF(0.f, 5.f));

  // Assert that we are notified of the scroll up event.
  EXPECT_EQ(2, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            web_contents_observer.last_value());

  // Scroll up again.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 0)"));
  render_frame_observer.WaitForScrollOffset(ScaledPointF(0.f, 0.f));

  // Assert that we are *not* notified of the scroll up event given that no
  // change in scroll direction occurred (as our previous scroll was also up).
  EXPECT_EQ(2, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            web_contents_observer.last_value());
}

// Tests that DidChangeVerticalScrollDirection is *not* called when the vertical
// scroll direction has changed in a child frame. We expect to only be notified
// of vertical scroll direction changes to the main frame's root layer.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidChangeVerticalScrollDirectionWithIframe) {
  net::EmbeddedTestServer* server = embedded_test_server();
  EXPECT_TRUE(server->Start());

  // Set up the DOM.
  GURL main_url(server->GetURL("a.co", "/scrollable_page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Set up the iframe.
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* iframe =
      web_contents->GetPrimaryFrameTree().root()->child_at(0);
  GURL iframe_url(server->GetURL("b.co", "/scrollable_page_with_content.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe, iframe_url));

  // Size our view so that we can scroll both horizontally and vertically.
  ResizeWebContentsView(shell(), gfx::Size(10, 10), /*set_start_page=*/false);

  // Set up an observer to watch the web contents.
  DidChangeVerticalScrollDirectionObserver web_contents_observer(web_contents);

  // Assert initial state.
  EXPECT_EQ(0, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            web_contents_observer.last_value());

  // Scroll down in the iframe.
  EXPECT_TRUE(ExecJs(iframe->current_frame_host(), "window.scrollTo(0, 10)"));

  // Assert that we are *not* notified of the scroll down event given that the
  // scroll was not performed on the main frame's root layer.
  EXPECT_EQ(0, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            web_contents_observer.last_value());

  // Scroll right in the iframe.
  EXPECT_TRUE(ExecJs(iframe->current_frame_host(), "window.scrollTo(10, 10)"));

  // Assert that we are *not* notified of the scroll right event given that the
  // scroll was not performed on the main frame's root layer.
  EXPECT_EQ(0, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            web_contents_observer.last_value());

  // Scroll left in the iframe.
  EXPECT_TRUE(ExecJs(iframe->current_frame_host(), "window.scrollTo(0, 10)"));

  // Assert that we are *not* notified of the scroll left event given that the
  // scroll was not performed on the main frame's root layer.
  EXPECT_EQ(0, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            web_contents_observer.last_value());

  // Scroll up in the iframe.
  EXPECT_TRUE(ExecJs(iframe->current_frame_host(), "window.scrollTo(0, 0)"));

  // Assert that we are *not* notified of the scroll up event given that the
  // scroll was not performed on the main frame's root layer.
  EXPECT_EQ(0, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            web_contents_observer.last_value());
}

// Verifies assertions for SetRendererInitiatedUserAgentOverrideOption().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RendererInitiatedUserAgentOverride) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* web_contents = shell()->web_contents();

  // This url triggers a renderer initiated navigation (redirect).
  NavigationController::LoadURLParams load_params(
      embedded_test_server()->GetURL("a.co", "/client_redirect.html"));
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;

  const GURL resulting_url =
      embedded_test_server()->GetURL("a.co", "/title1.html");

  // Assertions for the default SetRendererInitiatedUserAgentOverrideOption(),
  // which is UA_OVERRIDE_INHERIT.
  {
    // The 2 is because the url redirects.
    TestNavigationObserver observer(shell()->web_contents(), 2);
    web_contents->GetController().LoadURLWithParams(load_params);
    observer.Wait();

    NavigationEntry* resulting_entry =
        web_contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(resulting_entry);
    EXPECT_EQ(resulting_url, resulting_entry->GetURL());
    // The resulting entry should override the user-agent as the previous
    // entry (as created by |load_params|) was configured to override the
    // user-agent, and the WebContents was configured with
    // SetRendererInitiatedUserAgentOverrideOption() of
    // UA_OVERRIDE_INHERIT.
    EXPECT_TRUE(resulting_entry->GetIsOverridingUserAgent());
  }

  // Repeat the above, but with UA_OVERRIDE_FALSE.
  web_contents->SetRendererInitiatedUserAgentOverrideOption(
      NavigationController::UA_OVERRIDE_FALSE);
  {
    // The 2 is because the url redirects.
    TestNavigationObserver observer(shell()->web_contents(), 2);
    web_contents->GetController().LoadURLWithParams(load_params);
    observer.Wait();

    EXPECT_EQ(2, web_contents->GetController().GetEntryCount());
    NavigationEntry* resulting_entry =
        web_contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(resulting_entry);
    EXPECT_EQ(resulting_url, resulting_entry->GetURL());
    // Even though |load_params| was configured to override the user-agent, the
    // NavigationEntry for the redirect gets an override user-agent value of
    // false because
    // of SetRendererInitiatedUserAgentOverrideOption(UA_OVERRIDE_FALSE).
    EXPECT_FALSE(resulting_entry->GetIsOverridingUserAgent());
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       IgnoreUnresponsiveRendererDuringPaste) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"random pasted text";

  EXPECT_FALSE(web_contents->ShouldIgnoreUnresponsiveRenderer());
  web_contents->IsClipboardPasteAllowedByPolicy(
      ClipboardEndpoint(ui::DataTransferEndpoint(GURL("https://google.com"))),
      ClipboardEndpoint(ui::DataTransferEndpoint(GURL("https://google.com")),
                        base::BindLambdaForTesting([web_contents] {
                          return web_contents->GetBrowserContext();
                        }),
                        *web_contents->GetPrimaryMainFrame()),
      {.format_type = ui::ClipboardFormatType::PlainTextType()},
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [&web_contents](
              std::optional<ClipboardPasteData> clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_TRUE(web_contents->ShouldIgnoreUnresponsiveRenderer());
          }));
  EXPECT_FALSE(web_contents->ShouldIgnoreUnresponsiveRenderer());
}

// Intercept calls to RenderFramHostImpl's DidStopLoading mojo method. The
// caller has to guarantee that `render_frame_host` lives at least as long as
// DidStopLoadingInterceptor.
class DidStopLoadingInterceptor : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit DidStopLoadingInterceptor(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host),
        swapped_impl_(render_frame_host_->frame_host_receiver_for_testing(),
                      this) {}

  ~DidStopLoadingInterceptor() override = default;

  DidStopLoadingInterceptor(const DidStopLoadingInterceptor&) = delete;
  DidStopLoadingInterceptor& operator=(const DidStopLoadingInterceptor&) =
      delete;

  mojom::FrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void DidStopLoading() override {
    static_cast<RenderProcessHostImpl*>(render_frame_host_->GetProcess())
        ->mark_child_process_activity_time();
    static_cast<mojom::FrameHost*>(render_frame_host_)->DidStopLoading();
  }

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  mojo::test::ScopedSwapImplForTesting<mojom::FrameHost> swapped_impl_;
};

// Test that get_process_idle_time() returns reasonable values when compared
// with time deltas measured locally.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, RenderIdleTime) {
  EXPECT_TRUE(embedded_test_server()->Start());

  base::TimeTicks start = base::TimeTicks::Now();
  DidStopLoadingInterceptor interceptor(
      static_cast<content::RenderFrameHostImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame()));

  GURL test_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  base::TimeDelta renderer_td = shell()
                                    ->web_contents()
                                    ->GetPrimaryMainFrame()
                                    ->GetProcess()
                                    ->GetChildProcessIdleTime();
  base::TimeDelta browser_td = base::TimeTicks::Now() - start;
  EXPECT_TRUE(browser_td >= renderer_td);
}

class WebContentsDiscardBrowserTest : public WebContentsImplBrowserTest {
 public:
  WebContentsDiscardBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebContentsDiscard);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebContentsDiscardBrowserTest, DiscardRetainsTitle) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  const GURL initial_url =
      embedded_test_server()->GetURL("/frame_tree/top.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Update the tab title.
  const std::u16string test_title(u"test_title");
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  contents->UpdateTitleForEntry(
      contents->GetController().GetLastCommittedEntry(), test_title);
  EXPECT_EQ(test_title, contents->GetTitle());

  // Discard the tab.
  testing::NiceMock<MockWebContentsObserver> observer(contents);
  EXPECT_CALL(observer, AboutToBeDiscarded(contents)).Times(1);
  EXPECT_CALL(observer, WasDiscarded()).Times(1);
  EXPECT_FALSE(contents->WasDiscarded());
  contents->Discard();
  EXPECT_TRUE(contents->WasDiscarded());
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return 0u == root->child_count(); }));

  // The title should remain unchanged post discard.
  EXPECT_EQ(test_title, contents->GetTitle());
}

// Helper class that waits to receive a favicon from the renderer process.
class FaviconWaiter : public WebContentsObserver {
 public:
  explicit FaviconWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver:
  void DidUpdateFaviconURL(
      RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    received_favicon_ = true;
    run_loop_.Quit();
  }

  void Wait() {
    if (received_favicon_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  bool received_favicon_ = false;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(WebContentsDiscardBrowserTest, DiscardRetainsFavicon) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  const GURL initial_url =
      embedded_test_server()->GetURL("/frame_tree/top.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FaviconWaiter favicon_waiter(contents);

  // Insert a favicon dynamically.
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             "let l = document.createElement('link'); "
             "l.rel='icon'; l.type='image/png'; l.href='single_face.jpg'; "
             "document.head.appendChild(l)"));

  // Wait until it's received by the browser process.
  favicon_waiter.Wait();
  EXPECT_EQ(1u, contents->GetFaviconURLs().size());
  auto favicon_url = contents->GetFaviconURLs()[0]->icon_url;

  // Discard the tab.
  testing::NiceMock<MockWebContentsObserver> observer(contents);
  EXPECT_CALL(observer, AboutToBeDiscarded(contents)).Times(1);
  EXPECT_CALL(observer, WasDiscarded()).Times(1);
  EXPECT_FALSE(contents->WasDiscarded());
  contents->Discard();
  EXPECT_TRUE(contents->WasDiscarded());
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return 0u == root->child_count(); }));

  // The favicon URLs should remain unchanged.
  EXPECT_EQ(1u, contents->GetFaviconURLs().size());
  EXPECT_THAT(favicon_url, contents->GetFaviconURLs()[0]->icon_url);
}

#if !BUILDFLAG(IS_ANDROID)
class WebContentsImplBrowserTestWindowControlsOverlay
    : public WebContentsImplBrowserTest {
 public:
  void ValidateTitlebarAreaCSSValue(const std::string& name,
                                    int expected_result) {
    SCOPED_TRACE(name);
    EXPECT_EQ(
        expected_result,
        EvalJs(shell()->web_contents(),
               JsReplace(
                   "(() => {"
                   "const e = document.getElementById('target');"
                   "const style = window.getComputedStyle(e, null);"
                   "return Math.round(style.getPropertyValue($1).replace('px', "
                   "''));"
                   "})();",
                   name)));
  }

  void ValidateWindowsControlOverlayState(WebContents* web_contents,
                                          const gfx::Rect& expected_rect,
                                          int css_fallback_value) {
    EXPECT_EQ(!expected_rect.IsEmpty(),
              EvalJs(web_contents, "navigator.windowControlsOverlay.visible"));
    EXPECT_EQ(
        expected_rect.x(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getTitlebarAreaRect().x"));
    EXPECT_EQ(
        expected_rect.y(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getTitlebarAreaRect().y"));
    EXPECT_EQ(
        expected_rect.width(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getTitlebarAreaRect().width"));
    EXPECT_EQ(
        expected_rect.height(),
        EvalJs(web_contents,
               "navigator.windowControlsOverlay.getTitlebarAreaRect().height"));

    // When the overlay is not visible, the environment variables should be
    // undefined, and the the fallback value should be used.
    gfx::Rect css_rect = expected_rect;
    if (css_rect.IsEmpty()) {
      css_rect.SetRect(css_fallback_value, css_fallback_value,
                       css_fallback_value, css_fallback_value);
    }

    ValidateTitlebarAreaCSSValue("left", css_rect.x());
    ValidateTitlebarAreaCSSValue("top", css_rect.y());
    ValidateTitlebarAreaCSSValue("width", css_rect.width());
    ValidateTitlebarAreaCSSValue("height", css_rect.height());
  }

  void WaitForWindowControlsOverlayUpdate(
      WebContents* web_contents,
      const gfx::Rect& bounding_client_rect) {
    EXPECT_TRUE(
        ExecJs(web_contents->GetPrimaryMainFrame(),
               "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
               "  document.title = 'ongeometrychange'"
               "}"));

    web_contents->UpdateWindowControlsOverlay(bounding_client_rect);
    TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    std::ignore = title_watcher.WaitAndGetTitle();
  }
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestWindowControlsOverlay,
                       ValidateWindowControlsOverlayToggleOn) {
  auto* web_contents = shell()->web_contents();

  GURL url(
      R"(data:text/html,<body><div id=target style="position=absolute;
      left: env(titlebar-area-x, 50px);
      top: env(titlebar-area-y, 50px);
      width: env(titlebar-area-width, 50px);
      height: env(titlebar-area-height, 50px);"></div></body>)");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // In the initial state, the overlay is not visible and the bounding rect is
  // empty.
  ValidateWindowsControlOverlayState(web_contents, gfx::Rect(), 50);

  // Update bounds and ensure that JS APIs and CSS variables are updated.
  gfx::Rect bounding_client_rect(1, 2, 3, 4);
  WaitForWindowControlsOverlayUpdate(web_contents, bounding_client_rect);
  ValidateWindowsControlOverlayState(web_contents, bounding_client_rect, 50);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestWindowControlsOverlay,
                       ValidateWindowControlsOverlayToggleOff) {
  auto* web_contents = shell()->web_contents();

  GURL url(
      R"(data:text/html,<body><div id=target style="position=absolute;
      left: env(titlebar-area-x, 55px);
      top: env(titlebar-area-y, 55px);
      width: env(titlebar-area-width, 55px);
      height: env(titlebar-area-height, 55px);"></div></body>)");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Update bounds and ensure that JS APIs and CSS variables are updated.
  gfx::Rect bounding_client_rect(0, 0, 100, 32);
  WaitForWindowControlsOverlayUpdate(web_contents, bounding_client_rect);
  ValidateWindowsControlOverlayState(web_contents, bounding_client_rect, 55);

  // Now toggle Windows Controls Overlay off.
  gfx::Rect empty_rect;
  WaitForWindowControlsOverlayUpdate(web_contents, empty_rect);
  ValidateWindowsControlOverlayState(web_contents, empty_rect, 55);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestWindowControlsOverlay,
                       GeometryChangeEvent) {
  auto* web_contents = shell()->web_contents();

  GURL url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(
      ExecJs(web_contents->GetPrimaryMainFrame(),
             "geometrychangeCount = 0;"
             "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
             "  geometrychangeCount++;"
             "  rect = e.titlebarAreaRect;"
             "  visible = e.visible;"
             "  document.title = 'ongeometrychange' + geometrychangeCount"
             "}"));

  WaitForLoadStop(web_contents);

  // Ensure the "geometrychange" event is only fired when the the window
  // controls overlay bounds are updated.
  EXPECT_EQ(0, EvalJs(web_contents, "geometrychangeCount"));

  // Information about the bounds should be updated.
  gfx::Rect bounding_client_rect = gfx::Rect(2, 3, 4, 5);
  web_contents->UpdateWindowControlsOverlay(bounding_client_rect);
  TitleWatcher title_watcher(web_contents, u"ongeometrychange1");
  std::ignore = title_watcher.WaitAndGetTitle();

  // Expect the "geometrychange" event to have fired once.
  EXPECT_EQ(1, EvalJs(web_contents, "geometrychangeCount"));

  // Validate the event payload.
  EXPECT_EQ(true, EvalJs(web_contents, "visible"));
  EXPECT_EQ(bounding_client_rect.x(), EvalJs(web_contents, "rect.x;"));
  EXPECT_EQ(bounding_client_rect.y(), EvalJs(web_contents, "rect.y"));
  EXPECT_EQ(bounding_client_rect.width(), EvalJs(web_contents, "rect.width"));
  EXPECT_EQ(bounding_client_rect.height(), EvalJs(web_contents, "rect.height"));
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTestWindowControlsOverlay,
                       ValidatePageScaleChangesInfoAndFiresEvent) {
  auto* web_contents = shell()->web_contents();
  GURL url(
      R"(data:text/html,<body><div id=target style="position=absolute;
      left: env(titlebar-area-x, 60px);
      top: env(titlebar-area-y, 60px);
      width: env(titlebar-area-width, 60px);
      height: env(titlebar-area-height, 60px);"></div></body>)");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  WaitForLoadStop(web_contents);

  gfx::Rect bounding_client_rect = gfx::Rect(5, 10, 15, 20);
  WaitForWindowControlsOverlayUpdate(web_contents, bounding_client_rect);

  // Update zoom level, confirm the "geometrychange" event is fired,
  // and CSS variables are updated
  EXPECT_TRUE(
      ExecJs(web_contents->GetPrimaryMainFrame(),
             "geometrychangeCount = 0;"
             "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
             "  geometrychangeCount++;"
             "  rect = e.titlebarAreaRect;"
             "  visible = e.visible;"
             "  document.title = 'ongeometrychangefromzoomlevel'"
             "}"));
  content::HostZoomMap::SetZoomLevel(web_contents, 1.5);
  TitleWatcher title_watcher(web_contents, u"ongeometrychangefromzoomlevel");
  std::ignore = title_watcher.WaitAndGetTitle();

  // Validate the event payload.
  double zoom_factor = blink::ZoomLevelToZoomFactor(
      content::HostZoomMap::GetZoomLevel(web_contents));
  gfx::Rect scaled_rect =
      gfx::ScaleToEnclosingRect(bounding_client_rect, 1.0f / zoom_factor);

  EXPECT_EQ(true, EvalJs(web_contents, "visible"));
  EXPECT_EQ(scaled_rect.x(), EvalJs(web_contents, "rect.x"));
  EXPECT_EQ(scaled_rect.y(), EvalJs(web_contents, "rect.y"));
  EXPECT_EQ(scaled_rect.width(), EvalJs(web_contents, "rect.width"));
  EXPECT_EQ(scaled_rect.height(), EvalJs(web_contents, "rect.height"));
  ValidateWindowsControlOverlayState(web_contents, scaled_rect, 60);
}

class WebContentsImplBrowserTestWindowControlsOverlayNonOneDeviceScaleFactor
    : public WebContentsImplBrowserTestWindowControlsOverlay {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    // Device scale factor on MacOSX is always an integer.
    EnablePixelOutput(2.0f);
#else
    EnablePixelOutput(1.25f);
#endif
    WebContentsImplBrowserTestWindowControlsOverlay::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTestWindowControlsOverlayNonOneDeviceScaleFactor,
    ValidateScaledCorrectly) {
  auto* web_contents = shell()->web_contents();
  GURL url(
      R"(data:text/html,<body><div id=target style="position=absolute;
      left: env(titlebar-area-x, 70px);
      top: env(titlebar-area-y, 70px);
      width: env(titlebar-area-width, 70px);
      height: env(titlebar-area-height, 70px);"></div></body>)");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  WaitForLoadStop(web_contents);
#if BUILDFLAG(IS_MAC)
  // Device scale factor on MacOSX is always an integer.
  ASSERT_EQ(2.0f,
            web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor());
#else
  ASSERT_EQ(1.25f,
            web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor());
#endif

  gfx::Rect bounding_client_rect = gfx::Rect(5, 10, 15, 20);
  WaitForWindowControlsOverlayUpdate(web_contents, bounding_client_rect);
  ValidateWindowsControlOverlayState(web_contents, bounding_client_rect, 70);
}

IN_PROC_BROWSER_TEST_F(
    WebContentsImplBrowserTestWindowControlsOverlayNonOneDeviceScaleFactor,
    ValidateScaledCorrectlyAfterNavigate) {
  auto* web_contents = shell()->web_contents();
  GURL url(
      R"(data:text/html,<body><div id=target style="position=absolute;
      left: env(titlebar-area-x, 70px);
      top: env(titlebar-area-y, 70px);
      width: env(titlebar-area-width, 70px);
      height: env(titlebar-area-height, 70px);"><p>Page1</p></div></body>)");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  WaitForLoadStop(web_contents);
#if BUILDFLAG(IS_MAC)
  // Device scale factor on MacOSX is always an integer.
  ASSERT_EQ(2.0f,
            web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor());
#else
  ASSERT_EQ(1.25f,
            web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor());
#endif

  gfx::Rect bounding_client_rect = gfx::Rect(5, 10, 15, 20);
  WaitForWindowControlsOverlayUpdate(web_contents, bounding_client_rect);
  ValidateWindowsControlOverlayState(web_contents, bounding_client_rect, 70);

  // Validate that the |bounding_client_rect| is scaled correctly on navigation.
  GURL second_url(
      R"(data:text/html,<body><div id=target style="position=absolute;
      left: env(titlebar-area-x, 70px);
      top: env(titlebar-area-y, 70px);
      width: env(titlebar-area-width, 70px);
      height: env(titlebar-area-height, 70px);"><p>Page2</p></div></body>)");

  EXPECT_TRUE(NavigateToURL(shell(), second_url));
  WaitForLoadStop(web_contents);

  ValidateWindowsControlOverlayState(web_contents, bounding_client_rect, 70);
}
#endif  // !BUILDFLAG(IS_ANDROID)

class RenderFrameCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderFrameCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderFrameCreatedObserver() override = default;

  void WaitForRenderFrameCreated() { run_loop_.Run(); }

  void RenderFrameCreated(RenderFrameHost* host) override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ReinitializeMainFrameForCrashedTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  load_observer.Wait();

  CrashTab(shell()->web_contents());
  EXPECT_TRUE(shell()->web_contents()->IsCrashed());

  RenderFrameCreatedObserver frame_created_obs(shell()->web_contents());
  static_cast<WebContentsImpl*>(shell()->web_contents())
      ->GetPrimaryFrameTree()
      .root()
      ->render_manager()
      ->InitializeMainRenderFrameForImmediateUse();
  frame_created_obs.WaitForRenderFrameCreated();
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

// Check that there's no crash if a new window is set to defer navigations (for
// example, this is done on Android Webview and for <webview> guests), then the
// renderer process crashes while there's a deferred new window navigation in
// place, and then navigations are resumed. Prior to fixing
// https://crbug.com/1487110, the deferred navigation was allowed to proceed,
// performing an early RenderFrameHost swap and hitting a bug while clearing
// the deferred navigation state. Now, the deferred navigation should be
// canceled when the renderer process dies.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DeferredWindowOpenNavigationIsResumedWithEarlySwap) {
  // Force WebContents in a new Shell to defer new navigations until the
  // delegate is set.
  shell()->set_delay_popup_contents_delegate_for_testing(true);

  // Load an initial page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Open a popup to a same-site URL via window.open.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.open($1);", url)));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();

  // The navigation in the new popup should be deferred.
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_TRUE(new_contents->GetController().IsInitialBlankNavigation());
  EXPECT_TRUE(new_contents->GetLastCommittedURL().is_empty());

  // Set the new shell's delegate now.  This doesn't resume the navigation just
  // yet.
  EXPECT_FALSE(new_contents->GetDelegate());
  new_contents->SetDelegate(new_shell);

  // Crash the renderer process.  This should clear the deferred navigation
  // state.  If this wasn't done due to a bug, it would also force the resumed
  // navigation to use the early RenderFrameHost swap.
  {
    RenderProcessHost* popup_process =
        new_contents->GetPrimaryMainFrame()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        popup_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    EXPECT_TRUE(popup_process->Shutdown(0));
    crash_observer.Wait();
  }

  // Resume the navigation and verify that it gets canceled.  Ensure this
  // doesn't crash.
  NavigationHandleObserver handle_observer(new_contents, url);
  new_contents->ResumeLoadingCreatedWebContents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_FALSE(handle_observer.has_committed());
  EXPECT_TRUE(new_contents->GetController().IsInitialBlankNavigation());
  EXPECT_TRUE(new_contents->GetLastCommittedURL().is_empty());
}

namespace {

class MediaWaiter : public WebContentsObserver {
 public:
  explicit MediaWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const MediaPlayerId& id) override {
    started_media_id_ = id;
    ;
    media_started_playing_loop_.Quit();
  }
  void MediaDestroyed(const MediaPlayerId& id) override {
    EXPECT_EQ(id, started_media_id_);
    media_destroyed_loop_.Quit();
  }

  void WaitForMediaStartedPlaying() { media_started_playing_loop_.Run(); }
  void WaitForMediaDestroyed() { media_destroyed_loop_.Run(); }

 private:
  std::optional<MediaPlayerId> started_media_id_;
  base::RunLoop media_started_playing_loop_;
  base::RunLoop media_destroyed_loop_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MediaDestroyedOnRendererCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MediaWaiter waiter(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/media/video-player-autoplay.html")));

  waiter.WaitForMediaStartedPlaying();

  CrashTab(shell()->web_contents());
  EXPECT_TRUE(shell()->web_contents()->IsCrashed());

  // This will not hang if MediaDestroyed() is dispatched as expected when a
  // frame with a media player is destroyed.
  waiter.WaitForMediaDestroyed();
}

class WebContentsImplInsecureLocalhostBrowserTest
    : public WebContentsImplBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    WebContentsImplBrowserTest::SetUpOnMainThread();
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(WebContentsImplInsecureLocalhostBrowserTest,
                       BlocksByDefault) {
  https_server().SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server().Start());
  GURL url = https_server().GetURL("/title1.html");

  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  EXPECT_TRUE(
      IsLastCommittedEntryOfPageType(shell()->web_contents(), PAGE_TYPE_ERROR));
}

class WebContentsImplAllowInsecureLocalhostBrowserTest
    : public WebContentsImplInsecureLocalhostBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAllowInsecureLocalhost);
  }
};

IN_PROC_BROWSER_TEST_F(WebContentsImplAllowInsecureLocalhostBrowserTest,
                       WarnsWithSwitch) {
  https_server().SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server().Start());
  GURL url = https_server().GetURL("/title1.html");

  WebContentsConsoleObserver observer(shell()->web_contents());
  observer.SetFilter(base::BindRepeating(
      [](const GURL& expected_url,
         const WebContentsConsoleObserver::Message& message) {
        return message.source_frame->GetLastCommittedURL() == expected_url;
      },
      url));
  observer.SetPattern("*SSL certificate*");

  ASSERT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(observer.Wait());
}

class WebContentsPrerenderBrowserTest : public WebContentsImplBrowserTest {
 public:
  WebContentsPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&WebContentsPrerenderBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~WebContentsPrerenderBrowserTest() override = default;

  // PrerenderTestHelper requires access to WebContents object.
  WebContents* web_contents() { return shell()->web_contents(); }

  // Testing functionality requires access to WebContentsImpl object.
  WebContentsImpl* web_contents_impl() {
    return static_cast<WebContentsImpl*>(web_contents());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

class TestWebContentsDestructionObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsDestructionObserver(WebContentsImpl* web_contents)
      : content::WebContentsObserver(web_contents) {}

  TestWebContentsDestructionObserver(
      const TestWebContentsDestructionObserver&) = delete;
  TestWebContentsDestructionObserver& operator=(
      const TestWebContentsDestructionObserver&) = delete;

  ~TestWebContentsDestructionObserver() override = default;

  void WebContentsDestroyed() override {
    // This has been added to validate that it's safe to call this method
    // within a WebContentsDestroyed observer.  We want to verify that
    // this does not cause a crash.
    static_cast<WebContentsImpl*>(web_contents())
        ->ForEachFrameTree(base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(WebContentsPrerenderBrowserTest,
                       SafeToCallForEachFrameTreeDuringDestruction) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL url_a(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));

  TestWebContentsDestructionObserver test_web_contents_observer(
      web_contents_impl());
  WebContentsDestroyedWatcher close_observer(web_contents_impl());
  web_contents_impl()->DispatchBeforeUnload(false /* auto_cancel */);
  close_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(WebContentsPrerenderBrowserTest,
                       GetContentsMimeTypeForEachPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Prerender a page that has a MIME type, text/plain.
  const GURL prerendering_url = embedded_test_server()->GetURL("/plain.txt");
  FrameTreeNodeId host_id = prerender_helper().AddPrerender(prerendering_url);

  // Check MIME type for each page.
  EXPECT_EQ("text/html",
            web_contents()->GetPrimaryPage().GetContentsMimeType());
  EXPECT_EQ("text/plain", prerender_helper()
                              .GetPrerenderedMainFrameHost(host_id)
                              ->GetPage()
                              .GetContentsMimeType());

  // WebContents API should return the MIME type of the primary page.
  EXPECT_EQ("text/html", shell()->web_contents()->GetContentsMimeType());
}

class WebContentsPrerenderWithDiscardBrowserTest
    : public WebContentsPrerenderBrowserTest {
 public:
  WebContentsPrerenderWithDiscardBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebContentsDiscard);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebContentsPrerenderWithDiscardBrowserTest,
                       DiscardCancelsPrerendering) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Prerender a page.
  const GURL prerendering_url = embedded_test_server()->GetURL("/plain.txt");
  const FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerendering_url);
  PrerenderHostRegistry* registry =
      static_cast<WebContentsImpl*>(web_contents())->GetPrerenderHostRegistry();
  EXPECT_TRUE(registry->FindNonReservedHostById(host_id));

  // Discard the tab, this should immediately clear all prerender frame trees.
  testing::NiceMock<MockWebContentsObserver> observer(web_contents());
  EXPECT_CALL(observer, AboutToBeDiscarded(web_contents())).Times(1);
  EXPECT_CALL(observer, WasDiscarded()).Times(1);
  EXPECT_FALSE(web_contents()->WasDiscarded());
  web_contents()->Discard();
  EXPECT_TRUE(web_contents()->WasDiscarded());
  EXPECT_FALSE(registry->FindNonReservedHostById(host_id));
}

class WebContentsFencedFrameBrowserTest : public WebContentsImplBrowserTest {
 public:
  WebContentsFencedFrameBrowserTest() = default;
  ~WebContentsFencedFrameBrowserTest() override = default;

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Tests that DidUpdateFaviconURL() works only with the primary page by checking
// if it's not called on the fenced frame loading.
IN_PROC_BROWSER_TEST_F(WebContentsFencedFrameBrowserTest, UpdateFavicon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  testing::NiceMock<MockWebContentsObserver> observer(web_contents());
  const GURL main_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");

  RenderFrameHost* primary_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_CALL(observer, DidUpdateFaviconURL(primary_rfh, testing::_));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Create fenced frame.
  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");

  RenderFrameHost* inner_fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh,
                                                   fenced_frame_url);
  EXPECT_CALL(observer, DidUpdateFaviconURL(inner_fenced_frame_rfh, testing::_))
      .Times(0);
}

// Tests that pages are still visible after a page is navigated away
// from a page that contained a fenced frame. (crbug.com/1265615)
IN_PROC_BROWSER_TEST_F(WebContentsFencedFrameBrowserTest, RemainsVisible) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL main_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");

  RenderFrameHost* primary_rfh = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());

  // Create fenced frame.
  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh,
                                                   fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);

  const GURL same_origin_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title3.html");

  ASSERT_TRUE(NavigateToURL(shell(), same_origin_url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
}

// Tests that AXTreeIDForMainFrameHasChanged() works only with the primary page
// by checking if it's not called on the fenced frame loading.
IN_PROC_BROWSER_TEST_F(WebContentsFencedFrameBrowserTest, DoNotUpdateAXTree) {
  ASSERT_TRUE(embedded_test_server()->Start());
  testing::NiceMock<MockWebContentsObserver> observer(web_contents());
  const GURL main_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");

  EXPECT_CALL(observer, AXTreeIDForMainFrameHasChanged())
      .Times(testing::AtLeast(1));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Create fenced frame.
  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/title1.html");
  EXPECT_CALL(observer, AXTreeIDForMainFrameHasChanged()).Times(0);
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_rfh);
}

class MediaWatchTimeChangedDelegate : public WebContentsDelegate {
 public:
  explicit MediaWatchTimeChangedDelegate(WebContents* contents)
      : watch_time_(GURL(),
                    GURL(),
                    base::TimeDelta(),
                    base::TimeDelta(),
                    false,
                    false) {
    contents->SetDelegate(this);
  }
  ~MediaWatchTimeChangedDelegate() override = default;
  MediaWatchTimeChangedDelegate(const MediaWatchTimeChangedDelegate&) = delete;
  MediaWatchTimeChangedDelegate& operator=(
      const MediaWatchTimeChangedDelegate&) = delete;

  // WebContentsDelegate:
  void MediaWatchTimeChanged(const MediaPlayerWatchTime& watch_time) override {
    watch_time_ = watch_time;
  }

  const MediaPlayerWatchTime& watch_time() { return watch_time_; }

 private:
  MediaPlayerWatchTime watch_time_;
};

// Tests that a media in a fenced frame reports the watch time with the url from
// the top level frame.
IN_PROC_BROWSER_TEST_F(WebContentsFencedFrameBrowserTest,
                       MediaWatchTimeCallback) {
  using UkmEntry = ukm::builders::Media_WebMediaPlayerState;
  ukm::TestAutoSetUkmRecorder test_recorder;

  MediaWatchTimeChangedDelegate delegate(web_contents());
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  const GURL top_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), top_url));

  // Create a fenced frame.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  // Insert a video element.
  EXPECT_TRUE(ExecJs(fenced_frame, R"(
    var video = document.createElement('video');
    document.body.appendChild(video);
    video.src = '../media/bear.webm';
    video.play();
  )"));

  base::RunLoop run_loop;
  test_recorder.SetOnAddEntryCallback(UkmEntry::kEntryName,
                                      run_loop.QuitClosure());
  // Leave the current page to check the UKM records.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame,
      embedded_test_server()->GetURL("a.com", "/fenced_frames/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  run_loop.Run();

  const auto& entries = test_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_recorder.ExpectEntryMetric(entry, UkmEntry::kIsTopFrameName, false);
  }
}

}  // namespace content
