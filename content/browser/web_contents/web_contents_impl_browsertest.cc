// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
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
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

#define SCOPE_TRACED(statement) \
  {                             \
    SCOPED_TRACE(#statement);   \
    statement;                  \
  }

void ResizeWebContentsView(Shell* shell, const gfx::Size& size,
                           bool set_start_page) {
  // Shell::SizeTo is not implemented on Aura; WebContentsView::SizeContents
  // works on Win and ChromeOS but not Linux - we need to resize the shell
  // window on Linux because if we don't, the next layout of the unchanged shell
  // window will resize WebContentsView back to the previous size.
  // SizeContents is a hack and should not be relied on.
#if defined(OS_MACOSX)
  shell->SizeTo(size);
  // If |set_start_page| is true, start with blank page to make sure resize
  // takes effect.
  if (set_start_page)
    NavigateToURL(shell, GURL("about://blank"));
#else
  static_cast<WebContentsImpl*>(shell->web_contents())->GetView()->
      SizeContents(size);
#endif  // defined(OS_MACOSX)
}

// Class to test that OverrideWebkitPrefs has been called for all relevant
// RenderViewHosts.
class NotifyPreferencesChangedTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  NotifyPreferencesChangedTestContentBrowserClient() = default;

  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           WebPreferences* prefs) override {
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
  gfx::Size GetSizeForNewRenderView(WebContents* web_contents) const override {
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
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
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
  NavigateToURL(shell(), url1);
  load_observer.Wait();

  EXPECT_EQ(url1, load_observer.url_);
  EXPECT_EQ(0, load_observer.session_index_);
  EXPECT_EQ(&shell()->web_contents()->GetController(),
            load_observer.controller_);
}

namespace {

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

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

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
#define MAYBE_GetSizeForNewRenderView GetSizeForNewRenderView
#endif
// Test that RenderViewHost is created and updated at the size specified by
// WebContentsDelegate::GetSizeForNewRenderView().
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       MAYBE_GetSizeForNewRenderView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create a new server with a different site.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  std::unique_ptr<RenderViewSizeDelegate> delegate(
      new RenderViewSizeDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  // When no size is set, RenderWidgetHostView adopts the size of
  // WebContentsView.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(shell()->web_contents()->GetContainerBounds().size(),
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());

  // When a size is set, RenderWidgetHostView and WebContentsView honor this
  // size.
  gfx::Size size(300, 300);
  gfx::Size size_insets(10, 15);
  ResizeWebContentsView(shell(), size, true);
  delegate->set_size_insets(size_insets);
  NavigateToURL(shell(), https_server.GetURL("/"));
  size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(size,
            shell()->web_contents()->GetRenderWidgetHostView()->GetViewBounds().
                size());
  // The web_contents size is set by the embedder, and should not depend on the
  // rwhv size. The behavior is correct on OSX, but incorrect on other
  // platforms.
  gfx::Size exp_wcv_size(300, 300);
#if !defined(OS_MACOSX)
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
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));
  // RenderWidgetHostView is created at specified size.
  init_size.Enlarge(size_insets.width(), size_insets.height());
  EXPECT_EQ(init_size, observer.rwhv_create_size());

