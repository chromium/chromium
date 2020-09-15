// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/page_messages.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"
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
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_client_hints_controller_delegate.h"
#include "content/test/resource_load_observer.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "url/gurl.h"

namespace content {

#define SCOPE_TRACED(statement) \
  {                             \
    SCOPED_TRACE(#statement);   \
    statement;                  \
  }

void ResizeWebContentsView(Shell* shell, const gfx::Size& size,
                           bool set_start_page) {
  // Resizing the web content directly, independent of the Shell window,
  // requires the RenderWidgetHostView to exist. So we do a navigation
  // first if |set_start_page| is true.
  if (set_start_page)
    EXPECT_TRUE(NavigateToURL(shell, GURL(url::kAboutBlankURL)));

  shell->ResizeWebContentForTests(size);
}

// Class to test that OverrideWebkitPrefs has been called for all relevant
// RenderViewHosts.
class NotifyPreferencesChangedTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  NotifyPreferencesChangedTestContentBrowserClient() = default;

  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           blink::web_pref::WebPreferences* prefs) override {
    override_webkit_prefs_rvh_set_.insert(render_view_host);
  }

  const std::unordered_set<RenderViewHost*>& override_webkit_prefs_rvh_set() {
    return override_webkit_prefs_rvh_set_;
  }

 private:
  std::unordered_set<RenderViewHost*> override_webkit_prefs_rvh_set_;

  DISALLOW_COPY_AND_ASSIGN(NotifyPreferencesChangedTestContentBrowserClient);
};

class WebContentsImplBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImplBrowserTest() {}
  void SetUp() override {
    RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsInFullscreen() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->current_fullscreen_frame_;
  }

 protected:
  // Gets script to create subframe.
  std::string GetSubframeScript(const GURL& sub_frame) {
    const char kLoadIframeScript[] = R"(
        let iframe = document.createElement('iframe');
        iframe.src = $1;
        document.body.appendChild(iframe);
      )";
    return JsReplace(kLoadIframeScript, sub_frame);
  }

  // Creates and loads subframe, waits for load to stop, and then returns
  // subframe from the web contents frame tree.
  RenderFrameHost* CreateSubframe(const GURL& sub_frame) {
    EXPECT_TRUE(ExecuteScript(shell(), GetSubframeScript(sub_frame)));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetFrameTree()
        ->root()
        ->child_at(0)
        ->current_frame_host();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsImplBrowserTest);
};

// Keeps track of data from LoadNotificationDetails so we can later verify that
// they are correct, after the LoadNotificationDetails object is deleted.
class LoadStopNotificationObserver : public WindowedNotificationObserver {
 public:
  explicit LoadStopNotificationObserver(NavigationController* controller)
      : WindowedNotificationObserver(NOTIFICATION_LOAD_STOP,
                                     Source<NavigationController>(controller)),
        session_index_(-1),
        controller_(nullptr) {}
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override {
    if (type == NOTIFICATION_LOAD_STOP) {
      const Details<LoadNotificationDetails> load_details(details);
      url_ = load_details->url;
      session_index_ = load_details->session_index;
      controller_ = load_details->controller;
    }
    WindowedNotificationObserver::Observe(type, source, details);
  }

  GURL url_;
  int session_index_;
  NavigationController* controller_;
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
        done_(false) {
  }

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

  Shell* shell_;
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
        wcv_new_size_(wcv_new_size) {
  }

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
  Shell* shell_;  // Weak ptr.
  gfx::Size wcv_new_size_;
  gfx::Size rwhv_create_size_;
};

class LoadingStateChangedDelegate : public WebContentsDelegate {
 public:
  LoadingStateChangedDelegate()
      : loadingStateChangedCount_(0)
      , loadingStateToDifferentDocumentCount_(0) {
  }

  // WebContentsDelegate:
  void LoadingStateChanged(WebContents* contents,
                           bool to_different_document) override {
      loadingStateChangedCount_++;
      if (to_different_document)
        loadingStateToDifferentDocumentCount_++;
  }

  int loadingStateChangedCount() const { return loadingStateChangedCount_; }
  int loadingStateToDifferentDocumentCount() const {
    return loadingStateToDifferentDocumentCount_;
  }

 private:
  int loadingStateChangedCount_;
  int loadingStateToDifferentDocumentCount_;
};

// Test that DidStopLoading includes the correct URL in the details.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DidStopLoadingDetails) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  load_observer.Wait();

  EXPECT_EQ("/title1.html", load_observer.url_.path());
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}

// Test that DidStopLoading includes the correct URL in the details when a
// pending entry is present.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidStopLoadingDetailsWithPending) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(clamy): Add a cross-process navigation case as well once
  // crbug.com/581024 is fixed.
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Listen for the first load to stop.
  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  // Start a new pending navigation as soon as the first load commits.
  // We will hear a DidStopLoading from the first load as the new load
  // is started.
  NavigateOnCommitObserver commit_observer(shell(), url2);
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  load_observer.Wait();

  EXPECT_EQ(url1, load_observer.url_);
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
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
  FrameTreeNode* subframe_c = web_contents->GetFrameTree()->root()->child_at(1);
  EXPECT_EQ(url_c, subframe_c->current_url());
  TestNavigationManager delayer_d(web_contents, url_d);
  const std::string add_d_script = base::StringPrintf(
      "var f = document.createElement('iframe');"
      "f.src='%s';"
      "document.body.appendChild(f);",
      url_d.spec().c_str());
  EXPECT_TRUE(content::ExecuteScript(subframe_c, add_d_script));
  EXPECT_TRUE(delayer_d.WaitForRequestStart());
  EXPECT_TRUE(web_contents->IsLoading());

  // Let B finish and wait for another load stop.  A will still be loading due
  // to D.
  LoadFinishedWaiter load_waiter_b(web_contents, url_b);
  delayer_b.WaitForNavigationFinished();
  load_waiter_b.Wait();
  EXPECT_TRUE(web_contents->IsLoading());

  // Let D finish.  We should get a load stop in the main frame.
  LoadFinishedWaiter load_waiter_d(web_contents, url_d);
  delayer_d.WaitForNavigationFinished();
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
  LoadStopNotificationObserver load_observer1(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(
      ExecuteScript(shell(), "window.location.href=\"nonexistent:12121\";"));
  load_observer1.Wait();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

  LoadStopNotificationObserver load_observer2(
      &shell()->web_contents()->GetController());
  ASSERT_TRUE(ExecuteScript(shell(), "window.location.href=\"#foo\";"));
  load_observer2.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html#foo"),
            shell()->web_contents()->GetVisibleURL());
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(OS_WIN) || defined(OS_ANDROID) \
    || defined(THREAD_SANITIZER)
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
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());

  // When a size is set, RenderWidgetHostView and WebContentsView honor this
  // size.
  gfx::Size size(300, 300);
  gfx::Size size_insets(10, 15);
  ResizeWebContentsView(shell(), size, true);
  delegate->set_size_insets(size_insets);
  EXPECT_TRUE(NavigateToURL(shell(), https_server.GetURL("/")));
  size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(size,
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());
  // The web_contents size is set by the embedder, and should not depend on the
  // rwhv size. The behavior is correct on OSX, but incorrect on other
  // platforms.
  gfx::Size exp_wcv_size(300, 300);
#if !defined(OS_MAC)
  exp_wcv_size.Enlarge(size_insets.width(), size_insets.height());
#endif

  EXPECT_EQ(exp_wcv_size,
            shell()->web_contents()->GetContainerBounds().size());

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
#if !defined(OS_MAC)
  new_size.Enlarge(size_insets.width(), size_insets.height());