// Once again, the behavior is correct on OSX. The embedder explicitly sets
// the size to (100,100) during navigation. Both the wcv and the rwhv should
// take on that size.
#if !defined(OS_MACOSX)
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
  NavigateToURL(shell(), url);
  ASSERT_EQ(1, shell()->web_contents()->GetController().GetEntryCount());
  NavigationEntryImpl* entry1 = NavigationEntryImpl::FromNavigationEntry(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  SiteInstance* site_instance1 = entry1->site_instance();
  EXPECT_EQ(base::ASCIIToUTF16("A"), entry1->GetTitle());

  // Force a process switch by going to a privileged page.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), web_ui_page);
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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));
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
  const GURL kWebUIUrl("chrome://tracing");
  const char kJSCodeForAppendingFrame[] =
      "document.body.appendChild(document.createElement('iframe'));";

  NavigateToURL(shell(), kWebUIUrl);

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
  NavigateToURL(shell(), initial_url);
  RenderFrameHost* orig_rfh = shell()->web_contents()->GetMainFrame();

  // Install the observer and navigate cross-site.
  RenderFrameCreatedObserver observer(shell());
  NavigateToURL(shell(), cross_site_url);

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
  NavigateToURL(shell(), embedded_test_server()->GetURL("/push_state.html"));
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

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/title1.html"));

  WebContentsAddedObserver new_web_contents_observer;
  ASSERT_TRUE(ExecuteScript(shell(),
                            "var a = document.createElement('a');"
                            "a.href='./title2.html';"
                            "a.target = '_blank';"
                            "document.body.appendChild(a);"
                            "a.click();"));
  WebContents* new_web_contents = new_web_contents_observer.GetWebContents();
  WaitForLoadStop(new_web_contents);
  EXPECT_TRUE(new_web_contents_observer.RenderViewCreatedCalled());
}

// Observer class to track resource loads.
class ResourceLoadObserver : public WebContentsObserver {
 public:
  explicit ResourceLoadObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()) {}

  const std::vector<mojom::ResourceLoadInfoPtr>& resource_load_infos() const {
    return resource_load_infos_;
  }

  const std::vector<bool>& resource_is_associated_with_main_frame() const {
    return resource_is_associated_with_main_frame_;
  }

  const std::vector<GURL>& memory_cached_loaded_urls() const {
    return memory_cached_loaded_urls_;
  }

  // Use this method with the SCOPED_TRACE macro, so it shows the caller context
  // if it fails.
  void CheckResourceLoaded(
      const GURL& url,
      const GURL& referrer,
      const std::string& load_method,
      content::ResourceType resource_type,
      const base::FilePath::StringPieceType& served_file_name,
      const std::string& mime_type,
      const std::string& ip_address,
      bool was_cached,
      bool first_network_request,
      const base::TimeTicks& before_request,
      const base::TimeTicks& after_request) {
    bool resource_load_info_found = false;
    for (const auto& resource_load_info : resource_load_infos_) {
      if (resource_load_info->url != url)
        continue;

      resource_load_info_found = true;
      int64_t file_size = -1;
      if (!served_file_name.empty()) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        base::FilePath test_dir;
        ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_dir));
        base::FilePath served_file = test_dir.Append(served_file_name);
        ASSERT_TRUE(GetFileSize(served_file, &file_size));
      }
      EXPECT_EQ(referrer, resource_load_info->referrer);
      EXPECT_EQ(load_method, resource_load_info->method);
      EXPECT_EQ(resource_type, resource_load_info->resource_type);
      if (!first_network_request)
        EXPECT_GT(resource_load_info->request_id, 0);
      EXPECT_EQ(mime_type, resource_load_info->mime_type);
      ASSERT_TRUE(resource_load_info->network_info->ip_port_pair);
      EXPECT_EQ(ip_address,
                resource_load_info->network_info->ip_port_pair->host());
      EXPECT_EQ(was_cached, resource_load_info->was_cached);
      // Simple sanity check of the load timing info.
      auto CheckTime = [before_request, after_request](auto actual) {
        EXPECT_LE(before_request, actual);
        EXPECT_GT(after_request, actual);
      };
      const net::LoadTimingInfo& timing = resource_load_info->load_timing_info;
      CheckTime(timing.request_start);
      CheckTime(timing.receive_headers_end);
      CheckTime(timing.send_start);
      CheckTime(timing.send_end);
      if (!was_cached) {
        CheckTime(timing.connect_timing.dns_start);
        CheckTime(timing.connect_timing.dns_end);
        CheckTime(timing.connect_timing.connect_start);
        CheckTime(timing.connect_timing.connect_end);
      }
      if (file_size != -1) {
        EXPECT_EQ(file_size, resource_load_info->raw_body_bytes);
        EXPECT_LT(file_size, resource_load_info->total_received_bytes);
      }
    }
    EXPECT_TRUE(resource_load_info_found);
  }

  void Reset() {
    resource_load_infos_.clear();
    memory_cached_loaded_urls_.clear();
    resource_is_associated_with_main_frame_.clear();
  }

 private:
  // WebContentsObserver implementation:
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      const mojom::ResourceLoadInfo& resource_load_info) override {
    EXPECT_NE(nullptr, render_frame_host);
    resource_load_infos_.push_back(resource_load_info.Clone());
    resource_is_associated_with_main_frame_.push_back(
        render_frame_host->GetParent() == nullptr);
  }

  void DidLoadResourceFromMemoryCache(const GURL& url,
                                      const std::string& mime_type,
                                      ResourceType resource_type) override {
    memory_cached_loaded_urls_.push_back(url);
  }

  std::vector<GURL> memory_cached_loaded_urls_;
  std::vector<mojom::ResourceLoadInfoPtr> resource_load_infos_;
  std::vector<bool> resource_is_associated_with_main_frame_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadObserver);
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ResourceLoadComplete) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());
  // Load a page with an image and an image.
  GURL page_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  base::TimeTicks before = base::TimeTicks::Now();
  NavigateToURL(shell(), page_url);
  base::TimeTicks after = base::TimeTicks::Now();
  ASSERT_EQ(3U, observer.resource_load_infos().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET", content::RESOURCE_TYPE_MAIN_FRAME,
      FILE_PATH_LITERAL("page_with_iframe.html"), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/true, before, after));
  SCOPE_TRACED(observer.CheckResourceLoaded(
      embedded_test_server()->GetURL("/image.jpg"),
      /*referrer=*/page_url, "GET", content::RESOURCE_TYPE_IMAGE,
      FILE_PATH_LITERAL("image.jpg"), "image/jpeg", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
  SCOPE_TRACED(observer.CheckResourceLoaded(
      embedded_test_server()->GetURL("/title1.html"),
      /*referrer=*/page_url, "GET", content::RESOURCE_TYPE_SUB_FRAME,
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
  NavigateToURL(shell(), page_url);
  base::TimeTicks after = base::TimeTicks::Now();

  GURL resource_url = embedded_test_server()->GetURL("/cachetime");
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET", content::RESOURCE_TYPE_MAIN_FRAME,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false,
      /*first_network_request=*/true, before, after));

  SCOPE_TRACED(observer.CheckResourceLoaded(
      resource_url, /*referrer=*/page_url, "GET", content::RESOURCE_TYPE_SCRIPT,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
  EXPECT_TRUE(
      observer.resource_load_infos()[1]->network_info->network_accessed);
  EXPECT_TRUE(observer.memory_cached_loaded_urls().empty());
  observer.Reset();

  // Loading again should serve the request out of the in-memory cache.
  before = base::TimeTicks::Now();
  NavigateToURL(shell(), page_url);
  after = base::TimeTicks::Now();
  ASSERT_EQ(1U, observer.resource_load_infos().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET", content::RESOURCE_TYPE_MAIN_FRAME,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/false, before, after));
  ASSERT_EQ(1U, observer.memory_cached_loaded_urls().size());
  EXPECT_EQ(resource_url, observer.memory_cached_loaded_urls()[0]);
  observer.Reset();

  // Kill the renderer process so when the navigate again, it will be a fresh
  // renderer with an empty in-memory cache.
  NavigateToURL(shell(), GURL("chrome:crash"));

  // Reload that URL, the subresource should be served from the network cache.
  before = base::TimeTicks::Now();
  NavigateToURL(shell(), page_url);
  after = base::TimeTicks::Now();
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  SCOPE_TRACED(observer.CheckResourceLoaded(
      page_url, /*referrer=*/GURL(), "GET", content::RESOURCE_TYPE_MAIN_FRAME,
      /*served_file_name=*/FILE_PATH_LITERAL(""), "text/html", "127.0.0.1",
      /*was_cached=*/false, /*first_network_request=*/true, before, after));
  SCOPE_TRACED(observer.CheckResourceLoaded(
      resource_url, /*referrer=*/page_url, "GET", content::RESOURCE_TYPE_SCRIPT,
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
  NavigateToURL(shell(),
                GURL(embedded_test_server()->GetURL("/page_with_image.html")));
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_TRUE(
      observer.resource_load_infos()[0]->network_info->network_accessed);
  EXPECT_TRUE(
      observer.resource_load_infos()[1]->network_info->network_accessed);
  observer.Reset();

  NavigateToURL(shell(), GURL("chrome://gpu"));
  ASSERT_LE(1U, observer.resource_load_infos().size());
  for (const mojom::ResourceLoadInfoPtr& resource_load_info :
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
  NavigateToURL(shell(), page_original_url);

  ASSERT_EQ(2U, observer.resource_load_infos().size());
  const mojom::ResourceLoadInfoPtr& page_load_info =
      observer.resource_load_infos()[0];
  EXPECT_EQ(page_destination_url, page_load_info->url);
  EXPECT_EQ(page_original_url, page_load_info->original_url);

  GURL image_destination_url(embedded_test_server()->GetURL("/blank.jpg"));
  GURL image_original_url(
      embedded_test_server()->GetURL("/server-redirect?blank.jpg"));
  const mojom::ResourceLoadInfoPtr& image_load_info =
      observer.resource_load_infos()[1];
  EXPECT_EQ(image_destination_url, image_load_info->url);
  EXPECT_EQ(image_original_url, image_load_info->original_url);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ResourceLoadCompleteNetError) {
  ResourceLoadObserver observer(shell());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL page_url(embedded_test_server()->GetURL("/page_with_image.html"));
  GURL image_url(embedded_test_server()->GetURL("/blank.jpg"));

  // Load the page without errors.
  NavigateToURL(shell(), page_url);
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
  NavigateToURL(shell(), page_url);
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
  NavigateToURL(shell(), cacheable_url);
  ASSERT_EQ(1U, observer.resource_load_infos().size());
  EXPECT_FALSE(
      observer.resource_load_infos()[0]->network_info->always_access_network);
  observer.Reset();

  std::array<std::string, 3> headers = {
      "cache-control: no-cache", "cache-control: no-store", "pragma: no-cache"};
  for (const std::string& header : headers) {
    GURL no_cache_url(embedded_test_server()->GetURL("/set-header?" + header));
    NavigateToURL(shell(), no_cache_url);
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

  NavigateToURL(shell(), start_url);

  ASSERT_EQ(1U, observer.resource_load_infos().size());
  EXPECT_EQ(target_url, observer.resource_load_infos()[0]->url);

  ASSERT_EQ(2U, observer.resource_load_infos()[0]->redirect_info_chain.size());
  EXPECT_EQ(intermediate_url,
            observer.resource_load_infos()[0]->redirect_info_chain[0]->url);
  EXPECT_TRUE(observer.resource_load_infos()[0]
                  ->redirect_info_chain[0]
                  ->network_info->network_accessed);
  EXPECT_FALSE(observer.resource_load_infos()[0]
                   ->redirect_info_chain[0]
                   ->network_info->always_access_network);
  EXPECT_EQ(target_url,
            observer.resource_load_infos()[0]->redirect_info_chain[1]->url);
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
  NavigateToURL(shell(), url);
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_EQ(url, observer.resource_load_infos()[0]->url);
  EXPECT_TRUE(observer.resource_is_associated_with_main_frame()[0]);
  EXPECT_TRUE(observer.resource_is_associated_with_main_frame()[1]);
  observer.Reset();

  // Load that same page inside an iframe.
  GURL data_url("data:text/html,<iframe src='" + url.spec() + "'></iframe>");
  NavigateToURL(shell(), data_url);
  ASSERT_EQ(2U, observer.resource_load_infos().size());
  EXPECT_EQ(url, observer.resource_load_infos()[0]->url);
  EXPECT_FALSE(observer.resource_is_associated_with_main_frame()[0]);
  EXPECT_FALSE(observer.resource_is_associated_with_main_frame()[1]);
}

struct LoadProgressDelegateAndObserver : public WebContentsDelegate,
                                         public WebContentsObserver {
  explicit LoadProgressDelegateAndObserver(Shell* shell)
      : WebContentsObserver(shell->web_contents()),
        did_start_loading(false),
        did_stop_loading(false) {
    web_contents()->SetDelegate(this);
  }

  // WebContentsDelegate:
  void LoadProgressChanged(WebContents* source, double progress) override {
    EXPECT_TRUE(did_start_loading);
    EXPECT_FALSE(did_stop_loading);
    progresses.push_back(progress);
  }

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

  bool did_start_loading;
  std::vector<double> progresses;
  bool did_stop_loading;
};

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, LoadProgress) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

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
  std::unique_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

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
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Simulate a navigation that has not completed.
  const GURL kURL2 = embedded_test_server()->GetURL("/title2.html");
  TestNavigationManager navigation(shell()->web_contents(), kURL2);
  std::unique_ptr<LoadProgressDelegateAndObserver> delegate(
      new LoadProgressDelegateAndObserver(shell()));
  shell()->LoadURL(kURL2);
  EXPECT_TRUE(navigation.WaitForResponse());
  EXPECT_TRUE(delegate->did_start_loading);
  EXPECT_FALSE(delegate->did_stop_loading);

  // Also simulate a DidChangeLoadProgress, but not a DidStopLoading.
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  FrameHostMsg_DidChangeLoadProgress progress_msg(main_frame->GetRoutingID(),
                                                  1.0);
  main_frame->OnMessageReceived(progress_msg);
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
    on_did_first_visually_non_empty_paint_.Run();
  }

  void WaitForDidFirstVisuallyNonEmptyPaint() {
    if (did_fist_visually_non_empty_paint_)
      return;
    base::RunLoop run_loop;
    on_did_first_visually_non_empty_paint_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::Closure on_did_first_visually_non_empty_paint_;
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

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  observer->WaitForDidFirstVisuallyNonEmptyPaint();
  ASSERT_TRUE(observer->did_fist_visually_non_empty_paint_);
}

namespace {

class WebDisplayModeDelegate : public WebContentsDelegate {
 public:
  explicit WebDisplayModeDelegate(blink::WebDisplayMode mode) : mode_(mode) { }
  ~WebDisplayModeDelegate() override { }

  blink::WebDisplayMode GetDisplayMode(
      const WebContents* source) const override { return mode_; }
  void set_mode(blink::WebDisplayMode mode) { mode_ = mode; }
 private:
  blink::WebDisplayMode mode_;

  DISALLOW_COPY_AND_ASSIGN(WebDisplayModeDelegate);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangeDisplayMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebDisplayModeDelegate delegate(blink::kWebDisplayModeMinimalUi);
  shell()->web_contents()->SetDelegate(&delegate);

  NavigateToURL(shell(), GURL("about://blank"));

  ASSERT_TRUE(ExecuteScript(shell(),
                            "document.title = "
                            " window.matchMedia('(display-mode:"
                            " minimal-ui)').matches"));
  EXPECT_EQ(base::ASCIIToUTF16("true"), shell()->web_contents()->GetTitle());

  delegate.set_mode(blink::kWebDisplayModeFullscreen);
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
    on_page_scale_update_.Run();
  }

  base::Closure on_page_scale_update_;
  bool got_page_scale_update_;
};

// When the page scale factor is set in the renderer it should send
// a notification to the browser so that WebContentsObservers are notified.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ChangePageScale) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  MockPageScaleObserver observer(shell());
  ::testing::InSequence expect_call_sequence;

  shell()->web_contents()->SetPageScale(1.5);
  EXPECT_CALL(observer, OnPageScaleFactorChanged(::testing::FloatEq(1.5)));
  observer.WaitForPageScaleUpdate();

  // Navigate to reset the page scale factor.
  shell()->LoadURL(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_CALL(observer, OnPageScaleFactorChanged(::testing::_));
  observer.WaitForPageScaleUpdate();
}

// Test that a direct navigation to a view-source URL works.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, ViewSourceDirectNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  NavigateToURL(shell(), kViewSourceURL);
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
  NavigateToURL(shell(), kUrl);

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.open('" + kViewSourceURL.spec() + "');"));
  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_TRUE(new_shell->web_contents()->GetURL().spec().empty());
  // No navigation should commit.
  EXPECT_FALSE(
      new_shell->web_contents()->GetController().GetLastCommittedEntry());
}

// Test that a content initiated navigation to a view-source URL is blocked.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       ViewSourceRedirect_ShouldBeBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL("/simple_page.html"));
  const GURL kViewSourceURL(kViewSourceScheme + std::string(":") + kUrl.spec());
  NavigateToURL(shell(), kUrl);

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Not allowed to load local resource: view-source:*"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "window.location = '" + kViewSourceURL.spec() + "';"));
  console_delegate->Wait();
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
  const std::string kUrl =
      "view-source:chrome://" + std::string(kChromeUIGpuHost);
  const GURL kGURL(kUrl);
  NavigateToURL(shell(), kGURL);
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
    WaitForLoadStop(new_shell->web_contents());

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
    WaitForLoadStop(new_shell->web_contents());

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
    bool expect_onunload,
    bool expect_onbeforeunload) {
  NavigateToURL(shell, GURL("data:text/html," + html));
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(shell->web_contents()->GetMainFrame());
  EXPECT_EQ(expect_onunload,
            rfh->GetSuddenTerminationDisablerState(blink::kUnloadHandler));
  EXPECT_EQ(expect_onbeforeunload, rfh->GetSuddenTerminationDisablerState(
                                       blink::kBeforeUnloadHandler));
}
}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerNone) {
  const std::string NO_HANDLERS_HTML = "<html><body>foo</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(shell(), NO_HANDLERS_HTML,
                                                  false, false);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnUnload) {
  const std::string UNLOAD_HTML =
      "<html><body><script>window.onunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(shell(), UNLOAD_HTML, true,
                                                  false);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnBeforeUnload) {
  const std::string BEFORE_UNLOAD_HTML =
      "<html><body><script>window.onbeforeunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(shell(), BEFORE_UNLOAD_HTML,
                                                  false, true);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       SuddenTerminationDisablerOnUnloadAndBeforeUnload) {
  const std::string UNLOAD_AND_BEFORE_UNLOAD_HTML =
      "<html><body><script>window.onunload=function(e) {};"
      "window.onbeforeunload=function(e) {}</script>"
      "</body></html>";
  NavigateToDataURLAndCheckForTerminationDisabler(
      shell(), UNLOAD_AND_BEFORE_UNLOAD_HTML, true, true);
}