#endif
  gfx::Size actual_size = shell()->web_contents()->GetRenderWidgetHostView()->
      GetViewBounds().size();

  EXPECT_EQ(new_size, actual_size);
  EXPECT_EQ(new_size, shell()->web_contents()->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, SetTitleOnUnload) {
  GURL url(
      "data:text/html,"
      "<title>A</title>"
      "<body onunload=\"document.title = 'B'\"></body>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ(1, shell()->web_contents()->GetController().GetEntryCount());
  NavigationEntryImpl* entry1 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance1 = entry1->site_instance();
  EXPECT_EQ(base::ASCIIToUTF16("A"), entry1->GetTitle());

  // Force a process switch by going to a privileged page.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_page));
  NavigationEntryImpl* entry2 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance2 = entry2->site_instance();
  EXPECT_NE(site_instance1, site_instance2);

  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(base::ASCIIToUTF16("B"), entry1->GetTitle());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, OpenURLSubframe) {
  // Navigate to a page with frames and grab a subframe's FrameTreeNode ID.
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(3UL, root->child_count());
  int frame_tree_node_id = root->child_at(0)->frame_tree_node_id();
  EXPECT_NE(-1, frame_tree_node_id);

  // Navigate with the subframe's FrameTreeNode ID.
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  OpenURLParams params(url, Referrer(), frame_tree_node_id,
                       WindowOpenDisposition::CURRENT_TAB,
                       ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin = wc->GetMainFrame()->GetLastCommittedOrigin();
  shell()->web_contents()->OpenURL(params);

  // Make sure the NavigationEntry ends up with the FrameTreeNode ID.
  NavigationController* controller = &shell()->web_contents()->GetController();
  EXPECT_TRUE(controller->GetPendingEntry());
  EXPECT_EQ(frame_tree_node_id,
            NavigationEntryImpl::FromNavigationEntry(
                controller->GetPendingEntry())->frame_tree_node_id());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       AppendingFrameInWebUIDoesNotCrash) {
  const GURL kWebUIUrl(GetWebUIURL("gpu"));
  const char kJSCodeForAppendingFrame[] =
      "document.body.appendChild(document.createElement('iframe'));";

  EXPECT_TRUE(NavigateToURL(shell(), kWebUIUrl));

  EXPECT_TRUE(content::ExecuteScript(shell(), kJSCodeForAppendingFrame));
}

// Observer class to track the creation of RenderFrameHost objects. It is used
// in subsequent tests.
class RenderFrameCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderFrameCreatedObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()), last_rfh_(nullptr) {}

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    last_rfh_ = render_frame_host;
  }

  RenderFrameHost* last_rfh() const { return last_rfh_; }

 private:
  RenderFrameHost* last_rfh_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameCreatedObserver);
};

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
  RenderFrameHost* orig_rfh = shell()->web_contents()->GetMainFrame();

  // Install the observer and navigate cross-site.
  RenderFrameCreatedObserver observer(shell());
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));

  // The observer should've seen a RenderFrameCreated call for the new frame
  // and not the old one.
  EXPECT_NE(observer.last_rfh(), orig_rfh);
  EXPECT_EQ(observer.last_rfh(), shell()->web_contents()->GetMainFrame());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       LoadingStateChangedForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<LoadingStateChangedDelegate> delegate(
      new LoadingStateChangedDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());

  LoadStopNotificationObserver load_observer(
      &shell()->web_contents()->GetController());
  TitleWatcher title_watcher(shell()->web_contents(),
                             base::ASCIIToUTF16("pushState"));
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/push_state.html")));
  load_observer.Wait();
  base::string16 title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, base::ASCIIToUTF16("pushState"));

  // LoadingStateChanged should be called 5 times: start and stop for the
  // initial load of push_state.html, once for the switch from
  // IsWaitingForResponse() to !IsWaitingForResponse(), and start and stop for
  // the "navigation" triggered by history.pushState(). However, the start
  // notification for the history.pushState() navigation should set
  // to_different_document to false.
  EXPECT_EQ("pushState", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_EQ(5, delegate->loadingStateChangedCount());
  EXPECT_EQ(4, delegate->loadingStateToDifferentDocumentCount());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderViewCreatedForChildWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  WebContentsAddedObserver new_web_contents_observer;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "var a = document.createElement('a');"
                            "a.href='./title2.html';"
                            "a.target = '_blank';"
                            "document.body.appendChild(a);"
                            "a.click();"));
  WebContents* new_web_contents = new_web_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(new_web_contents));
  EXPECT_TRUE(new_web_contents_observer.RenderViewCreatedCalled());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ResourceLoadComplete) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());
  // Load a page with an image and an image.
  GURL page_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  base::TimeTicks before = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  base::TimeTicks after = base::TimeTicks::Now();
  ASSERT_EQ(3U, observer.resource_load_infos().size());
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
  ASSERT_EQ(2U, observer.resource_load_infos().size());
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
  EXPECT_TRUE(
      observer.resource_load_infos()[1]->network_info->network_accessed);
  EXPECT_TRUE(observer.memory_cached_loaded_urls().empty());
  observer.Reset();

  // Loading again should serve the request out of the in-memory cache.
  before = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  after = base::TimeTicks::Now();
  ASSERT_EQ(1U, observer.resource_load_infos().size());
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
  ASSERT_EQ(2U, observer.resource_load_infos().size());
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
  EXPECT_FALSE(
      observer.resource_load_infos()[1]->network_info->network_accessed);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteFromLocalResource) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL(embedded_test_server()->GetURL("/page_with_image.html"))));
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_TRUE(
      observer.resource_load_infos()[0]->network_info->network_accessed);
  EXPECT_TRUE(
      observer.resource_load_infos()[1]->network_info->network_accessed);
  observer.Reset();

  EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("gpu")));
  ASSERT_LE(1U, observer.resource_load_infos().size());
  for (const blink::mojom::ResourceLoadInfoPtr& resource_load_info :
       observer.resource_load_infos()) {
    EXPECT_FALSE(resource_load_info->network_info->network_accessed);
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

  ASSERT_EQ(2U, observer.resource_load_infos().size());
  const blink::mojom::ResourceLoadInfoPtr& page_load_info =
      observer.resource_load_infos()[0];
  EXPECT_EQ(page_destination_url, page_load_info->final_url);
  EXPECT_EQ(page_original_url, page_load_info->original_url);

  GURL image_destination_url(embedded_test_server()->GetURL("/blank.jpg"));
  GURL image_original_url(
      embedded_test_server()->GetURL("/server-redirect?blank.jpg"));
  const blink::mojom::ResourceLoadInfoPtr& image_load_info =
      observer.resource_load_infos()[1];
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
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_EQ(net::OK, observer.resource_load_infos()[0]->net_error);
  EXPECT_EQ(net::OK, observer.resource_load_infos()[1]->net_error);
  observer.Reset();

  // Load the page and simulate a network error.
  content::URLLoaderInterceptor url_interceptor(base::BindRepeating(
      [](const GURL& url,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != url)
          return false;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_ADDRESS_UNREACHABLE;
        params->client->OnComplete(status);
        return true;
      },
      image_url));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_EQ(net::OK, observer.resource_load_infos()[0]->net_error);
  EXPECT_EQ(net::ERR_ADDRESS_UNREACHABLE,
            observer.resource_load_infos()[1]->net_error);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteAlwaysAccessNetwork) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL cacheable_url(embedded_test_server()->GetURL("/set-header"));
  EXPECT_TRUE(NavigateToURL(shell(), cacheable_url));
  ASSERT_EQ(1U, observer.resource_load_infos().size());
  EXPECT_FALSE(
      observer.resource_load_infos()[0]->network_info->always_access_network);
  observer.Reset();

  std::array<std::string, 3> headers = {
      "cache-control: no-cache", "cache-control: no-store", "pragma: no-cache"};
  for (const std::string& header : headers) {
    GURL no_cache_url(embedded_test_server()->GetURL("/set-header?" + header));
    EXPECT_TRUE(NavigateToURL(shell(), no_cache_url));
    ASSERT_EQ(1U, observer.resource_load_infos().size());
    EXPECT_TRUE(
        observer.resource_load_infos()[0]->network_info->always_access_network);
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

  ASSERT_EQ(1U, observer.resource_load_infos().size());
  EXPECT_EQ(target_url, observer.resource_load_infos()[0]->final_url);

  ASSERT_EQ(2U, observer.resource_load_infos()[0]->redirect_info_chain.size());
  EXPECT_EQ(url::Origin::Create(intermediate_url),
            observer.resource_load_infos()[0]
                ->redirect_info_chain[0]
                ->origin_of_new_url);
  EXPECT_TRUE(observer.resource_load_infos()[0]
                  ->redirect_info_chain[0]
                  ->network_info->network_accessed);
  EXPECT_FALSE(observer.resource_load_infos()[0]
                   ->redirect_info_chain[0]
                   ->network_info->always_access_network);
  EXPECT_EQ(url::Origin::Create(target_url), observer.resource_load_infos()[0]
                                                 ->redirect_info_chain[1]
                                                 ->origin_of_new_url);
  EXPECT_TRUE(observer.resource_load_infos()[0]
                  ->redirect_info_chain[1]
                  ->network_info->network_accessed);
  EXPECT_FALSE(observer.resource_load_infos()[0]
                   ->redirect_info_chain[1]
                   ->network_info->always_access_network);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteIsMainFrame) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_EQ(url, observer.resource_load_infos()[0]->original_url);
  EXPECT_EQ(url, observer.resource_load_infos()[0]->final_url);
  EXPECT_TRUE(observer.resource_is_associated_with_main_frame()[0]);
  EXPECT_TRUE(observer.resource_is_associated_with_main_frame()[1]);
  observer.Reset();

  // Load that same page inside an iframe.
  GURL data_url("data:text/html,<iframe src='" + url.spec() + "'></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));
  ASSERT_EQ(3U, observer.resource_load_infos().size());
  EXPECT_EQ(data_url, observer.resource_load_infos()[0]->original_url);
  EXPECT_EQ(data_url, observer.resource_load_infos()[0]->final_url);
  EXPECT_EQ(url, observer.resource_load_infos()[1]->original_url);
  EXPECT_EQ(url, observer.resource_load_infos()[1]->final_url);
  EXPECT_TRUE(observer.resource_is_associated_with_main_frame()[0]);
  EXPECT_FALSE(observer.resource_is_associated_with_main_frame()[1]);
  EXPECT_FALSE(observer.resource_is_associated_with_main_frame()[2]);
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
  if (std::adjacent_find(progresses.begin(),
                         progresses.end(),
                         std::greater<double>()) != progresses.end()) {
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
  if (std::adjacent_find(progresses.begin(),
                         progresses.end(),
                         std::greater<double>()) != progresses.end()) {
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
      shell()->web_contents()->GetMainFrame());

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
#if defined(OS_ANDROID)
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
  ~WebDisplayModeDelegate() override { }

  blink::mojom::DisplayMode GetDisplayMode(const WebContents* source) override {
    return mode_;
  }
  void set_mode(blink::mojom::DisplayMode mode) { mode_ = mode; }

 private:
  blink::mojom::DisplayMode mode_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayModeDelegate);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangeDisplayMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebDisplayModeDelegate delegate(blink::mojom::DisplayMode::kMinimalUi);
  shell()->web_contents()->SetDelegate(&delegate);

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  ASSERT_TRUE(ExecuteScript(shell(),
                            "document.title = "
                            " window.matchMedia('(display-mode:"
                            " minimal-ui)').matches"));
  EXPECT_EQ(base::ASCIIToUTF16("true"), shell()->web_contents()->GetTitle());

  delegate.set_mode(blink::mojom::DisplayMode::kFullscreen);
  // Simulate widget is entering fullscreen (changing size is enough).
  shell()
      ->web_contents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->SynchronizeVisualProperties();

  ASSERT_TRUE(ExecuteScript(shell(),
                            "document.title = "
                            " window.matchMedia('(display-mode:"
                            " fullscreen)').matches"));
  EXPECT_EQ(base::ASCIIToUTF16("true"), shell()->web_contents()->GetTitle());
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
    ON_CALL(*this, OnPageScaleFactorChanged(::testing::_)).WillByDefault(
        ::testing::InvokeWithoutArgs(
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
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.open('" + kViewSourceURL.spec() + "');"));
  console_observer.Wait();
  // Original page shouldn't navigate away, no new tab should be opened.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
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

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "window.location = '" + kViewSourceURL.spec() + "';"));
  console_observer.Wait();
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
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
    EXPECT_TRUE(
        ExecuteScript(shell(), "window.open('about:blank','new_window');"));

    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    EXPECT_EQ("new_window",
              static_cast<WebContentsImpl*>(new_shell->web_contents())
                  ->GetFrameTree()->root()->frame_name());

    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        new_shell,
        "window.domAutomationController.send(window.name == 'new_window');",
        &success));
    EXPECT_TRUE(success);
  }

  {
    ShellAddedObserver new_shell_observer;

    // Test clicking a target=foo link.
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "window.domAutomationController.send(clickSameSiteTargetedLink());",
        &success));
    EXPECT_TRUE(success);

    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    EXPECT_EQ("foo",
              static_cast<WebContentsImpl*>(new_shell->web_contents())
                  ->GetFrameTree()->root()->frame_name());
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

  EXPECT_EQ(shell2->web_contents(),
            WebContents::FromRenderFrameHost(
                shell3->web_contents()->GetOriginalOpener()));
  EXPECT_EQ(shell1->web_contents(),
            WebContents::FromRenderFrameHost(
                shell2->web_contents()->GetOriginalOpener()));

  shell2->Close();

  EXPECT_EQ(shell1->web_contents(),
            WebContents::FromRenderFrameHost(
                shell3->web_contents()->GetOriginalOpener()));
}

// TODO(clamy): Make the test work on Windows and on Mac. On Mac and Windows,
// there seem to be an issue with the ShellJavascriptDialogManager.
// Flaky on all platforms: https://crbug.com/655628
// Test that if a BeforeUnload dialog is destroyed due to the commit of a
// cross-site navigation, it will not reset the loading state.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_NoResetOnBeforeUnloadCanceledOnCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kStartURL(
      embedded_test_server()->GetURL("/hang_before_unload.html"));
  const GURL kCrossSiteURL(
      embedded_test_server()->GetURL("bar.com", "/title1.html"));

  // Navigate to a first web page with a BeforeUnload event listener.
  EXPECT_TRUE(NavigateToURL(shell(), kStartURL));

  // Start a cross-site navigation that will not commit for the moment.
  TestNavigationManager cross_site_delayer(shell()->web_contents(),
                                           kCrossSiteURL);
  shell()->LoadURL(kCrossSiteURL);
  EXPECT_TRUE(cross_site_delayer.WaitForRequestStart());

  // Click on a link in the page. This will show the BeforeUnload dialog.
  // Ensure the dialog is not dismissed, which will cause it to still be
  // present when the cross-site navigation later commits.
  // Note: the javascript function executed will not do the link click but
  // schedule it for afterwards. Since the BeforeUnload event is synchronous,
  // clicking on the link right away would cause the ExecuteScript to never
  // return.
  SetShouldProceedOnBeforeUnload(shell(), false, false);
  EXPECT_TRUE(ExecuteScript(shell(), "clickLinkSoon()"));
  WaitForAppModalDialog(shell());

  // Have the cross-site navigation commit. The main RenderFrameHost should
  // still be loading after that.
  cross_site_delayer.WaitForNavigationFinished();
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
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(shell->web_contents()->GetMainFrame());
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
      ->GetMainFrame()
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
                       SuddenTerminationDisablerOnVisibilityChange) {
  const std::string VISIBILITYCHANGE_HTML =
      "<html><body><script>document.onvisibilitychange=function(e) {}</script>"
      "</body></html>";
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
      "document.addEventListener('visibilitychange', handleEverything);"
      "window.removeEventListener('unload', handleEverything);"
      "window.removeEventListener('beforeunload', handleEverything);"
      "window.removeEventListener('pagehide', handleEverything);"
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
      !base::FeatureList::IsEnabled(features::kWebContentsOcclusion) ||
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

  void WillWaitForDialog() { waiting_for_ = kDialog; }
  void WillWaitForNewContents() { waiting_for_ = kNewContents; }
  void WillWaitForFullscreenExit() { waiting_for_ = kFullscreenExit; }

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
    is_fullscreen_ = true;
  }

  void ExitFullscreenModeForTab(WebContents*) override {
    is_fullscreen_ = false;

    if (waiting_for_ == kFullscreenExit) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return is_fullscreen_;
  }

  void AddNewContents(WebContents* source,
                      std::unique_ptr<WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override {
    popups_.push_back(std::move(new_contents));

    if (waiting_for_ == kNewContents) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
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
    std::move(callback).Run(true, base::string16());

    if (waiting_for_ == kDialog) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents,
                     bool reset_state) override {}

 private:
  WebContentsImpl* web_contents_;
  WebContentsDelegate* old_delegate_;

  enum {
    kNothing,
    kDialog,
    kNewContents,
    kFullscreenExit
  } waiting_for_ = kNothing;

  std::string last_message_;

  bool is_fullscreen_ = false;

  std::vector<std::unique_ptr<WebContents>> popups_;

  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();

  DISALLOW_COPY_AND_ASSIGN(TestWCDelegateForDialogsAndFullscreen);
};