namespace {

class TestWCDelegateForDialogsAndFullscreen : public JavaScriptDialogManager,
                                              public WebContentsDelegate {
 public:
  TestWCDelegateForDialogsAndFullscreen() = default;
  ~TestWCDelegateForDialogsAndFullscreen() override = default;

  void WillWaitForDialog() { waiting_for_ = kDialog; }
  void WillWaitForNewContents() { waiting_for_ = kNewContents; }
  void WillWaitForFullscreenExit() { waiting_for_ = kFullscreenExit; }

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  std::string last_message() { return last_message_; }

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  void EnterFullscreenModeForTab(
      WebContents* web_contents,
      const GURL& origin,
      const blink::WebFullscreenOptions& options) override {
    is_fullscreen_ = true;
  }

  void ExitFullscreenModeForTab(WebContents*) override {
    is_fullscreen_ = false;

    if (waiting_for_ == kFullscreenExit) {
      waiting_for_ = kNothing;
      run_loop_->Quit();
    }
  }

  bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const override {
    return is_fullscreen_;
  }

  void AddNewContents(WebContents* source,
                      std::unique_ptr<WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override {
    popup_ = std::move(new_contents);

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
  enum {
    kNothing,
    kDialog,
    kNewContents,
    kFullscreenExit
  } waiting_for_ = kNothing;

  std::string last_message_;

  bool is_fullscreen_ = false;

  std::unique_ptr<WebContents> popup_;

  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();

  DISALLOW_COPY_AND_ASSIGN(TestWCDelegateForDialogsAndFullscreen);
};

class MockFileSelectListener : public FileSelectListener {
 public:
  MockFileSelectListener() {}
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    blink::mojom::FileChooserParams::Mode mode) override {}
  void FileSelectionCanceled() override {}
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       JavaScriptDialogsInMainAndSubframes) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/title1.html"));
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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("c.com", "/title3.html"));
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

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       JavaScriptDialogsNormalizeText) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // A dialog with mixed linebreaks.
  std::string alert = "alert('1\\r2\\r\\n3\\n4')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, alert));
  test_delegate.Wait();
  EXPECT_EQ("1\n2\n3\n4", test_delegate.last_message());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
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
  create_params.initial_size =
      base_web_contents->GetContainerBounds().size();
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
    create_params.initial_size = base_web_contents->GetContainerBounds().size();
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
  create_params.initial_size =
      base_web_contents->GetContainerBounds().size();
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
      image_url, false, 1024, false,
      base::BindOnce(&DownloadImageObserver::OnFinishDownloadImage,
                     base::Unretained(&download_image_observer)));

  // Wait for response.
  loop_runner->Run();
}