class MockFileSelectListener : public FileChooserImpl::FileSelectListenerImpl {
 public:
  MockFileSelectListener() : FileChooserImpl::FileSelectListenerImpl(nullptr) {
    SetListenerFunctionCalledTrueForTesting();
  }
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {}
  void FileSelectionCanceled() override {}

 private:
  ~MockFileSelectListener() override = default;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       JavaScriptDialogsInMainAndSubframes) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(WaitForLoadStop(wc));

  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(0U, root->child_count());

  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  url::Replacements<char> clear_port;
  clear_port.ClearPort();

  // A dialog from the main frame.
  std::string alert_location = "alert(document.location)";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // A dialog from the subframe.
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(
      content::ExecuteScript(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ("about:blank", test_delegate.last_message());

  // Navigate the subframe cross-site.
  NavigateFrameToURL(frame,
                     embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  // A dialog from the subframe.
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(
      content::ExecuteScript(frame->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://b.com/title2.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // A dialog from the main frame.
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://a.com/title1.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // Navigate the top frame cross-site; ensure that dialogs work.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("c.com", "/title3.html")));
  EXPECT_TRUE(WaitForLoadStop(wc));
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
  test_delegate.Wait();
  EXPECT_EQ(GURL("http://c.com/title3.html"),
            GURL(test_delegate.last_message()).ReplaceComponents(clear_port));

  // Navigate back; ensure that dialogs work.
  wc->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(wc));
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(
      content::ExecuteScript(root->current_frame_host(), alert_location));
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
  EXPECT_TRUE(content::ExecuteScript(wc, alert));
  test_delegate.Wait();
  EXPECT_EQ("1\n2\n3\n4", test_delegate.last_message());
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
  ASSERT_TRUE(web_contents->GetMainFrame());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
  RenderProcessHost* process = web_contents->GetMainFrame()->GetProcess();
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
  EXPECT_EQ(process, web_contents->GetMainFrame()->GetProcess());
  EXPECT_EQ(renderer_id, web_contents->GetMainFrame()->GetProcess()->GetID());
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
    ASSERT_TRUE(web_contents->GetMainFrame());
    EXPECT_FALSE(web_contents->GetMainFrame()->IsRenderFrameLive());
    EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
    RenderProcessHost* process = web_contents->GetMainFrame()->GetProcess();
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
    EXPECT_EQ(process, web_contents->GetMainFrame()->GetProcess());
    EXPECT_EQ(renderer_id, web_contents->GetMainFrame()->GetProcess()->GetID());

    // Verify that the navigation succeeded.
    EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
    NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(url, entry->GetURL());
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NavigatingToWebUIDoesNotUsePreWarmedProcess) {
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

  ASSERT_TRUE(web_contents->GetMainFrame());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(web_contents->GetController().IsInitialBlankNavigation());
  int renderer_id = web_contents->GetMainFrame()->GetProcess()->GetID();

  TestNavigationObserver same_tab_observer(web_contents.get(), 1);
  NavigationController::LoadURLParams params(web_ui_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);
  same_tab_observer.Wait();

  // Check that pre-warmed process isn't used.
  EXPECT_NE(renderer_id, web_contents->GetMainFrame()->GetProcess()->GetID());
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(web_ui_url, entry->GetURL());
}

namespace {

class DownloadImageObserver {
 public:
  MOCK_METHOD5(OnFinishDownloadImage, void(
      int id,
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
  scoped_refptr<MessageLoopRunner> loop_runner =
      new MessageLoopRunner();

  // Set up expectation and stub.
  EXPECT_CALL(download_image_observer,
              OnFinishDownloadImage(_, expected_http_status, _,
                                    SizeIs(expected_number_of_images), _));
  ON_CALL(download_image_observer, OnFinishDownloadImage(_, _, _, _, _))
      .WillByDefault(
          InvokeWithoutArgs(loop_runner.get(), &MessageLoopRunner::Quit));

  shell->LoadURL(GURL("about:blank"));
  shell->web_contents()->DownloadImage(
      image_url, false, 0, 1024, false,
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
                                    int expected_size,
                                    int id,
                                    int status_code,
                                    const GURL& image_url,
                                    const std::vector<SkBitmap>& bitmap,
                                    const std::vector<gfx::Size>& sizes) {
  EXPECT_EQ(200, status_code);
  ASSERT_EQ(bitmap.size(), 1u);
  EXPECT_EQ(bitmap[0].width(), expected_size);
  EXPECT_EQ(bitmap[0].height(), expected_size);
  ASSERT_EQ(sizes.size(), 1u);
  EXPECT_EQ(sizes[0].width(), expected_size);
  EXPECT_EQ(sizes[0].height(), expected_size);
  std::move(quit_closure).Run();
}

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_HttpImage) {
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

  const GURL kImageUrl =
      GetTestUrl("", "image.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DownloadImage_NoValidImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/invalid.ico");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, 0, 2, false,
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
      kImageUrl, false, 30, 1024, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     30));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredSizeZero) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, 0, 1024, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     90));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_PreferredSizeClampedByMaxSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/rgb.svg");
  shell()->LoadURL(GURL("about:blank"));
  base::RunLoop run_loop;
  shell()->web_contents()->DownloadImage(
      kImageUrl, false, 60, 30, false,
      base::BindOnce(&ExpectSingleValidImageCallback, run_loop.QuitClosure(),
                     30));

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
      kImageUrl, false, 0, 0, false,
      base::BindOnce(&ExpectTwoValidImageCallback, run_loop.QuitClosure(),
                     expected_sizes));

  run_loop.Run();
}

class MouseLockDelegate : public WebContentsDelegate {
 public:
  // WebContentsDelegate:
  void RequestToLockMouse(WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override {
    request_to_lock_mouse_called_ = true;
  }
  bool request_to_lock_mouse_called_ = false;
};

// TODO(crbug.com/898641): This test is flaky.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DISABLED_RenderWidgetDeletedWhileMouseLockPending) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<MouseLockDelegate> delegate(new MouseLockDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Try to request pointer lock. WebContentsDelegate should get a notification.
  ASSERT_TRUE(ExecuteScript(shell(),
                            "window.domAutomationController.send(document.body."
                            "requestPointerLock());"));
  EXPECT_TRUE(delegate.get()->request_to_lock_mouse_called_);

  // Make sure that the renderer didn't get the pointer lock, since the
  // WebContentsDelegate didn't approve the notification.
  bool locked = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(shell(),
                                          "window.domAutomationController.send("
                                          "document.pointerLockElement == "
                                          "null);",
                                          &locked));
  EXPECT_TRUE(locked);

  // Try to request the pointer lock again. Since there's a pending request in
  // WebContentsDelelgate, the WebContents shouldn't ask again.
  delegate.get()->request_to_lock_mouse_called_ = false;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "window.domAutomationController.send(document.body."
                            "requestPointerLock());"));
  EXPECT_FALSE(delegate.get()->request_to_lock_mouse_called_);

  // Force a cross-process navigation so that the RenderWidgetHost is deleted.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // Make sure the WebContents cleaned up the previous pending request. A new
  // request should be forwarded to the WebContentsDelegate.
  delegate.get()->request_to_lock_mouse_called_ = false;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "window.domAutomationController.send(document.body."
                            "requestPointerLock());"));
  EXPECT_TRUE(delegate.get()->request_to_lock_mouse_called_);
}

// Checks that user agent override string is only used when it's overridden.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, UserAgentOverride) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string kHeaderPath =
      std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
  const GURL kUrl(embedded_test_server()->GetURL(kHeaderPath));
  const std::string kUserAgentOverride = "foo";

  EXPECT_TRUE(NavigateToURL(shell(), kUrl));
  std::string header_value;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_NE(kUserAgentOverride, header_value);

  shell()->web_contents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly("foo"), false);
  EXPECT_TRUE(NavigateToURL(shell(), kUrl));
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_NE(kUserAgentOverride, header_value);

  shell()
      ->web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  tab_observer.Wait();
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_EQ(kUserAgentOverride, header_value);
}

// Changes the WebContents and active entry user agent override from
// DidStartNavigation().
// in WebContentsObserver::DidStartNavigation().
class UserAgentInjector : public WebContentsObserver {
 public:
  UserAgentInjector(WebContents* web_contents, const std::string& user_agent)
      : UserAgentInjector(web_contents,
                          blink::UserAgentOverride::UserAgentOnly(user_agent),
                          true) {}

  UserAgentInjector(WebContents* web_contents,
                    const blink::UserAgentOverride& ua_override,
                    bool is_overriding_user_agent = true)
      : WebContentsObserver(web_contents),
        user_agent_override_(ua_override),
        is_overriding_user_agent_(is_overriding_user_agent) {}

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    web_contents()->SetUserAgentOverride(user_agent_override_, false);
    navigation_handle->SetIsOverridingUserAgent(is_overriding_user_agent_);
  }

 private:
  const blink::UserAgentOverride user_agent_override_;
  const bool is_overriding_user_agent_ = true;
};

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

  // This triggers creating a NavigationRequest without a NavigationEntry. More
  // specifically back() triggers creating a pending entry, and because back()
  // does not complete, the reload() call results in a NavigationRequest with no
  // NavigationEntry.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.back(); location.reload();"));

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
    scoped_feature_list_.InitAndEnableFeature(features::kUserAgentClientHint);
    WebContentsImplBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  const std::string mobile_id =
      blink::kClientHintsHeaderMapping[static_cast<int>(
          network::mojom::WebClientHintsType::kUAMobile)];
  ASSERT_TRUE(base::Contains(http_response.http_request()->headers, mobile_id));
  // "?!" corresponds to "mobile=true".
  EXPECT_EQ("?1", http_response.http_request()->headers.at(mobile_id));
  ShellContentBrowserClient::Get()
      ->browser_context()
      ->set_client_hints_controller_delegate(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DialogsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // alert
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  std::string script = "alert('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());

  // confirm
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  script = "confirm('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());

  // prompt
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  script = "prompt('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());

  // beforeunload
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  // Disable the hang monitor (otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer) and give the page a
  // gesture to allow dialogs.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  script = "window.onbeforeunload=function(e){ return 'x' };";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
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

  FrameTreeNode* root = top_contents->GetFrameTree()->root();
  ASSERT_EQ(0U, root->child_count());

  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(content::ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1U, root->child_count());
  RenderFrameHost* frame = root->child_at(0)->current_frame_host();
  ASSERT_NE(nullptr, frame);

  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(frame));
  TestWCDelegateForDialogsAndFullscreen inner_test_delegate(inner_contents);

  // A dialog from the inner WebContents should make the outer contents lose
  // fullscreen.
  top_contents->EnterFullscreenMode(top_contents->GetMainFrame(), {});
  EXPECT_TRUE(top_contents->IsFullscreen());
  script = "alert('hi')";
  inner_test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(inner_contents, script));
  inner_test_delegate.Wait();
  EXPECT_FALSE(top_contents->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FileChooserEndsFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(wc);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  wc->RunFileChooser(wc->GetMainFrame(),
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
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());
  std::string script = "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());
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
  EXPECT_TRUE(content::ExecuteScript(wc, popup_script));
  test_delegate.Wait();
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(test_delegate.last_popup());

  // Put the original page into fullscreen.
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());

  // Have the popup open a popup.
  TestWCDelegateForDialogsAndFullscreen popup_test_delegate(popup);
  popup_test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(content::ExecuteScript(popup, popup_script));
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
  EXPECT_TRUE(content::ExecuteScript(wc, popup_script));
  test_delegate.Wait();
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(test_delegate.last_popup());

  // Put the popup into fullscreen.
  TestWCDelegateForDialogsAndFullscreen popup_test_delegate(popup);
  popup->EnterFullscreenMode(popup->GetMainFrame(), {});
  EXPECT_TRUE(popup->IsFullscreen());

  // Have the original page open a new popup.
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(content::ExecuteScript(wc, popup_script));
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
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();

  // Put the main contents into fullscreen ...
  wc->EnterFullscreenMode(wc->GetMainFrame(), {});
  EXPECT_TRUE(wc->IsFullscreen());

  // ... and ensure that a call to window.focus() from it causes loss of
  // ... fullscreen.
  script = "window.FocusFromJavaScriptEndsFullscreen.focus()";
  test_delegate.WillWaitForFullscreenExit();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreen());
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
  ASSERT_TRUE(ExecuteScript(web_contents, "window[0].focus();"));
  FrameTree* frame_tree = web_contents->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  ASSERT_EQ(root->child_at(0), frame_tree->GetFocusedFrame());
  shell()->web_contents()->Copy();

  TitleWatcher title_watcher(web_contents, base::ASCIIToUTF16("done"));
  base::string16 title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, base::ASCIIToUTF16("done"));
}

class UpdateTargetURLWaiter : public WebContentsDelegate {
 public:
  explicit UpdateTargetURLWaiter(WebContents* web_contents) {
    web_contents->SetDelegate(this);
  }

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

  base::Optional<GURL> updated_target_url_;
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(UpdateTargetURLWaiter);
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
  FrameTreeNode* subframe = web_contents->GetFrameTree()->root()->child_at(0);
  GURL subframe_url =
      embedded_test_server()->GetURL("b.com", "/simple_links.html");
  NavigateFrameToURL(subframe, subframe_url);

  // Focusing the link should fire the UpdateTargetURL notification.
  UpdateTargetURLWaiter target_url_waiter(web_contents);
  EXPECT_TRUE(ExecuteScript(
      subframe, "document.getElementById('cross_site_link').focus();"));
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

  // Waits until the WebContents changes its LoadStateHost to |host|.
  void Wait(net::LoadState load_state, const base::string16& host) {
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
  content::WebContents* web_contents_ = nullptr;
  base::string16 waiting_host_;
  net::LoadState waiting_state_;

  DISALLOW_COPY_AND_ASSIGN(LoadStateWaiter);
};

}  // namespace