void ExpectNoValidImageCallback(const base::Closure& quit_closure,
                                int id,
                                int status_code,
                                const GURL& image_url,
                                const std::vector<SkBitmap>& bitmap,
                                const std::vector<gfx::Size>& sizes) {
  EXPECT_EQ(200, status_code);
  EXPECT_TRUE(bitmap.empty());
  EXPECT_TRUE(sizes.empty());
  quit_closure.Run();
}

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_HttpImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kImageUrl = embedded_test_server()->GetURL("/single_face.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 200, 1);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_Deny_FileImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));

  const GURL kImageUrl = GetTestUrl("", "single_face.jpg");
  DownloadImageTestInternal(shell(), kImageUrl, 0, 0);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DownloadImage_Allow_FileImage) {
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
      kImageUrl, false, 2, false,
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

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       RenderWidgetDeletedWhileMouseLockPending) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<MouseLockDelegate> delegate(new MouseLockDelegate());
  shell()->web_contents()->SetDelegate(delegate.get());
  ASSERT_TRUE(shell()->web_contents()->GetDelegate() == delegate.get());

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/title1.html"));

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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("b.com", "/title1.html"));

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

  NavigateToURL(shell(), kUrl);
  std::string header_value;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_NE(kUserAgentOverride, header_value);

  shell()->web_contents()->SetUserAgentOverride("foo", false);
  NavigateToURL(shell(), kUrl);
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

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DialogsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // alert
  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  std::string script = "alert('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  // confirm
  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  script = "confirm('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  // prompt
  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  script = "prompt('hi')";
  test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  // beforeunload
  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
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
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       DialogsFromJavaScriptEndFullscreenEvenInInnerWC) {
  WebContentsImpl* top_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen top_test_delegate;
  top_contents->SetDelegate(&top_test_delegate);

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
  TestWCDelegateForDialogsAndFullscreen inner_test_delegate;
  inner_contents->SetDelegate(&inner_test_delegate);

  // A dialog from the inner WebContents should make the outer contents lose
  // fullscreen.
  top_contents->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(top_contents->IsFullscreenForCurrentTab());
  script = "alert('hi')";
  inner_test_delegate.WillWaitForDialog();
  EXPECT_TRUE(content::ExecuteScript(inner_contents, script));
  inner_test_delegate.Wait();
  EXPECT_FALSE(top_contents->IsFullscreenForCurrentTab());

  inner_contents->SetDelegate(nullptr);
  inner_contents->SetJavaScriptDialogManagerForTesting(nullptr);

  top_contents->SetDelegate(nullptr);
  top_contents->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FileChooserEndsFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  wc->RunFileChooser(wc->GetMainFrame(),
                     std::make_unique<MockFileSelectListener>(),
                     blink::mojom::FileChooserParams());
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       PopupsFromJavaScriptEndFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

  GURL url("about:blank");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // popup
  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());
  std::string script = "window.open('', '', 'width=200,height=100')";
  test_delegate.WillWaitForNewContents();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       FocusFromJavaScriptEndsFullscreen) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  wc->SetDelegate(&test_delegate);

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
  wc->EnterFullscreenMode(url, blink::WebFullscreenOptions());
  EXPECT_TRUE(wc->IsFullscreenForCurrentTab());

  // ... and ensure that a call to window.focus() from it causes loss of
  // ... fullscreen.
  script = "window.FocusFromJavaScriptEndsFullscreen.focus()";
  test_delegate.WillWaitForFullscreenExit();
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  test_delegate.Wait();
  EXPECT_FALSE(wc->IsFullscreenForCurrentTab());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
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
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, UpdateLoadState) {
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
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    waiter.Wait(net::LOAD_STATE_IDLE, base::string16());
  else
    waiter.Wait(net::LOAD_STATE_WAITING_FOR_DELEGATE, paused_host);

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

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Now the only request in flight should be the delayed frame.
    waiter.Wait(net::LOAD_STATE_WAITING_FOR_DELEGATE, paused_host);
    frame_pauser.ResumeNavigation();
    waiter.Wait(net::LOAD_STATE_IDLE, base::string16());
  }
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
  auto* b_subframe_rvh = ChildFrameAt(main_frame, 0)->GetRenderViewHost();
  auto* c_subframe_rvh = ChildFrameAt(main_frame, 1)->GetRenderViewHost();

  NotifyPreferencesChangedTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  web_contents->NotifyPreferencesChanged();

  // We should have updated the preferences for all three RenderViewHosts.
  EXPECT_EQ(std::unordered_set<RenderViewHost*>(
                {main_frame_rvh, b_subframe_rvh, c_subframe_rvh}),
            new_client.override_webkit_prefs_rvh_set());

  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, PausePageScheduledTasks) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/pause_schedule_task.html");
  NavigateToURL(shell(), test_url);
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

  // Suspend blink schedule tasks.
  shell()->web_contents()->PausePageScheduledTasks(true);

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

  // Resume the paused blink schedule tasks.
  shell()->web_contents()->PausePageScheduledTasks(false);

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
        shell(),
        "window.domAutomationController.send(clickDeadFileNewWindowLink());",
        &success));
    new_shell = new_shell_observer.GetShell();
    new_contents = new_shell->web_contents();
    // Delaying popup holds the initial load.
    EXPECT_FALSE(WaitForLoadStop(new_contents));
  }

  EXPECT_FALSE(new_contents->GetDelegate());
  new_contents->SetDelegate(new_shell);
  new_contents->ResumeLoadingCreatedWebContents();
  // Dead file link may or may not load depending on OS. The result is not
  // relevant for this test, so not checking the the result.
  WaitForLoadStop(new_contents);
  EXPECT_TRUE(new_contents->GetLastCommittedURL().SchemeIs("file"));
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
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  web_contents->SetDelegate(&test_delegate);

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

// Regression test for https://crbug.com/855018.
// RenderFrameHostImpls exit fullscreen as soon as they are swapped out.
IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest, FullscreenAfterFrameSwap) {
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
  //    the swapout ack and stayed in pending deletion for a while. Even if the
  //    frame is still present, it must be removed from the list of frame in
  //    fullscreen immediately.
  auto filter = base::MakeRefCounted<SwapoutACKMessageFilter>();
  main_frame->GetProcess()->AddFilter(filter.get());
  main_frame->DisableSwapOutTimerForTesting();
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(0u, web_contents->fullscreen_frames_.size());
}

IN_PROC_BROWSER_TEST_F(WebContentsImplBrowserTest,
                       NotifyFullscreenAcquired_Navigate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  test_delegate.WillWaitForFullscreenExit();
  web_contents->SetDelegate(&test_delegate);

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
  TestWCDelegateForDialogsAndFullscreen test_delegate;
  web_contents->SetDelegate(&test_delegate);

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
  NavigateToURL(shell(), main_url);

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

}  // namespace content