// TODO(csharrison,mmenke):  Beef up testing of LoadState a little. In
// particular, check upload progress and check the LoadState param.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, DISABLED_UpdateLoadState) {
  base::string16 a_host = url_formatter::IDNToUnicode("a.com");
  base::string16 b_host = url_formatter::IDNToUnicode("b.com");
  base::string16 paused_host = url_formatter::IDNToUnicode("paused.com");

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
  FrameTreeNode* a_frame = web_contents->GetFrameTree()->root();
  FrameTreeNode* b_frame = a_frame->child_at(0);

  // Start loading the respective resources in each frame.
  auto load_resource = [](FrameTreeNode* frame, const std::string url) {
    const char kLoadResourceScript[] = R"(
      var img = new Image();
      img.src = '%s';
      document.body.appendChild(img);
    )";
    std::string script = base::StringPrintf(kLoadResourceScript, url.c_str());
    EXPECT_TRUE(ExecuteScript(frame, script));
  };

  // There should be no outgoing requests, so the load state should be empty.
  waiter.Wait(net::LOAD_STATE_IDLE, base::string16());

  // The |frame_pauser| pauses the navigation after every step. It will only
  // finish by calling WaitForNavigationFinished or ResumeNavigation.
  GURL paused_url(embedded_test_server()->GetURL("paused.com", "/title1.html"));
  TestNavigationManager frame_pauser(web_contents, paused_url);
  const char kLoadFrameScript[] = R"(
    var frame = document.createElement('iframe');
    frame.src = "%s";
    document.body.appendChild(frame);
  )";
  EXPECT_TRUE(ExecuteScript(
      web_contents,
      base::StringPrintf(kLoadFrameScript, paused_url.spec().c_str())));

  // Wait for the response to be ready, but never finish it.
  EXPECT_TRUE(frame_pauser.WaitForResponse());
  EXPECT_FALSE(frame_pauser.was_successful());
  // Note: the pausing only works for the non-network service path because of
  // http://crbug.com/791049.
  waiter.Wait(net::LOAD_STATE_IDLE, base::string16());

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

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, NotifyPreferencesChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();

  // Navigate to a site with two iframes in different origins.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* main_frame_rvh = main_frame->GetRenderViewHost();

  NotifyPreferencesChangedTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  web_contents->NotifyPreferencesChanged();

  // We should have updated the preferences for the WebContents, and should call
  // OverrideWebkitPrefs with the main RenderViewHost only (not subframe RVHs).
  EXPECT_EQ(std::unordered_set<RenderViewHost*>({main_frame_rvh}),
            new_client.override_webkit_prefs_rvh_set());

  SetBrowserClientForTesting(old_client);
}

namespace {

class OutgoingSetRendererPrefsIPCWatcher {
 public:
  OutgoingSetRendererPrefsIPCWatcher(RenderProcessHostImpl* rph)
      : rph_(rph), outgoing_message_seen_(false) {
    rph_->SetIpcSendWatcherForTesting(
        base::BindRepeating(&OutgoingSetRendererPrefsIPCWatcher::OnMessage,
                            base::Unretained(this)));
  }
  ~OutgoingSetRendererPrefsIPCWatcher() {
    rph_->SetIpcSendWatcherForTesting(
        base::RepeatingCallback<void(const IPC::Message& msg)>());
  }

  void WaitForIPC() {
    if (outgoing_message_seen_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  const blink::mojom::RendererPreferences& renderer_preferences() const {
    return renderer_preferences_;
  }

 private:
  void OnMessage(const IPC::Message& message) {
    IPC_BEGIN_MESSAGE_MAP(OutgoingSetRendererPrefsIPCWatcher, message)
      IPC_MESSAGE_HANDLER(PageMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_END_MESSAGE_MAP()
  }

  void OnSetRendererPrefs(
      const blink::mojom::RendererPreferences& renderer_prefs) {
    outgoing_message_seen_ = true;
    renderer_preferences_ = renderer_prefs;
    if (run_loop_)
      run_loop_->Quit();
  }

  RenderProcessHostImpl* rph_;
  bool outgoing_message_seen_;
  std::unique_ptr<base::RunLoop> run_loop_;
  blink::mojom::RendererPreferences renderer_preferences_;
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
  blink::mojom::RendererPreferences* renderer_preferences =
      web_contents->GetMutableRendererPrefs();
  const bool use_custom_colors_old = renderer_preferences->use_custom_colors;

  // Retrieve all unique render process hosts.
  std::vector<RenderProcessHostImpl*> render_process_hosts;
  for (FrameTreeNode* frame_tree_node : web_contents->GetFrameTree()->Nodes()) {
    RenderProcessHostImpl* render_process_host =
        static_cast<RenderProcessHostImpl*>(
            frame_tree_node->current_frame_host()->GetProcess());
    ASSERT_NE(nullptr, render_process_host);
    DLOG(INFO) << "render_process_host=" << render_process_host;

    // It's possible (Android e.g.) for frame hosts to share a
    // RenderProcessHost.
    if (std::find(render_process_hosts.begin(), render_process_hosts.end(),
                  render_process_host) == render_process_hosts.end()) {
      render_process_hosts.push_back(render_process_host);
    }
  }

  // Set up watchers for PageMsg_SetRendererPrefs message being sent from unique
  // render process hosts.
  std::vector<std::unique_ptr<OutgoingSetRendererPrefsIPCWatcher>> ipc_watchers;
  for (auto* render_process_host : render_process_hosts) {
    ipc_watchers.push_back(std::make_unique<OutgoingSetRendererPrefsIPCWatcher>(
        render_process_host));

    // Make sure the IPC watchers have the same default value for the arbitrary
    // preference.
    EXPECT_EQ(use_custom_colors_old,
              ipc_watchers.back()->renderer_preferences().use_custom_colors);
  }

  // Change the arbitrary renderer preference.
  const bool use_custom_colors_new = !use_custom_colors_old;
  renderer_preferences->use_custom_colors = use_custom_colors_new;
  web_contents->SyncRendererPrefs();

  // Ensure IPC is sent to each frame.
  for (auto& ipc_watcher : ipc_watchers) {
    ipc_watcher->WaitForIPC();
    EXPECT_EQ(use_custom_colors_new,
              ipc_watcher->renderer_preferences().use_custom_colors);
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
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(),
        "domAutomationController.send(document.getElementById('textfield')."
        "value.length)",
        &text_length));

    // Wait until |text_length| exceed 0.
    if (text_length > 0)
      break;
  }

  // Freeze the blink page.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(true);

  // Make the javascript work.
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(),
        "domAutomationController.send(document.getElementById('textfield')."
        "value.length)",
        &text_length));
  }

  // Check if |next_text_length| is equal to |text_length|.
  int next_text_length;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(),
      "domAutomationController.send(document.getElementById('textfield')."
      "value.length)",
      &next_text_length));
  EXPECT_EQ(text_length, next_text_length);

  // Wake the frozen page up.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(false);

  // Wait for an amount of time in order to give the javascript time to
  // work again. If the javascript doesn't work again, the test will fail due to
  // the time out.
  while (true) {
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(),
        "domAutomationController.send(document.getElementById('textfield')."
        "value.length)",
        &next_text_length));
    if (next_text_length > text_length)
      break;
  }

  // Check if |next_text_length| exceeds |text_length| because the blink
  // schedule tasks have resumed.
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      shell(),
      "domAutomationController.send(document.getElementById('textfield')."
      "value.length)",
      &next_text_length));
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
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_a->child_at(1)->current_frame_host();
  RenderFrameDeletedObserver delete_rfh_b(rfh_b);
  RenderFrameDeletedObserver delete_rfh_c(rfh_c);

  // Delete an iframe when the page is active(not frozen), which should succeed.
  rfh_b->Send(new UnfreezableFrameMsg_Delete(
      rfh_b->routing_id(), FrameDeleteIntention::kNotMainFrame));
  delete_rfh_b.WaitUntilDeleted();
  EXPECT_TRUE(delete_rfh_b.deleted());
  EXPECT_FALSE(delete_rfh_c.deleted());

  // Freeze the blink page.
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->SetPageFrozen(true);

  // Try to delete an iframe, and succeeds because the message is unfreezable.
  rfh_c->Send(new UnfreezableFrameMsg_Delete(
      rfh_c->routing_id(), FrameDeleteIntention::kNotMainFrame));
  delete_rfh_c.WaitUntilDeleted();
  EXPECT_TRUE(delete_rfh_c.deleted());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupWindowBrowserNavResumeLoad) {
  // This test verifies a pop up that requires navigation from browser side
  // works with a delegate that delays navigations of pop ups.
  // Create a file: scheme pop up from a file: scheme page, which requires
  // requires an OpenURL IPC to the browser process.
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  base::FilePath simple_links_path =
      test_data_dir.Append(GetTestDataFilePath())
          .Append(FILE_PATH_LITERAL("simple_links.html"));
  GURL url(base::FilePath::StringType(FILE_PATH_LITERAL("file://")) +
           simple_links_path.value());

  shell()->set_delay_popup_contents_delegate_for_testing(true);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  Shell* new_shell = nullptr;
  WebContents* new_contents = nullptr;
  {
    ShellAddedObserver new_shell_observer;
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickLinkToSelf());",
        &success));
    new_shell = new_shell_observer.GetShell();
    new_contents = new_shell->web_contents();
    // Delaying popup holds the initial load.
    EXPECT_FALSE(WaitForLoadStop(new_contents));
  }

  EXPECT_FALSE(new_contents->GetDelegate());
  new_contents->SetDelegate(new_shell);
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
  RenderFrameHost* wanted_rfh_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenWebContentsObserver);
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
  RenderFrameHostImpl* main_frame = web_contents->GetMainFrame();
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));

  std::set<RenderFrameHostImpl*> fullscreen_frames;
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(
        ExecuteScript(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(main_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame, web_contents->current_fullscreen_frame_);

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecuteScript(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(child_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(child_frame, web_contents->current_fullscreen_frame_);

  // Exit fullscreen on the child frame.
  // This will not work with --site-per-process until crbug.com/617369
  // is fixed.
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    {
      FullscreenWebContentsObserver observer(web_contents, main_frame);
      EXPECT_TRUE(
          ExecuteScript(child_frame, "document.webkitExitFullscreen();"));
      observer.Wait();
    }

    fullscreen_frames.erase(child_frame);
    EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
    EXPECT_EQ(main_frame, web_contents->current_fullscreen_frame_);
  }
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, RejectFullscreenIfBlocked) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate(web_contents);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* main_frame = web_contents->GetMainFrame();

  EXPECT_TRUE(ExecuteScript(
      main_frame,
      "document.body.onfullscreenchange = "
      "function (event) { document.title = 'onfullscreenchange' };"));
  EXPECT_TRUE(ExecuteScript(
      main_frame,
      "document.body.onfullscreenerror = "
      "function (event) { document.title = 'onfullscreenerror' };"));

  TitleWatcher title_watcher(web_contents,
                             base::ASCIIToUTF16("onfullscreenchange"));
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("onfullscreenerror"));

  // While the |fullscreen_block| is in scope, fullscreen should fail with an
  // error.
  base::ScopedClosureRunner fullscreen_block =
      web_contents->ForSecurityDropFullscreen();

  EXPECT_TRUE(ExecuteScript(main_frame, "document.body.requestFullscreen();"));

  base::string16 title = title_watcher.WaitAndGetTitle();
  ASSERT_EQ(title, base::ASCIIToUTF16("onfullscreenerror"));
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
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  EXPECT_EQ(0u, web_contents->fullscreen_frames_.size());

  // 2) Make it fullscreen.
  FullscreenWebContentsObserver observer(web_contents, main_frame);
  EXPECT_TRUE(
      ExecuteScript(main_frame, "document.body.webkitRequestFullscreen();"));
  observer.Wait();
  EXPECT_EQ(1u, web_contents->fullscreen_frames_.size());

  // 3) Navigate cross origin. Act as if the old frame was very slow delivering
  //    the unload ack and stayed in pending deletion for a while. Even if the
  //    frame is still present, it must be removed from the list of frame in
  //    fullscreen immediately.
  auto filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Unload_ACK::ID);
  main_frame->GetProcess()->AddFilter(filter.get());
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
  RenderFrameHostImpl* main_frame = web_contents->GetMainFrame();
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));

  std::set<RenderFrameHostImpl*> nodes;
  EXPECT_EQ(nodes, web_contents->fullscreen_frames_);
  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(
        ExecuteScript(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  nodes.insert(main_frame);
  EXPECT_EQ(nodes, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame, web_contents->current_fullscreen_frame_);

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecuteScript(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  nodes.insert(child_frame);
  EXPECT_EQ(nodes, web_contents->fullscreen_frames_);
  EXPECT_EQ(child_frame, web_contents->current_fullscreen_frame_);

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
  RenderFrameHostImpl* main_frame = web_contents->GetMainFrame();
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));

  std::set<RenderFrameHostImpl*> fullscreen_frames;
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_FALSE(IsInFullscreen());

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(
        ExecuteScript(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(main_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame, web_contents->current_fullscreen_frame_);

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecuteScript(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.insert(child_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(child_frame, web_contents->current_fullscreen_frame_);

  // Exit fullscreen on the child frame.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecuteScript(child_frame, "document.webkitExitFullscreen();"));
    observer.Wait();
  }

  fullscreen_frames.erase(child_frame);
  EXPECT_EQ(fullscreen_frames, web_contents->fullscreen_frames_);
  EXPECT_EQ(main_frame, web_contents->current_fullscreen_frame_);
}

class MockDidOpenRequestedURLObserver : public WebContentsObserver {
 public:
  explicit MockDidOpenRequestedURLObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()) {}

  MOCK_METHOD8(DidOpenRequestedURL,
               void(WebContents* new_contents,
                    RenderFrameHost* source_render_frame_host,
                    const GURL& url,
                    const Referrer& referrer,
                    WindowOpenDisposition disposition,
                    ui::PageTransition transition,
                    bool started_from_context_menu,
                    bool renderer_initiated));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDidOpenRequestedURLObserver);
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
  RenderFrameHost* subframe = shell()->web_contents()->GetAllFrames()[1];
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
  EXPECT_TRUE(ExecuteScript(
      shell(), "window.domAutomationController.send(ctrlClickLink());"));
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
      attached_web_contents->GetBrowserContext(), nullptr /* site_instance */);
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
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* root_web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = root_web_contents->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child_to_replace = root->child_at(0);
  auto* child_to_replace_rfh = child_to_replace->current_frame_host();

  WebContents::CreateParams inner_params(
      root_web_contents->GetBrowserContext());

  std::unique_ptr<WebContents> child_contents_ptr =
      WebContents::Create(inner_params);
  auto* child_rfh =
      static_cast<RenderFrameHostImpl*>(child_contents_ptr->GetMainFrame());

  std::unique_ptr<WebContents> grandchild_contents_ptr =
      WebContents::Create(inner_params);

  // Attach grandchild to child.
  child_contents_ptr->AttachInnerWebContents(
      std::move(grandchild_contents_ptr), child_rfh, false /* is_full_page */);

  // At this point the child hasn't been attached to the root.
  EXPECT_EQ(1U, root_web_contents->GetInputEventRouter()
                    ->RegisteredViewCountForTesting());

  // Attach child+grandchild subtree to root.
  root_web_contents->AttachInnerWebContents(std::move(child_contents_ptr),
                                            child_to_replace_rfh,
                                            false /* is_full_page */);

  // Verify views registered for both child and grandchild.
  EXPECT_EQ(3U, root_web_contents->GetInputEventRouter()
                    ->RegisteredViewCountForTesting());
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
      attached_web_contents->GetBrowserContext(), /*site_instance=*/nullptr);
  std::unique_ptr<WebContents> public_web_contents =
      WebContents::Create(create_params);
  auto* web_contents = static_cast<WebContentsImpl*>(public_web_contents.get());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // Complete a navigation.
  GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(web_contents, url1));

  // Start navigating to a second page.
  GURL url2 = embedded_test_server()->GetURL("b.com", "/title2.html");
  TestNavigationManager manager(web_contents, url2);
  web_contents->GetController().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // While there is a speculative RenderFrameHost in the root FrameTreeNode...
  ASSERT_TRUE(root->render_manager()->speculative_frame_host());

  auto* frame_process = static_cast<RenderProcessHostImpl*>(
      root->render_manager()->speculative_frame_host()->GetProcess());
  int frame_routing_id =
      root->render_manager()->speculative_frame_host()->GetRoutingID();

  std::vector<int> deleted_routing_ids;
  auto watcher = base::BindRepeating(
      [](std::vector<int>* deleted_routing_ids, const IPC::Message& message) {
        if (message.type() == UnfreezableFrameMsg_Delete::ID) {
          deleted_routing_ids->push_back(message.routing_id());
        }
      },
      &deleted_routing_ids);
  frame_process->SetIpcSendWatcherForTesting(watcher);

  // ...shutdown the WebContents.
  public_web_contents.reset();

  // What should have happened is the speculative RenderFrameHost deletes the
  // provisional RenderFrame. The |watcher| verifies that this happened.
  EXPECT_THAT(deleted_routing_ids, testing::Contains(frame_routing_id));
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

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, MouseButtonsDontNavigate) {
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
  EXPECT_TRUE(content::ExecuteScript(shell(),
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  EXPECT_TRUE(content::ExecuteScript(shell(),
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }
  ASSERT_EQ(url_a, web_contents->GetLastCommittedURL());
}

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
  auto* rfh =
      static_cast<RenderFrameHostImpl*>(CreateSubframe(url_with_iframes));

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
  auto* rfh = static_cast<RenderFrameHostImpl*>(CreateSubframe(url));
  ASSERT_NE(rfh, nullptr);
  EXPECT_EQ(web_contents->max_loaded_frame_count_, 3u);

  // Let's remove the first child.
  auto* main_frame = web_contents->GetMainFrame();
  auto* node_to_remove = main_frame->child_at(0);
  FrameDeletedObserver observer(node_to_remove->current_frame_host());
  EXPECT_TRUE(ExecuteScript(main_frame,
                            "document.body.removeChild(document.querySelector('"
                            "iframe').parentNode);"));
  observer.Wait();

  EXPECT_EQ(web_contents->max_loaded_frame_count_, 3u);

  // Let's remove the second child.
  node_to_remove = main_frame->child_at(0);
  FrameDeletedObserver observer_second(node_to_remove->current_frame_host());
  EXPECT_TRUE(ExecuteScript(
      main_frame,
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

  void DocumentAvailableInMainFrame() override {
    events_.push_back("DocumentAvailableInMainFrame");
  }

  void DocumentOnLoadCompletedInMainFrame() override {
    events_.push_back("DocumentOnLoadCompletedInMainFrame");
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
              testing::ElementsAre(
                  "DidStartLoading", "DidStartNavigation",
                  "DidFinishNavigation", "DocumentAvailableInMainFrame",
                  "DOMContentLoaded", "DocumentOnLoadCompletedInMainFrame",
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
                       LoadingCallbacksOrder_ErrorPage) {
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
              testing::ElementsAre(
                  "DidStartLoading", "DidStartNavigation",
                  "DidFinishNavigation", "DocumentAvailableInMainFrame",
                  "DOMContentLoaded", "DidFinishLoad", "DidStartNavigation",
                  "DidFinishNavigation", "DocumentAvailableInMainFrame",
                  "DOMContentLoaded", "DocumentOnLoadCompletedInMainFrame",
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
  EXPECT_EQ(shell()->web_contents()->GetThemeColor(), base::nullopt);

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

  DISALLOW_COPY_AND_ASSIGN(DidChangeVerticalScrollDirectionObserver);
};

}  // namespace

// Tests that DidChangeVerticalScrollDirection is called only when the vertical
// scroll direction has changed and that it includes the correct details.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DidChangeVerticalScrollDirection) {
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
  render_frame_observer.WaitForScrollOffset(gfx::Vector2dF());

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
  auto ScaledVector2dF = [device_pixel_ratio](float x, float y) {
    return gfx::Vector2dF(std::floor(x * device_pixel_ratio),
                          std::floor(y * device_pixel_ratio));
  };

  // Scroll down.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5)"));
  render_frame_observer.WaitForScrollOffset(ScaledVector2dF(0.f, 5.f));

  // Assert that we are notified of the scroll down event.
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll down again.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 10)"));
  render_frame_observer.WaitForScrollOffset(ScaledVector2dF(0.f, 10.f));

  // Assert that we are *not* notified of the scroll down event given that no
  // change in scroll direction occurred (as our previous scroll was also down).
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll right.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(10, 10)"));
  render_frame_observer.WaitForScrollOffset(ScaledVector2dF(10.f, 10.f));

  // Assert that we are *not* notified of the scroll right event given that no
  // change occurred in the vertical direction.
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll left.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 10)"));
  render_frame_observer.WaitForScrollOffset(ScaledVector2dF(0.f, 10.f));

  // Assert that we are *not* notified of the scroll left event given that no
  // change occurred in the vertical direction.
  EXPECT_EQ(1, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            web_contents_observer.last_value());

  // Scroll up.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5)"));
  render_frame_observer.WaitForScrollOffset(ScaledVector2dF(0.f, 5.f));

  // Assert that we are notified of the scroll up event.
  EXPECT_EQ(2, web_contents_observer.call_count());
  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            web_contents_observer.last_value());

  // Scroll up again.
  EXPECT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 0)"));
  render_frame_observer.WaitForScrollOffset(ScaledVector2dF(0.f, 0.f));

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
  FrameTreeNode* iframe = web_contents->GetFrameTree()->root()->child_at(0);
  GURL iframe_url(server->GetURL("b.co", "/scrollable_page_with_content.html"));
  NavigateFrameToURL(iframe, iframe_url);

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

  EXPECT_FALSE(web_contents->ShouldIgnoreUnresponsiveRenderer());
  web_contents->IsClipboardPasteAllowed(
      GURL("https://google.com"), ui::ClipboardFormatType::GetPlainTextType(),
      "random pasted text",
      base::BindLambdaForTesting(
          [&web_contents](
              content::ContentBrowserClient::ClipboardPasteAllowed allowed) {
            EXPECT_TRUE(allowed);
            EXPECT_TRUE(web_contents->ShouldIgnoreUnresponsiveRenderer());
          }));
  EXPECT_FALSE(web_contents->ShouldIgnoreUnresponsiveRenderer());
}

// Intercept calls to RenderFramHostImpl's DidStopLoading mojo method.
class DidStopLoadingInterceptor : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit DidStopLoadingInterceptor(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host) {
    render_frame_host_->frame_host_receiver_for_testing().SwapImplForTesting(
        this);
  }

  ~DidStopLoadingInterceptor() override = default;

  mojom::FrameHost* GetForwardingInterface() override {
    return render_frame_host_;
  }

  void DidStopLoading() override {
    static_cast<RenderProcessHostImpl*>(render_frame_host_->GetProcess())
        ->mark_child_process_activity_time();
    static_cast<mojom::FrameHost*>(render_frame_host_)->DidStopLoading();
  }

 private:
  RenderFrameHostImpl* render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(DidStopLoadingInterceptor);
};

// Test that get_process_idle_time() returns reasonable values when compared
// with time deltas measured locally.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, RenderIdleTime) {
  EXPECT_TRUE(embedded_test_server()->Start());

  base::TimeTicks start = base::TimeTicks::Now();
  DidStopLoadingInterceptor interceptor(
      static_cast<content::RenderFrameHostImpl*>(
          shell()->web_contents()->GetMainFrame()));

  GURL test_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  base::TimeDelta renderer_td = shell()
                                    ->web_contents()
                                    ->GetMainFrame()
                                    ->GetProcess()
                                    ->GetChildProcessIdleTime();
  base::TimeDelta browser_td = base::TimeTicks::Now() - start;
  EXPECT_TRUE(browser_td >= renderer_td);
}

}  // namespace content
