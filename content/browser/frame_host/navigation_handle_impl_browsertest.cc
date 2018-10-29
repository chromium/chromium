// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace {

// Text to place in an HTML body. Should not contain any markup.
const char kBodyTextContent[] = "some plain text content";

}  // namespace

namespace content {

namespace {

// A test NavigationThrottle that will return pre-determined checks and run
// callbacks when the various NavigationThrottle methods are called. It is
// not instantiated directly but through a TestNavigationThrottleInstaller.
class TestNavigationThrottle : public NavigationThrottle {
 public:
  TestNavigationThrottle(
      NavigationHandle* handle,
      NavigationThrottle::ThrottleCheckResult will_start_result,
      NavigationThrottle::ThrottleCheckResult will_redirect_result,
      NavigationThrottle::ThrottleCheckResult will_fail_result,
      NavigationThrottle::ThrottleCheckResult will_process_result,
      base::Closure did_call_will_start,
      base::Closure did_call_will_redirect,
      base::Closure did_call_will_fail,
      base::Closure did_call_will_process)
      : NavigationThrottle(handle),
        will_start_result_(will_start_result),
        will_redirect_result_(will_redirect_result),
        will_fail_result_(will_fail_result),
        will_process_result_(will_process_result),
        did_call_will_start_(did_call_will_start),
        did_call_will_redirect_(did_call_will_redirect),
        did_call_will_fail_(did_call_will_fail),
        did_call_will_process_(did_call_will_process) {}
  ~TestNavigationThrottle() override {}

  const char* GetNameForLogging() override { return "TestNavigationThrottle"; }

  blink::mojom::RequestContextType request_context_type() {
    return request_context_type_;
  }

  // Expose Resume and Cancel to the installer.
  void ResumeNavigation() { Resume(); }

  void CancelNavigation(NavigationThrottle::ThrottleCheckResult result) {
    CancelDeferredNavigation(result);
  }

 private:
  // NavigationThrottle implementation.
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_NE(blink::mojom::RequestContextType::UNSPECIFIED,
             navigation_handle_impl->request_context_type());
    request_context_type_ = navigation_handle_impl->request_context_type();

    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             did_call_will_start_);
    return will_start_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_EQ(request_context_type_,
             navigation_handle_impl->request_context_type());

    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             did_call_will_redirect_);
    return will_redirect_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_EQ(request_context_type_,
             navigation_handle_impl->request_context_type());

    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             did_call_will_fail_);
    return will_fail_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    NavigationHandleImpl* navigation_handle_impl =
        static_cast<NavigationHandleImpl*>(navigation_handle());
    CHECK_EQ(request_context_type_,
             navigation_handle_impl->request_context_type());

    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             did_call_will_process_);
    return will_process_result_;
  }

  NavigationThrottle::ThrottleCheckResult will_start_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_result_;
  NavigationThrottle::ThrottleCheckResult will_fail_result_;
  NavigationThrottle::ThrottleCheckResult will_process_result_;
  base::Closure did_call_will_start_;
  base::Closure did_call_will_redirect_;
  base::Closure did_call_will_fail_;
  base::Closure did_call_will_process_;
  blink::mojom::RequestContextType request_context_type_ =
      blink::mojom::RequestContextType::UNSPECIFIED;
};

// Installs a TestNavigationThrottle either on all following requests or on
// requests with an expected starting URL, and allows waiting for various
// NavigationThrottle related events. Waiting works only for the immediately
// next navigation. New instances are needed to wait for further navigations.
class TestNavigationThrottleInstaller : public WebContentsObserver {
 public:
  enum Method {
    WILL_START_REQUEST,
    WILL_REDIRECT_REQUEST,
    WILL_FAIL_REQUEST,
    WILL_PROCESS_RESPONSE,
  };

  TestNavigationThrottleInstaller(
      WebContents* web_contents,
      NavigationThrottle::ThrottleCheckResult will_start_result,
      NavigationThrottle::ThrottleCheckResult will_redirect_result,
      NavigationThrottle::ThrottleCheckResult will_fail_result,
      NavigationThrottle::ThrottleCheckResult will_process_result,
      const GURL& expected_start_url = GURL())
      : WebContentsObserver(web_contents),
        will_start_result_(will_start_result),
        will_redirect_result_(will_redirect_result),
        will_fail_result_(will_fail_result),
        will_process_result_(will_process_result),
        expected_start_url_(expected_start_url),
        weak_factory_(this) {}
  ~TestNavigationThrottleInstaller() override {}

  // Installs a TestNavigationThrottle whose |method| method will return
  // |result|. All other methods will return NavigationThrottle::PROCEED.
  static std::unique_ptr<TestNavigationThrottleInstaller> CreateForMethod(
      WebContents* web_contents,
      Method method,
      NavigationThrottle::ThrottleCheckResult result) {
    NavigationThrottle::ThrottleCheckResult will_start_result =
        NavigationThrottle::PROCEED;
    auto will_redirect_result = will_start_result;
    auto will_fail_result = will_start_result;
    auto will_process_result = will_start_result;

    switch (method) {
      case WILL_START_REQUEST:
        will_start_result = result;
        break;
      case WILL_REDIRECT_REQUEST:
        will_redirect_result = result;
        break;
      case WILL_FAIL_REQUEST:
        will_fail_result = result;
        break;
      case WILL_PROCESS_RESPONSE:
        will_process_result = result;
        break;
    }

    return std::make_unique<TestNavigationThrottleInstaller>(
        web_contents, will_start_result, will_redirect_result, will_fail_result,
        will_process_result);
  }

  TestNavigationThrottle* navigation_throttle() { return navigation_throttle_; }

  void WaitForThrottleWillStart() {
    if (will_start_called_)
      return;
    will_start_loop_runner_ = new MessageLoopRunner();
    will_start_loop_runner_->Run();
    will_start_loop_runner_ = nullptr;
  }

  void WaitForThrottleWillRedirect() {
    if (will_redirect_called_)
      return;
    will_redirect_loop_runner_ = new MessageLoopRunner();
    will_redirect_loop_runner_->Run();
    will_redirect_loop_runner_ = nullptr;
  }

  void WaitForThrottleWillFail() {
    if (will_fail_called_)
      return;
    will_fail_loop_runner_ = new MessageLoopRunner();
    will_fail_loop_runner_->Run();
    will_fail_loop_runner_ = nullptr;
  }

  void WaitForThrottleWillProcess() {
    if (will_process_called_)
      return;
    will_process_loop_runner_ = new MessageLoopRunner();
    will_process_loop_runner_->Run();
    will_process_loop_runner_ = nullptr;
  }

  void Continue(NavigationThrottle::ThrottleCheckResult result) {
    ASSERT_NE(NavigationThrottle::DEFER, result.action());
    if (result.action() == NavigationThrottle::PROCEED)
      navigation_throttle()->ResumeNavigation();
    else
      navigation_throttle()->CancelNavigation(result);
  }

  int will_start_called() { return will_start_called_; }
  int will_redirect_called() { return will_redirect_called_; }
  int will_fail_called() { return will_fail_called_; }
  int will_process_called() { return will_process_called_; }

  int install_count() { return install_count_; }

 protected:
  virtual void DidCallWillStartRequest() {
    will_start_called_++;
    if (will_start_loop_runner_)
      will_start_loop_runner_->Quit();
  }

  virtual void DidCallWillRedirectRequest() {
    will_redirect_called_++;
    if (will_redirect_loop_runner_)
      will_redirect_loop_runner_->Quit();
  }

  virtual void DidCallWillFailRequest() {
    will_fail_called_++;
    if (will_fail_loop_runner_)
      will_fail_loop_runner_->Quit();
  }

  virtual void DidCallWillProcessResponse() {
    will_process_called_++;
    if (will_process_loop_runner_)
      will_process_loop_runner_->Quit();
  }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    if (!expected_start_url_.is_empty() &&
        handle->GetURL() != expected_start_url_)
      return;

    std::unique_ptr<NavigationThrottle> throttle(new TestNavigationThrottle(
        handle, will_start_result_, will_redirect_result_, will_fail_result_,
        will_process_result_,
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillStartRequest,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillRedirectRequest,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillFailRequest,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&TestNavigationThrottleInstaller::DidCallWillProcessResponse,
                   weak_factory_.GetWeakPtr())));
    navigation_throttle_ = static_cast<TestNavigationThrottle*>(throttle.get());
    handle->RegisterThrottleForTesting(std::move(throttle));
    ++install_count_;
  }

  void DidFinishNavigation(NavigationHandle* handle) override {
    if (!navigation_throttle_)
      return;

    if (handle == navigation_throttle_->navigation_handle())
      navigation_throttle_ = nullptr;
  }

  NavigationThrottle::ThrottleCheckResult will_start_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_result_;
  NavigationThrottle::ThrottleCheckResult will_fail_result_;
  NavigationThrottle::ThrottleCheckResult will_process_result_;
  int will_start_called_ = 0;
  int will_redirect_called_ = 0;
  int will_fail_called_ = 0;
  int will_process_called_ = 0;
  TestNavigationThrottle* navigation_throttle_ = nullptr;
  int install_count_ = 0;
  scoped_refptr<MessageLoopRunner> will_start_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_redirect_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_fail_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_process_loop_runner_;
  GURL expected_start_url_;

  // The throttle installer can be deleted before all tasks posted by its
  // throttles are run, so it must be referenced via weak pointers.
  base::WeakPtrFactory<TestNavigationThrottleInstaller> weak_factory_;
};

// Same as above, but installs NavigationThrottles that do not directly return
// the pre-programmed check results, but first DEFER the navigation at each
// stage and then resume/cancel asynchronously.
class TestDeferringNavigationThrottleInstaller
    : public TestNavigationThrottleInstaller {
 public:
  TestDeferringNavigationThrottleInstaller(
      WebContents* web_contents,
      NavigationThrottle::ThrottleCheckResult will_start_result,
      NavigationThrottle::ThrottleCheckResult will_redirect_result,
      NavigationThrottle::ThrottleCheckResult will_fail_result,
      NavigationThrottle::ThrottleCheckResult will_process_result,
      GURL expected_start_url = GURL())
      : TestNavigationThrottleInstaller(web_contents,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        expected_start_url),
        will_start_deferred_result_(will_start_result),
        will_redirect_deferred_result_(will_redirect_result),
        will_fail_deferred_result_(will_fail_result),
        will_process_deferred_result_(will_process_result) {}

 protected:
  void DidCallWillStartRequest() override {
    TestNavigationThrottleInstaller::DidCallWillStartRequest();
    Continue(will_start_deferred_result_);
  }

  void DidCallWillRedirectRequest() override {
    TestNavigationThrottleInstaller::DidCallWillStartRequest();
    Continue(will_redirect_deferred_result_);
  }

  void DidCallWillFailRequest() override {
    TestNavigationThrottleInstaller::DidCallWillStartRequest();
    Continue(will_fail_deferred_result_);
  }

  void DidCallWillProcessResponse() override {
    TestNavigationThrottleInstaller::DidCallWillStartRequest();
    Continue(will_process_deferred_result_);
  }

 private:
  NavigationThrottle::ThrottleCheckResult will_start_deferred_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_deferred_result_;
  NavigationThrottle::ThrottleCheckResult will_fail_deferred_result_;
  NavigationThrottle::ThrottleCheckResult will_process_deferred_result_;
};

// Records all navigation start URLs from the WebContents.
class NavigationStartUrlRecorder : public WebContentsObserver {
 public:
  NavigationStartUrlRecorder(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    urls_.push_back(navigation_handle->GetURL());
  }

  const std::vector<GURL>& urls() const { return urls_; }

 private:
  std::vector<GURL> urls_;
};

void ExpectChildFrameSetAsCollapsedInFTN(Shell* shell, bool expect_collapsed) {
  // Check if the frame should be collapsed in theory as per FTN.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);
  EXPECT_EQ(expect_collapsed, child->is_collapsed());
}

void ExpectChildFrameCollapsedInLayout(Shell* shell,
                                       const std::string& frame_id,
                                       bool expect_collapsed) {
  // Check if the frame is collapsed in practice.
  const char kScript[] =
      "window.domAutomationController.send("
      "  document.getElementById(\"%s\").clientWidth"
      ");";
  int client_width = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell, base::StringPrintf(kScript, frame_id.c_str()), &client_width));
  EXPECT_EQ(expect_collapsed, !client_width) << client_width;
}

void ExpectChildFrameCollapsed(Shell* shell,
                               const std::string& frame_id,
                               bool expect_collapsed) {
  ExpectChildFrameSetAsCollapsedInFTN(shell, expect_collapsed);
  ExpectChildFrameCollapsedInLayout(shell, frame_id, expect_collapsed);
}

}  // namespace

class NavigationHandleImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Installs a NavigationThrottle whose |method| method will return a
  // ThrottleCheckResult with |action|, net::ERR_BLOCKED_BY_CLIENT and
  // a custom error page. Navigates to |url|, checks that the navigation
  // committed and that the custom error page is being shown.
  void InstallThrottleAndTestNavigationCommittedWithErrorPage(
      const GURL& url,
      NavigationThrottle::ThrottleAction action,
      TestNavigationThrottleInstaller::Method method) {
    auto installer = TestNavigationThrottleInstaller::CreateForMethod(
        shell()->web_contents(), method,
        NavigationThrottle::ThrottleCheckResult(
            action, net::ERR_BLOCKED_BY_CLIENT,
            base::StringPrintf("<html><body>%s</body><html>",
                               kBodyTextContent)));

    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_FALSE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());

    std::string result;
    const std::string javascript =
        "domAutomationController.send(document.body.textContent)";
    content::RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
    ASSERT_TRUE(
        content::ExecuteScriptAndExtractString(rfh, javascript, &result));
    EXPECT_EQ(kBodyTextContent, result);
  }
};

class NavigationHandleImplDownloadBrowserTest
    : public NavigationHandleImplBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    NavigationHandleImplBrowserTest::SetUpOnMainThread();

    // Set up a test download directory, in order to prevent prompting for
    // handling downloads.
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

 private:
  base::ScopedTempDir downloads_directory_;
};

// Ensure that PageTransition is properly set on the NavigationHandle.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyPageTransition) {
  {
    // Test browser initiated navigation, which should have a PageTransition as
    // if it came from the omnibox.
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);
    ui::PageTransition expected_transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(url, observer.last_committed_url());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        observer.page_transition(), expected_transition));
  }

  {
    // Test navigating to a page with subframe. The subframe will have
    // PageTransition of type AUTO_SUBFRAME.
    GURL url(
        embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("/cross-site/baz.com/title1.html"));
    ui::PageTransition expected_transition =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
              observer.last_committed_url());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        observer.page_transition(), expected_transition));
    EXPECT_FALSE(observer.is_main_frame());
  }
}

// Ensure that the following methods on NavigationHandle behave correctly:
// * IsInMainFrame
// * IsParentMainFrame
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyFrameTree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c())"));
  GURL c_url(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c()"));

  NavigationHandleObserver main_observer(shell()->web_contents(), main_url);
  NavigationHandleObserver b_observer(shell()->web_contents(), b_url);
  NavigationHandleObserver c_observer(shell()->web_contents(), c_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Verify the main frame.
  EXPECT_TRUE(main_observer.has_committed());
  EXPECT_FALSE(main_observer.is_error());
  EXPECT_EQ(main_url, main_observer.last_committed_url());
  EXPECT_TRUE(main_observer.is_main_frame());
  EXPECT_EQ(root->frame_tree_node_id(), main_observer.frame_tree_node_id());

  // Verify the b.com frame.
  EXPECT_TRUE(b_observer.has_committed());
  EXPECT_FALSE(b_observer.is_error());
  EXPECT_EQ(b_url, b_observer.last_committed_url());
  EXPECT_FALSE(b_observer.is_main_frame());
  EXPECT_TRUE(b_observer.is_parent_main_frame());
  EXPECT_EQ(root->child_at(0)->frame_tree_node_id(),
            b_observer.frame_tree_node_id());

  // Verify the c.com frame.
  EXPECT_TRUE(c_observer.has_committed());
  EXPECT_FALSE(c_observer.is_error());
  EXPECT_EQ(c_url, c_observer.last_committed_url());
  EXPECT_FALSE(c_observer.is_main_frame());
  EXPECT_FALSE(c_observer.is_parent_main_frame());
  EXPECT_EQ(root->child_at(0)->child_at(0)->frame_tree_node_id(),
            c_observer.frame_tree_node_id());
}

// Ensure that the WasRedirected() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifyRedirect) {
  {
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.was_redirected());
  }

  {
    GURL url(embedded_test_server()->GetURL("/cross-site/baz.com/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    NavigateToURL(shell(), url);

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.was_redirected());
  }
}

// Ensure that a certificate error results in a committed navigation with
// the appropriate error code on the handle.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyCertErrorFailure) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());
  GURL url(https_server.GetURL("/title1.html"));

  NavigationHandleObserver observer(shell()->web_contents(), url);

  EXPECT_FALSE(NavigateToURL(shell(), url));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, observer.net_error_code());
}

// Ensure that the IsRendererInitiated() method on NavigationHandle behaves
// correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyRendererInitiated) {
  {
    // Test browser initiated navigation.
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_renderer_initiated());
  }

  {
    // Test a main frame + subframes navigation.
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
    GURL b_url(embedded_test_server()->GetURL(
        "b.com", "/cross_site_iframe_factory.html?b(c())"));
    GURL c_url(embedded_test_server()->GetURL(
        "c.com", "/cross_site_iframe_factory.html?c()"));

    NavigationHandleObserver main_observer(shell()->web_contents(), main_url);
    NavigationHandleObserver b_observer(shell()->web_contents(), b_url);
    NavigationHandleObserver c_observer(shell()->web_contents(), c_url);

    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    // Verify that the main frame navigation is not renderer initiated.
    EXPECT_TRUE(main_observer.has_committed());
    EXPECT_FALSE(main_observer.is_error());
    EXPECT_FALSE(main_observer.is_renderer_initiated());

    // Verify that the subframe navigations are renderer initiated.
    EXPECT_TRUE(b_observer.has_committed());
    EXPECT_FALSE(b_observer.is_error());
    EXPECT_TRUE(b_observer.is_renderer_initiated());
    EXPECT_TRUE(c_observer.has_committed());
    EXPECT_FALSE(c_observer.is_error());
    EXPECT_TRUE(c_observer.is_renderer_initiated());
  }

  {
    // Test a pushState navigation.
    GURL url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(a())"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();

    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar"));
    EXPECT_TRUE(ExecuteScript(root->child_at(0),
                              "window.history.pushState({}, '', 'bar');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_renderer_initiated());
  }
}

// Ensure that methods on NavigationHandle behave correctly with an iframe that
// navigates to its srcdoc attribute.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifySrcdoc) {
  GURL url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_srcdoc_frame.html"));
  NavigationHandleObserver observer(shell()->web_contents(),
                                    GURL(kAboutSrcDocURL));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_error());
  EXPECT_EQ(GURL(kAboutSrcDocURL), observer.last_committed_url());
}

// Ensure that the IsSameDocument() method on NavigationHandle behaves
// correctly.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, VerifySameDocument) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a())"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/foo"));
    EXPECT_TRUE(ExecuteScript(root->child_at(0),
                              "window.history.pushState({}, '', 'foo');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_document());
  }
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar"));
    EXPECT_TRUE(ExecuteScript(root->child_at(0),
                              "window.history.replaceState({}, '', 'bar');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_document());
  }
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar#frag"));
    EXPECT_TRUE(
        ExecuteScript(root->child_at(0), "window.location.replace('#frag');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_document());
  }

  GURL about_blank_url(url::kAboutBlankURL);
  {
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    EXPECT_TRUE(ExecuteScript(
        root, "document.body.appendChild(document.createElement('iframe'));"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_same_document());
    EXPECT_EQ(about_blank_url, observer.last_committed_url());
  }
  {
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    NavigateFrameToURL(root->child_at(0), about_blank_url);

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_same_document());
    EXPECT_EQ(about_blank_url, observer.last_committed_url());
  }
}

// Ensure that a NavigationThrottle can cancel the navigation at navigation
// start.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ThrottleCancelStart) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());

  // The navigation should have been canceled before being redirected.
  EXPECT_FALSE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can cancel the navigation when a navigation
// is redirected.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ThrottleCancelRedirect) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // A navigation with a redirect should be canceled.
  {
    GURL redirect_url(
        embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
    NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::CANCEL, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);

    EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

    EXPECT_FALSE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_TRUE(observer.was_redirected());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
  }

  // A navigation without redirects should be successful.
  {
    GURL no_redirect_url(embedded_test_server()->GetURL("/title2.html"));
    NavigationHandleObserver observer(shell()->web_contents(), no_redirect_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::CANCEL, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);

    EXPECT_TRUE(NavigateToURL(shell(), no_redirect_url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), no_redirect_url);
  }
}

// Ensure that a NavigationThrottle can respond CANCEL when a navigation fails.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ThrottleCancelFailure) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());
  GURL url(https_server.GetURL("/title1.html"));

  // A navigation with a cert error failure should be canceled. CANCEL has a
  // default net error of ERR_ABORTED, which should prevent the navigation from
  // committing.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::CANCEL,
        NavigationThrottle::PROCEED);

    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_FALSE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());

    EXPECT_FALSE(NavigateToURL(shell(), url));

    EXPECT_FALSE(installer.will_fail_called());
    EXPECT_FALSE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_EQ(net::ERR_ABORTED, observer.net_error_code());
  }

  // A navigation with a cert error failure should be canceled.
  // A custom net error should result in a committed error page.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED,
        NavigationThrottle::ThrottleCheckResult(NavigationThrottle::CANCEL,
                                                net::ERR_CERT_DATE_INVALID),
        NavigationThrottle::PROCEED);

    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_FALSE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());

    EXPECT_FALSE(NavigateToURL(shell(), url));

    EXPECT_TRUE(installer.will_fail_called());
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_EQ(net::ERR_CERT_DATE_INVALID, observer.net_error_code());
  }

  // A navigation without a cert error should be successful, without calling
  // WillFailRequest() on the throttle. (We set the failure method response to
  // CANCEL, but that shouldn't matter if the test passes.)
  {
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::CANCEL,
        NavigationThrottle::PROCEED);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_FALSE(installer.will_fail_called());
    EXPECT_FALSE(observer.is_error());
  }
}

// Ensure that a NavigationThrottle can cancel the navigation when the response
// is received.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ThrottleCancelResponse) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::CANCEL);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  // The navigation should have been redirected first, and then canceled when
  // the response arrived.
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can defer and resume the navigation at
// navigation start, navigation redirect and response received.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ThrottleDefer) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER,
      NavigationThrottle::DEFER);

  shell()->LoadURL(redirect_url);

  // Wait for WillStartRequest.
  installer.WaitForThrottleWillStart();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for WillRedirectRequest.
  installer.WaitForThrottleWillRedirect();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(1, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for WillProcessResponse.
  installer.WaitForThrottleWillProcess();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(1, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(1, installer.will_process_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for the end of the navigation.
  navigation_observer.Wait();

  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_FALSE(observer.is_error());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(embedded_test_server()->GetURL("bar.com", "/title2.html")));
}

// Ensure that a NavigationThrottle can defer and resume the navigation at
// navigation start and navigation failure.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ThrottleDeferFailure) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());
  GURL failure_url(https_server.GetURL("/title1.html"));

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  NavigationHandleObserver observer(shell()->web_contents(), failure_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER,
      NavigationThrottle::DEFER);

  shell()->LoadURL(failure_url);

  // Wait for WillStartRequest.
  installer.WaitForThrottleWillStart();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for WillFailRequest.
  installer.WaitForThrottleWillFail();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(1, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for the end of the navigation.
  navigation_observer.Wait();

  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, observer.net_error_code());
}

// Ensure that a NavigationThrottle can block the navigation and collapse the
// frame owner both on request start as well as after a redirect. Plus, ensure
// that the frame is restored on the subsequent non-error-page navigation.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ThrottleBlockAndCollapse) {
  const char kChildFrameId[] = "child0";
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  GURL blocked_subframe_url(embedded_test_server()->GetURL(
      "a.com", "/cross-site/baz.com/title1.html"));
  GURL allowed_subframe_url(embedded_test_server()->GetURL(
      "a.com", "/cross-site/baz.com/title2.html"));
  GURL allowed_subframe_final_url(
      embedded_test_server()->GetURL("baz.com", "/title2.html"));

  // Exercise both synchronous and deferred throttle check results, and both on
  // WillStartRequest and on WillRedirectRequest.
  const struct {
    NavigationThrottle::ThrottleAction will_start_result;
    NavigationThrottle::ThrottleAction will_redirect_result;
    bool deferred_block;
  } kTestCases[] = {
      {NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
       NavigationThrottle::PROCEED, false},
      {NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
       NavigationThrottle::PROCEED, true},
      {NavigationThrottle::PROCEED,
       NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE, false},
      {NavigationThrottle::PROCEED,
       NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << test_case.will_start_result << ", "
                                      << test_case.will_redirect_result << ", "
                                      << test_case.deferred_block);

    std::unique_ptr<TestNavigationThrottleInstaller>
        subframe_throttle_installer;
    if (test_case.deferred_block) {
      subframe_throttle_installer.reset(
          new TestDeferringNavigationThrottleInstaller(
              shell()->web_contents(), test_case.will_start_result,
              test_case.will_redirect_result, NavigationThrottle::PROCEED,
              NavigationThrottle::PROCEED, blocked_subframe_url));
    } else {
      subframe_throttle_installer.reset(new TestNavigationThrottleInstaller(
          shell()->web_contents(), test_case.will_start_result,
          test_case.will_redirect_result, NavigationThrottle::PROCEED,
          NavigationThrottle::PROCEED, blocked_subframe_url));
    }

    {
      SCOPED_TRACE("Initial navigation blocked on main frame load.");
      NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                                 blocked_subframe_url);

      ASSERT_TRUE(NavigateToURL(shell(), main_url));
      EXPECT_TRUE(subframe_observer.is_error());
      EXPECT_TRUE(subframe_observer.has_committed());
      EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, subframe_observer.net_error_code());
      ExpectChildFrameCollapsed(shell(), kChildFrameId,
                                true /* expect_collapsed */);
    }

    {
      SCOPED_TRACE("Subsequent subframe navigation is allowed.");
      NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                                 allowed_subframe_url);

      ASSERT_TRUE(NavigateIframeToURL(shell()->web_contents(), kChildFrameId,
                                      allowed_subframe_url));
      EXPECT_TRUE(subframe_observer.has_committed());
      EXPECT_FALSE(subframe_observer.is_error());
      EXPECT_EQ(allowed_subframe_final_url,
                subframe_observer.last_committed_url());
      ExpectChildFrameCollapsed(shell(), kChildFrameId,
                                false /* expect_collapsed */);
    }

    {
      SCOPED_TRACE("Subsequent subframe navigation is blocked.");
      NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                                 blocked_subframe_url);

      ASSERT_TRUE(NavigateIframeToURL(shell()->web_contents(), kChildFrameId,
                                      blocked_subframe_url));

      EXPECT_TRUE(subframe_observer.has_committed());
      EXPECT_TRUE(subframe_observer.is_error());
      EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, subframe_observer.net_error_code());
      ExpectChildFrameCollapsed(shell(), kChildFrameId,
                                true /* expect_collapsed */);
    }
  }
}

// BLOCK_REQUEST_AND_COLLAPSE should block the navigation in legacy <frame>'s,
// but should not collapse the <frame> element itself for legacy reasons.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ThrottleBlockAndCollapse_LegacyFrameNotCollapsed) {
  const char kChildFrameId[] = "child0";
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/legacy_frameset.html"));
  GURL blocked_subframe_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL allowed_subframe_url(
      embedded_test_server()->GetURL("a.com", "/title2.html"));

  TestNavigationThrottleInstaller subframe_throttle_installer(
      shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, blocked_subframe_url);

  {
    SCOPED_TRACE("Initial navigation blocked on main frame load.");
    NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                               blocked_subframe_url);

    ASSERT_TRUE(NavigateToURL(shell(), main_url));
    EXPECT_TRUE(subframe_observer.is_error());
    EXPECT_TRUE(subframe_observer.has_committed());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, subframe_observer.net_error_code());
    ExpectChildFrameSetAsCollapsedInFTN(shell(), true /* expect_collapsed */);
    ExpectChildFrameCollapsedInLayout(shell(), kChildFrameId,
                                      false /* expect_collapsed */);
  }

  {
    SCOPED_TRACE("Subsequent subframe navigation is allowed.");
    NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                               allowed_subframe_url);

    ASSERT_TRUE(NavigateIframeToURL(shell()->web_contents(), kChildFrameId,
                                    allowed_subframe_url));
    EXPECT_TRUE(subframe_observer.has_committed());
    EXPECT_FALSE(subframe_observer.is_error());
    EXPECT_EQ(allowed_subframe_url, subframe_observer.last_committed_url());
    ExpectChildFrameCollapsed(shell(), kChildFrameId,
                              false /* expect_collapsed */);
  }
}

// Checks that the RequestContextType value is properly set.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyRequestContextTypeForFrameTree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c())"));
  GURL c_url(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c()"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);
  TestNavigationManager main_manager(shell()->web_contents(), main_url);
  TestNavigationManager b_manager(shell()->web_contents(), b_url);
  TestNavigationManager c_manager(shell()->web_contents(), c_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());

  // Starts and verifies the main frame navigation.
  shell()->LoadURL(main_url);
  EXPECT_TRUE(main_manager.WaitForRequestStart());
  // For each navigation a new throttle should have been installed.
  EXPECT_EQ(1, installer.install_count());
  // Checks the only URL recorded so far is the one expected for the main frame.
  EXPECT_EQ(main_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());
  // Checks the main frame RequestContextType.
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            installer.navigation_throttle()->request_context_type());

  // Ditto for frame b navigation.
  main_manager.WaitForNavigationFinished();
  EXPECT_TRUE(b_manager.WaitForRequestStart());
  EXPECT_EQ(2, installer.install_count());
  EXPECT_EQ(b_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            installer.navigation_throttle()->request_context_type());

  // Ditto for frame c navigation.
  b_manager.WaitForNavigationFinished();
  EXPECT_TRUE(c_manager.WaitForRequestStart());
  EXPECT_EQ(3, installer.install_count());
  EXPECT_EQ(c_url, url_recorder.urls().back());
  EXPECT_EQ(3ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            installer.navigation_throttle()->request_context_type());

  // Lets the final navigation finish so that we conclude running the
  // RequestContextType checks that happen in TestNavigationThrottle.
  c_manager.WaitForNavigationFinished();
  // Confirms the last navigation did finish.
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the RequestContextType value is properly set for an hyper-link.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyHyperlinkRequestContextType) {
  GURL link_url(embedded_test_server()->GetURL("/title2.html"));
  GURL document_url(embedded_test_server()->GetURL("/simple_links.html"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);
  TestNavigationManager link_manager(shell()->web_contents(), link_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());

  // Navigate to a page with a link.
  EXPECT_TRUE(NavigateToURL(shell(), document_url));
  EXPECT_EQ(document_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());

  // Starts the navigation from a link click and then check it.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickSameSiteLink());",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(link_manager.WaitForRequestStart());
  EXPECT_EQ(link_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            installer.navigation_throttle()->request_context_type());

  // Finishes the last navigation.
  link_manager.WaitForNavigationFinished();
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the RequestContextType value is properly set for an form (POST).
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       VerifyFormRequestContextType) {
  GURL document_url(
      embedded_test_server()->GetURL("/session_history/form.html"));
  GURL post_url(embedded_test_server()->GetURL("/echotitle"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);
  TestNavigationManager post_manager(shell()->web_contents(), post_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());

  // Navigate to a page with a form.
  EXPECT_TRUE(NavigateToURL(shell(), document_url));
  EXPECT_EQ(document_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());

  // Executes the form POST navigation and then check it.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  GURL submit_url("javascript:submitForm('isubmit')");
  shell()->LoadURL(submit_url);
  EXPECT_TRUE(post_manager.WaitForRequestStart());
  EXPECT_EQ(post_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::FORM,
            installer.navigation_throttle()->request_context_type());

  // Finishes the last navigation.
  post_manager.WaitForNavigationFinished();
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the error code is properly set on the NavigationHandle when a
// NavigationThrottle cancels.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       ErrorCodeOnThrottleCancelNavigation) {
  const GURL kUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kRedirectingUrl =
      embedded_test_server()->GetURL("/server-redirect?" + kUrl.spec());

  {
    // Set up a NavigationThrottle that will cancel the navigation in
    // WillStartRequest.
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::CANCEL_AND_IGNORE,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), kUrl);

    // Try to navigate to the url. The navigation should be canceled and the
    // NavigationHandle should have the right error code.
    EXPECT_FALSE(NavigateToURL(shell(), kUrl));
    EXPECT_EQ(net::ERR_ABORTED, observer.net_error_code());
  }

  {
    // Set up a NavigationThrottle that will cancel the navigation in
    // WillRedirectRequest.
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::CANCEL_AND_IGNORE, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), kRedirectingUrl);

    // Try to navigate to the url. The navigation should be canceled and the
    // NavigationHandle should have the right error code.
    EXPECT_FALSE(NavigateToURL(shell(), kRedirectingUrl));
    EXPECT_EQ(net::ERR_ABORTED, observer.net_error_code());
  }

  {
    // Set up a NavigationThrottle that will cancel the navigation in
    // WillProcessResponse.
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::CANCEL_AND_IGNORE);
    NavigationHandleObserver observer(shell()->web_contents(), kUrl);

    // Try to navigate to the url. The navigation should be canceled and the
    // NavigationHandle should have the right error code.
    EXPECT_FALSE(NavigateToURL(shell(), kUrl));
    EXPECT_EQ(net::ERR_ABORTED, observer.net_error_code());
  }

  {
    // Set up a NavigationThrottle that will block the navigation in
    // WillStartRequest.
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), kUrl);

    // Try to navigate to the url. The navigation should be canceled and the
    // NavigationHandle should have the right error code.
    EXPECT_FALSE(NavigateToURL(shell(), kUrl));
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.net_error_code());
  }

  // Set up a NavigationThrottle that will block the navigation in
  // WillRedirectRequest.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::BLOCK_REQUEST, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);
  NavigationHandleObserver observer(shell()->web_contents(), kRedirectingUrl);

  // Try to navigate to the url. The navigation should be canceled and the
  // NavigationHandle should have the right error code.
  EXPECT_FALSE(NavigateToURL(shell(), kRedirectingUrl));
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.net_error_code());
}

// Checks that there's no UAF if NavigationHandleImpl::WillStartRequest cancels
// the navigation.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       CancelNavigationInWillStartRequest) {
  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  // First make a successful commit, as this issue only reproduces when there
  // are existing entries (i.e. when NavigationControllerImpl::GetVisibleEntry
  // has safe_to_show_pending=false).
  EXPECT_TRUE(NavigateToURL(shell(), kUrl1));

  // To take the path that doesn't run beforeunload, so that
  // NavigationControllerImpl::NavigateToPendingEntry is on the botttom of the
  // stack when NavigationHandleImpl::WillStartRequest is called.
  CrashTab(shell()->web_contents());

  // Set up a NavigationThrottle that will cancel the navigation in
  // WillStartRequest.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL_AND_IGNORE,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), kUrl2));
}

// Specialized test that verifies the NavigationHandle gets the HTTPS upgraded
// URL from the very beginning of the navigation.
class NavigationHandleImplHttpsUpgradeBrowserTest
    : public NavigationHandleImplBrowserTest {
 public:
  void CheckHttpsUpgradedIframeNavigation(const GURL& start_url,
                                          const GURL& iframe_secure_url) {
    ASSERT_TRUE(start_url.SchemeIs(url::kHttpScheme));
    ASSERT_TRUE(iframe_secure_url.SchemeIs(url::kHttpsScheme));

    NavigationStartUrlRecorder url_recorder(shell()->web_contents());
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    TestNavigationManager navigation_manager(shell()->web_contents(),
                                             iframe_secure_url);

    // Load the page and wait for the frame load with the expected URL.
    // Note: if the test times out while waiting then a navigation to
    // iframe_secure_url never happened and the expected upgrade may not be
    // working.
    shell()->LoadURL(start_url);
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());

    // The main frame should have finished navigating while the iframe should
    // have just started.
    EXPECT_EQ(2, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(1, installer.will_process_called());

    // Check the correct start URLs have been registered.
    EXPECT_EQ(iframe_secure_url, url_recorder.urls().back());
    EXPECT_EQ(start_url, url_recorder.urls().front());
    EXPECT_EQ(2ul, url_recorder.urls().size());
  }
};

// Tests that the start URL is HTTPS upgraded for a same site navigation.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplHttpsUpgradeBrowserTest,
                       StartUrlIsHttpsUpgradedSameSite) {
  GURL start_url(embedded_test_server()->GetURL(
      "example.com", "/https_upgrade_same_site.html"));

  // Builds the expected upgraded same site URL.
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("https");
  GURL cross_site_iframe_secure_url =
      embedded_test_server()
          ->GetURL("example.com", "/title1.html")
          .ReplaceComponents(replace_scheme);

  CheckHttpsUpgradedIframeNavigation(start_url, cross_site_iframe_secure_url);
}

// Tests that the start URL is HTTPS upgraded for a cross site navigation.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplHttpsUpgradeBrowserTest,
                       StartUrlIsHttpsUpgradedCrossSite) {
  GURL start_url(
      embedded_test_server()->GetURL("/https_upgrade_cross_site.html"));
  GURL cross_site_iframe_secure_url("https://other.com/title1.html");

  CheckHttpsUpgradedIframeNavigation(start_url, cross_site_iframe_secure_url);
}

// Ensure that browser-initiated same-document navigations are detected and
// don't issue network requests.  See crbug.com/663777.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       SameDocumentBrowserInitiatedNoReload) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_fragment_1(embedded_test_server()->GetURL("/title1.html#id_1"));
  GURL url_fragment_2(embedded_test_server()->GetURL("/title1.html#id_2"));

  // 1) Perform a new-document navigation.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_FALSE(observer.is_same_document());
  }

  // 2) Perform a same-document navigation by adding a fragment.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment_1);
    EXPECT_TRUE(NavigateToURL(shell(), url_fragment_1));
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_TRUE(observer.is_same_document());
  }

  // 3) Perform a same-document navigation by modifying the fragment.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment_2);
    EXPECT_TRUE(NavigateToURL(shell(), url_fragment_2));
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_TRUE(observer.is_same_document());
  }

  // 4) Redo the last navigation, but this time it should trigger a reload.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment_2);
    EXPECT_TRUE(NavigateToURL(shell(), url_fragment_2));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_FALSE(observer.is_same_document());
  }

  // 5) Perform a new-document navigation by removing the fragment.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_FALSE(observer.is_same_document());
  }
}

// Record and list the navigations that are started and finished.
class NavigationLogger : public WebContentsObserver {
 public:
  NavigationLogger(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    started_navigation_urls_.push_back(navigation_handle->GetURL());
  }

  void DidRedirectNavigation(NavigationHandle* navigation_handle) override {
    redirected_navigation_urls_.push_back(navigation_handle->GetURL());
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    finished_navigation_urls_.push_back(navigation_handle->GetURL());
  }

  const std::vector<GURL>& started_navigation_urls() const {
    return started_navigation_urls_;
  }
  const std::vector<GURL>& redirected_navigation_urls() const {
    return redirected_navigation_urls_;
  }
  const std::vector<GURL>& finished_navigation_urls() const {
    return finished_navigation_urls_;
  }

 private:
  std::vector<GURL> started_navigation_urls_;
  std::vector<GURL> redirected_navigation_urls_;
  std::vector<GURL> finished_navigation_urls_;
};

// There was a bug without PlzNavigate that happened when a navigation was
// blocked after a redirect. Blink didn't know about the redirect and tried
// to commit an error page to the pre-redirect URL. The result was that the
// NavigationHandle was not found on the browser-side and a new NavigationHandle
// created for committing the error page. This test makes sure that only one
// NavigationHandle is used for committing the error page.
// See https://crbug.com/695421
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, BlockedOnRedirect) {
  const GURL kUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kRedirectingUrl =
      embedded_test_server()->GetURL("/server-redirect?" + kUrl.spec());

  // Set up a NavigationThrottle that will block the navigation in
  // WillRedirectRequest.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::BLOCK_REQUEST, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);
  NavigationHandleObserver observer(shell()->web_contents(), kRedirectingUrl);
  NavigationLogger logger(shell()->web_contents());

  // Try to navigate to the url. The navigation should be canceled and the
  // NavigationHandle should have the right error code.
  EXPECT_FALSE(NavigateToURL(shell(), kRedirectingUrl));
  // EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.net_error_code());

  // Only one navigation is expected to happen.
  std::vector<GURL> started_navigation = {kRedirectingUrl};
  EXPECT_EQ(started_navigation, logger.started_navigation_urls());

  std::vector<GURL> finished_navigation = {kUrl};
  EXPECT_EQ(finished_navigation, logger.finished_navigation_urls());
}

// Tests that when a navigation starts while there's an existing one, the first
// one has the right error code set on its navigation handle.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ErrorCodeOnCancel) {
  GURL slow_url = embedded_test_server()->GetURL("/slow?60");
  NavigationHandleObserver observer(shell()->web_contents(), slow_url);
  shell()->LoadURL(slow_url);

  GURL url2(embedded_test_server()->GetURL("/title1.html"));
  TestNavigationObserver same_tab_observer(
      shell()->web_contents(), 1);
  shell()->LoadURL(url2);
  same_tab_observer.Wait();

  EXPECT_EQ(net::ERR_ABORTED, observer.net_error_code());
}

// Tests that when a renderer-initiated request redirects to a URL that the
// renderer can't access, the right error code is set on the NavigationHandle.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, ErrorCodeOnRedirect) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL redirect_url = embedded_test_server()->GetURL(
      std::string("/server-redirect?") + kChromeUINetworkErrorsListingURL);
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecuteScript(shell(), base::StringPrintf("location.href = '%s';",
                                                redirect_url.spec().c_str())));
  same_tab_observer.Wait();
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, observer.net_error_code());
}

// This class allows running tests with PlzNavigate enabled, regardless of
// default test configuration.
// TODO(clamy): Make those regular NavigationHandleImplBrowserTests.
class PlzNavigateNavigationHandleImplBrowserTest : public ContentBrowserTest {
 public:
  PlzNavigateNavigationHandleImplBrowserTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Test to verify that error pages caused by NavigationThrottle blocking a
// request in the main frame from being made are properly committed in a
// separate error page process.
IN_PROC_BROWSER_TEST_F(PlzNavigateNavigationHandleImplBrowserTest,
                       ErrorPageBlockedNavigation) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL blocked_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));

  {
    NavigationHandleObserver observer(shell()->web_contents(), start_url);
    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
  }

  scoped_refptr<SiteInstance> site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  auto installer = std::make_unique<TestNavigationThrottleInstaller>(
      shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  {
    // A blocked, renderer-initiated navigation in the main frame should commit
    // an error page in a new process.
    NavigationHandleObserver observer(shell()->web_contents(), blocked_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(
        ExecuteScript(shell(), base::StringPrintf("location.href = '%s'",
                                                  blocked_url.spec().c_str())));
    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_NE(site_instance,
                shell()->web_contents()->GetMainFrame()->GetSiteInstance());
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
    } else {
      EXPECT_EQ(site_instance,
                shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    }
  }

  {
    // Reloading the blocked document from the browser process still ends up
    // in the error page process.
    int process_id =
        shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
    NavigationHandleObserver observer(shell()->web_contents(), blocked_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

    shell()->Reload();
    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
      EXPECT_EQ(process_id,
                shell()->web_contents()->GetMainFrame()->GetProcess()->GetID());
    } else if (AreAllSitesIsolatedForTesting()) {
      EXPECT_NE(site_instance,
                shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    } else {
      EXPECT_EQ(site_instance,
                shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    }
  }

  installer.reset();

  {
    // With the throttle uninstalled, going back should return to |start_url| in
    // the same process, and clear the error page.
    NavigationHandleObserver observer(shell()->web_contents(), start_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

    shell()->GoBackOrForward(-1);
    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(site_instance,
              shell()->web_contents()->GetMainFrame()->GetSiteInstance());
  }

  installer = std::make_unique<TestNavigationThrottleInstaller>(
      shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  {
    // A blocked, browser-initiated navigation should commit an error page in a
    // different process.
    NavigationHandleObserver observer(shell()->web_contents(), blocked_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

    EXPECT_FALSE(NavigateToURL(shell(), blocked_url));

    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_NE(site_instance,
              shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
    }
  }

  installer.reset();

  {
    // A blocked subframe navigation should commit an error page in the same
    // process.
    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    const std::string javascript =
        "var i = document.createElement('iframe');"
        "i.src = '" +
        blocked_url.spec() +
        "';"
        "document.body.appendChild(i);";

    installer = std::make_unique<TestNavigationThrottleInstaller>(
        shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED);

    content::RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(content::ExecuteScript(rfh, javascript));
    navigation_observer.Wait();

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    ASSERT_EQ(1u, root->child_count());
    FrameTreeNode* child = root->child_at(0u);

    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
    EXPECT_NE(kUnreachableWebDataURL,
              child->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }
}

// Test to verify that error pages caused by network error or other
// recoverable error are properly committed in the process for the
// destination URL.
IN_PROC_BROWSER_TEST_F(PlzNavigateNavigationHandleImplBrowserTest,
                       ErrorPageNetworkError) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/close-socket"));
  EXPECT_NE(start_url.host(), error_url.host());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));

  {
    NavigationHandleObserver observer(shell()->web_contents(), start_url);
    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
  }

  scoped_refptr<SiteInstance> site_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();
  {
    NavigationHandleObserver observer(shell()->web_contents(), error_url);
    EXPECT_FALSE(NavigateToURL(shell(), error_url));
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_NE(site_instance,
              shell()->web_contents()->GetMainFrame()->GetSiteInstance());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
    }
  }
}

// Tests the case where a browser-initiated navigation to a normal webpage is
// blocked (net::ERR_BLOCKED_BY_CLIENT) while departing from a privileged WebUI
// page (chrome://gpu). It is a security risk for the error page to commit in
// the privileged process.
IN_PROC_BROWSER_TEST_F(PlzNavigateNavigationHandleImplBrowserTest,
                       BlockedRequestAfterWebUI) {
  GURL web_ui_url("chrome://gpu");
  WebContents* web_contents = shell()->web_contents();

  // Navigate to the initial page.
  EXPECT_FALSE(web_contents->GetMainFrame()->GetEnabledBindings() &
               BINDINGS_POLICY_WEB_UI);
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));
  EXPECT_TRUE(web_contents->GetMainFrame()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);
  scoped_refptr<SiteInstance> web_ui_process = web_contents->GetSiteInstance();

  // Start a new, non-webUI navigation that will be blocked by a
  // NavigationThrottle.
  GURL blocked_url("http://blocked-by-throttle.example.cc");
  TestNavigationThrottleInstaller installer(
      web_contents, NavigationThrottle::BLOCK_REQUEST,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);
  NavigationHandleObserver commit_observer(web_contents, blocked_url);
  EXPECT_FALSE(NavigateToURL(shell(), blocked_url));
  NavigationEntry* last_committed =
      web_contents->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_committed);
  EXPECT_EQ(blocked_url, last_committed->GetVirtualURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, last_committed->GetPageType());
  EXPECT_NE(web_ui_process.get(), web_contents->GetSiteInstance());
  EXPECT_TRUE(commit_observer.has_committed());
  EXPECT_TRUE(commit_observer.is_error());
  EXPECT_FALSE(commit_observer.is_renderer_initiated());
}

// Redirects to renderer debug URLs caused problems.
// See https://crbug.com/728398.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       RedirectToRendererDebugUrl) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  const GURL kTestUrls[] = {GURL("javascript:window.alert('hello')"),
                            GURL(kChromeUIBadCastCrashURL),
                            GURL(kChromeUICrashURL),
                            GURL(kChromeUIDumpURL),
                            GURL(kChromeUIKillURL),
                            GURL(kChromeUIHangURL),
                            GURL(kChromeUIShorthangURL),
                            GURL(kChromeUIMemoryExhaustURL)};

  for (const auto& test_url : kTestUrls) {
    SCOPED_TRACE(testing::Message() << "renderer_debug_url = " << test_url);

    GURL redirecting_url =
        embedded_test_server()->GetURL("/server-redirect?" + test_url.spec());

    NavigationHandleObserver observer(shell()->web_contents(), redirecting_url);
    NavigationLogger logger(shell()->web_contents());

    // Try to navigate to the url. The navigation should be canceled and the
    // NavigationHandle should have the right error code.
    EXPECT_FALSE(NavigateToURL(shell(), redirecting_url));
    EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, observer.net_error_code());

    // Both WebContentsObserver::{DidStartNavigation, DidFinishNavigation}
    // are called, but no WebContentsObserver::DidRedirectNavigation.
    std::vector<GURL> started_navigation = {redirecting_url};
    std::vector<GURL> redirected_navigation;  // Empty.
    std::vector<GURL> finished_navigation = {redirecting_url};
    EXPECT_EQ(started_navigation, logger.started_navigation_urls());
    EXPECT_EQ(redirected_navigation, logger.redirected_navigation_urls());
    EXPECT_EQ(finished_navigation, logger.finished_navigation_urls());
  }
}

// Check that iframe with embedded credentials are blocked.
// See https://crbug.com/755892.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       BlockCredentialedSubresources) {
  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources))
    return;

  const struct {
    GURL main_url;
    GURL iframe_url;
    bool blocked;
  } kTestCases[] = {
      // Normal case with no credential in neither of the urls.
      {GURL("http://a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://a.com/title1.html"), false},

      // Username in the iframe, but nothing in the main frame.
      {GURL("http://a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user@a.com/title1.html"), true},

      // Username and password in the iframe, but none in the main frame.
      {GURL("http://a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user:pass@a.com/title1.html"), true},

      // Username and password in the main frame, but none in the iframe.
      {GURL("http://user:pass@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://a.com/title1.html"), false},

      // Same usernames in both frames.
      // Relative URLs on top-level pages that were loaded with embedded
      // credentials should load correctly. It doesn't work correctly when
      // PlzNavigate is disabled. See https://crbug.com/756846.
      {GURL("http://user@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user@a.com/title1.html"), false},

      // Same usernames and passwords in both frames.
      // Relative URLs on top-level pages that were loaded with embedded
      // credentials should load correctly. It doesn't work correctly when
      // PlzNavigate is disabled. See https://crbug.com/756846.
      {GURL("http://user:pass@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user:pass@a.com/title1.html"), false},

      // Different usernames.
      {GURL("http://user@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://wrong@a.com/title1.html"), true},

      // Different passwords.
      {GURL("http://user:pass@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user:wrong@a.com/title1.html"), true},

      // Different usernames and same passwords.
      {GURL("http://user:pass@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://wrong:pass@a.com/title1.html"), true},

      // Different origins.
      {GURL("http://user:pass@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user:pass@b.com/title1.html"), true},
  };
  for (const auto test_case : kTestCases) {
    // Modify the URLs port to use the embedded test server's port.
    std::string port_str(std::to_string(embedded_test_server()->port()));
    GURL::Replacements set_port;
    set_port.SetPortStr(port_str);
    GURL main_url(test_case.main_url.ReplaceComponents(set_port));
    GURL iframe_url_final(test_case.iframe_url.ReplaceComponents(set_port));
    GURL iframe_url_with_redirect = GURL(embedded_test_server()->GetURL(
        "/server-redirect?" + iframe_url_final.spec()));

    ASSERT_TRUE(NavigateToURL(shell(), main_url));

    // Blocking the request must work, even after a redirect.
    for (bool redirect : {false, true}) {
      const GURL& iframe_url =
          redirect ? iframe_url_with_redirect : iframe_url_final;
      SCOPED_TRACE(::testing::Message()
                   << std::endl
                   << "- main_url = " << main_url << std::endl
                   << "- iframe_url = " << iframe_url << std::endl);
      NavigationHandleObserver subframe_observer(shell()->web_contents(),
                                                 iframe_url);
      TestNavigationThrottleInstaller installer(
          shell()->web_contents(), NavigationThrottle::PROCEED,
          NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
          NavigationThrottle::PROCEED);

      NavigateIframeToURL(shell()->web_contents(), "child0", iframe_url);

      FrameTreeNode* root =
          static_cast<WebContentsImpl*>(shell()->web_contents())
              ->GetFrameTree()
              ->root();
      ASSERT_EQ(1u, root->child_count());
      if (test_case.blocked) {
        EXPECT_EQ(redirect, !!installer.will_start_called());
        EXPECT_FALSE(installer.will_process_called());
        EXPECT_FALSE(subframe_observer.has_committed());
        EXPECT_TRUE(subframe_observer.last_committed_url().is_empty());
        EXPECT_NE(iframe_url_final, root->child_at(0u)->current_url());
      } else {
        EXPECT_TRUE(installer.will_start_called());
        EXPECT_TRUE(installer.will_process_called());
        EXPECT_TRUE(subframe_observer.has_committed());
        EXPECT_EQ(iframe_url_final, subframe_observer.last_committed_url());
        EXPECT_EQ(iframe_url_final, root->child_at(0u)->current_url());
      }
    }
  }
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest, StartToCommitMetrics) {
  enum class FrameType {
    kMain,
    kSub,
  };
  enum class ProcessType {
    kCross,
    kSame,
  };
  enum class TransitionType {
    kNew,
    kReload,
    kBackForward,
  };

  // Uses the provided ProcessType, FrameType, and TransitionType expected for
  // this navigation to generate all combinations of Navigation.StartToCommit
  // metrics.
  auto check_navigation = [](const base::HistogramTester& histograms,
                             ProcessType process_type, FrameType frame_type,
                             TransitionType transition_type) {
    const std::map<FrameType, std::string> kFrameSuffixes = {
        {FrameType::kMain, ".MainFrame"}, {FrameType::kSub, ".Subframe"}};
    const std::map<ProcessType, std::string> kProcessSuffixes = {
        {ProcessType::kCross, ".CrossProcess"},
        {ProcessType::kSame, ".SameProcess"}};
    const std::map<TransitionType, std::string> kTransitionSuffixes = {
        {TransitionType::kNew, ".NewNavigation"},
        {TransitionType::kReload, ".Reload"},
        {TransitionType::kBackForward, ".BackForward"},
    };

    // Add the suffix to all existing histogram names, and append the results to
    // |names|.
    std::vector<std::string> names{"Navigation.StartToCommit"};
    auto add_suffix = [&names](std::vector<std::string> suffixes) {
      size_t original_size = names.size();
      for (size_t i = 0; i < original_size; i++) {
        for (const std::string& suffix : suffixes)
          names.push_back(names[i] + suffix);
      }
    };
    add_suffix({kProcessSuffixes.at(process_type)});
    add_suffix({kFrameSuffixes.at(frame_type)});
    add_suffix({kTransitionSuffixes.at(transition_type),
                ".ForegroundProcessPriority"});

    // Check that all generated histogram names are logged exactly once.
    for (const auto& name : names) {
      histograms.ExpectTotalCount(name, 1);
    }

    // Check that no additional histograms with the StartToCommit prefix were
    // logged.
    base::HistogramTester::CountsMap counts =
        histograms.GetTotalCountsForPrefix("Navigation.StartToCommit");
    int32_t total_counts = 0;
    for (const auto& it : counts) {
      total_counts += it.second;
    }
    EXPECT_EQ(static_cast<int32_t>(names.size()), total_counts);
  };

  // Main frame tests.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/hello.html"));
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    check_navigation(histograms, ProcessType::kSame, FrameType::kMain,
                     TransitionType::kNew);
  }
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    NavigateToURL(shell(), url);
    check_navigation(histograms, ProcessType::kCross, FrameType::kMain,
                     TransitionType::kNew);
  }
  {
    base::HistogramTester histograms;
    ReloadBlockUntilNavigationsComplete(shell(), 1);
    check_navigation(histograms, ProcessType::kSame, FrameType::kMain,
                     TransitionType::kReload);
  }
  {
    base::HistogramTester histograms;
    TestNavigationObserver nav_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    nav_observer.Wait();
    check_navigation(histograms, ProcessType::kCross, FrameType::kMain,
                     TransitionType::kBackForward);
  }
  {
    base::HistogramTester histograms;
    TestNavigationObserver nav_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    nav_observer.Wait();
    check_navigation(histograms, ProcessType::kSame, FrameType::kMain,
                     TransitionType::kBackForward);
  }
  {
    base::HistogramTester histograms;
    NavigateToURL(shell(), GURL(url::kAboutBlankURL));
    check_navigation(histograms, ProcessType::kSame, FrameType::kMain,
                     TransitionType::kNew);
  }

  // Subframe tests. All of these tests just navigate a frame within
  // page_with_iframe.html.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html")));
  FrameTreeNode* first_child =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->child_at(0);
  {
    base::HistogramTester histograms;
    EXPECT_TRUE(NavigateToURLFromRenderer(
        first_child, embedded_test_server()->GetURL("c.com", "/title1.html")));
    check_navigation(histograms, ProcessType::kCross, FrameType::kSub,
                     TransitionType::kNew);
  }
  {
    base::HistogramTester histograms;
    TestFrameNavigationObserver nav_observer(first_child);
    EXPECT_TRUE(ExecuteScript(first_child, "location.reload();"));
    nav_observer.Wait();
    // location.reload triggers the PAGE_TRANSITION_AUTO_SUBFRAME which
    // corresponds to NewNavigation.
    check_navigation(histograms, ProcessType::kSame, FrameType::kSub,
                     TransitionType::kNew);
  }
  {
    base::HistogramTester histograms;
    shell()->GoBackOrForward(-1);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    // History back triggers the PAGE_TRANSITION_AUTO_SUBFRAME which corresponds
    // to NewNavigation.
    check_navigation(histograms, ProcessType::kCross, FrameType::kSub,
                     TransitionType::kNew);
  }
  EXPECT_TRUE(NavigateToURLFromRenderer(
      first_child, embedded_test_server()->GetURL("/simple_links.html")));
  {
    base::HistogramTester histograms;
    TestFrameNavigationObserver nav_observer(first_child);
    EXPECT_TRUE(ExecuteScript(first_child, "clickSameSiteLink();"));
    nav_observer.Wait();
    // Link clicking will trigger PAGE_TRANSITION_MANUAL_SUBFRAME which
    // corresponds to NewNavigation.
    check_navigation(histograms, ProcessType::kSame, FrameType::kSub,
                     TransitionType::kNew);
  }
}

// Verify that the TimeToReadyToCommit metrics are correctly logged for
// SameProcess vs CrossProcess as well as MainFrame vs Subframe cases.
IN_PROC_BROWSER_TEST_F(NavigationHandleImplBrowserTest,
                       TimeToReadyToCommitMetrics) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/hello.html")));

  // Check that only SameProcess version is logged and not CrossProcess.
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    base::HistogramTester::CountsMap expected_counts = {
        {"Navigation.TimeToReadyToCommit.MainFrame", 1},
        {"Navigation.TimeToReadyToCommit.MainFrame.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit.SameProcess", 1},
        {"Navigation.TimeToReadyToCommit.SameProcess.NewNavigation", 1}};
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("Navigation.TimeToReadyToCommit."),
        testing::ContainerEq(expected_counts));
  }

  // Navigate cross-process and ensure that only CrossProcess is logged.
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("a.com", "/title2.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    base::HistogramTester::CountsMap expected_counts = {
        {"Navigation.TimeToReadyToCommit.MainFrame", 1},
        {"Navigation.TimeToReadyToCommit.MainFrame.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit.CrossProcess", 1},
        {"Navigation.TimeToReadyToCommit.CrossProcess.NewNavigation", 1}};
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("Navigation.TimeToReadyToCommit."),
        testing::ContainerEq(expected_counts));
  }

  // Add a new subframe.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_TRUE(ExecuteScript(
      root, "document.body.appendChild(document.createElement('iframe'));"));

  // Navigate subframe cross-site and ensure Subframe metrics are logged.
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("b.com", "/title3.html"));
    NavigateFrameToURL(root->child_at(0), url);

    std::string navigation_type =
        AreAllSitesIsolatedForTesting() ? "CrossProcess" : "SameProcess";
    base::HistogramTester::CountsMap expected_counts = {
        {"Navigation.TimeToReadyToCommit.Subframe", 1},
        {"Navigation.TimeToReadyToCommit.Subframe.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit.NewNavigation", 1},
        {base::StringPrintf("Navigation.TimeToReadyToCommit.%s",
                            navigation_type.c_str()),
         1},
        {base::StringPrintf("Navigation.TimeToReadyToCommit.%s.NewNavigation",
                            navigation_type.c_str()),
         1}};
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("Navigation.TimeToReadyToCommit."),
        testing::ContainerEq(expected_counts));
  }
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest, IsDownload) {
  GURL url(embedded_test_server()->GetURL("/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  NavigateToURL(shell(), url);
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       DownloadFalseForHtmlResponse) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  NavigateToURL(shell(), url);
  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       DownloadFalseFor404) {
  GURL url(embedded_test_server()->GetURL("/page404.html"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  NavigateToURL(shell(), url);
  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       DownloadFalseForFailedNavigation) {
  GURL url(embedded_test_server()->GetURL("/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), url));
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_FALSE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       RedirectToDownload) {
  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  NavigateToURL(shell(), redirect_url);
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_TRUE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       RedirectToDownloadFails) {
  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::CANCEL, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_FALSE(observer.is_download());
  EXPECT_TRUE(observer.is_error());
  EXPECT_TRUE(observer.was_redirected());
}

// Set of tests that check the various NavigationThrottle events can be used
// with custom error pages.
class NavigationHandleImplThrottleResultWithErrorPageBrowserTest
    : public NavigationHandleImplBrowserTest,
      public ::testing::WithParamInterface<NavigationThrottle::ThrottleAction> {
};

IN_PROC_BROWSER_TEST_P(
    NavigationHandleImplThrottleResultWithErrorPageBrowserTest,
    WillStartRequest) {
  NavigationThrottle::ThrottleAction action = GetParam();

  if (action == NavigationThrottle::CANCEL_AND_IGNORE) {
    // There is no support for CANCEL_AND_IGNORE and a custom error page.
    return;
  }

  if (action == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    // This can only be returned from sub frame navigations.
    return;
  }

  if (action == NavigationThrottle::PROCEED ||
      action == NavigationThrottle::DEFER) {
    // Neither is relevant for what we want to test i.e. error pages.
    return;
  }

  if (action == NavigationThrottle::BLOCK_RESPONSE) {
    // BLOCK_RESPONSE can't be used with WillStartRequest.
    return;
  }

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  InstallThrottleAndTestNavigationCommittedWithErrorPage(
      url, action, TestNavigationThrottleInstaller::WILL_START_REQUEST);
}

IN_PROC_BROWSER_TEST_P(
    NavigationHandleImplThrottleResultWithErrorPageBrowserTest,
    WillRedirectRequest) {
  NavigationThrottle::ThrottleAction action = GetParam();

  if (action == NavigationThrottle::CANCEL_AND_IGNORE) {
    // There is no support for CANCEL_AND_IGNORE and a custom error page.
    return;
  }

  if (action == NavigationThrottle::PROCEED ||
      action == NavigationThrottle::DEFER) {
    // Neither is relevant for what we want to test i.e. error pages.
    return;
  }

  if (action == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    // This can only be returned from sub frame navigations.
    return;
  }

  if (action == NavigationThrottle::BLOCK_RESPONSE) {
    // BLOCK_RESPONSE can't be used with WillRedirectRequest.
    return;
  }

  GURL url(embedded_test_server()->GetURL("/cross-site/foo.com/title1.html"));
  InstallThrottleAndTestNavigationCommittedWithErrorPage(
      url, action, TestNavigationThrottleInstaller::WILL_REDIRECT_REQUEST);
}

IN_PROC_BROWSER_TEST_P(
    NavigationHandleImplThrottleResultWithErrorPageBrowserTest,
    WillFailRequest) {
  NavigationThrottle::ThrottleAction action = GetParam();

  if (action == NavigationThrottle::PROCEED ||
      action == NavigationThrottle::DEFER) {
    // Neither is relevant for what we want to test i.e. error pages.
    return;
  }

  if (action == NavigationThrottle::BLOCK_REQUEST ||
      action == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    // BLOCK_REQUEST, BLOCK_REQUEST_AND_COLLAPSE, can't be used with
    // WillFailRequest.
    return;
  }

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());

  const GURL url = https_server.GetURL("/title1.html");
  InstallThrottleAndTestNavigationCommittedWithErrorPage(
      url, action, TestNavigationThrottleInstaller::WILL_FAIL_REQUEST);
}

IN_PROC_BROWSER_TEST_P(
    NavigationHandleImplThrottleResultWithErrorPageBrowserTest,
    WillProcessResponse) {
  NavigationThrottle::ThrottleAction action = GetParam();

  if (action == NavigationThrottle::CANCEL_AND_IGNORE) {
    // There is no support for CANCEL_AND_IGNORE and a custom error page.
    return;
  }

  if (action == NavigationThrottle::PROCEED ||
      action == NavigationThrottle::DEFER) {
    // Neither is relevant for what we want to test i.e. error pages.
    return;
  }

  if (action == NavigationThrottle::BLOCK_REQUEST ||
      action == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    // BLOCK_REQUEST and BLOCK_REQUEST_AND_COLLAPSE can't be used with
    // WillProcessResponse.
    return;
  }

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  InstallThrottleAndTestNavigationCommittedWithErrorPage(
      url, action, TestNavigationThrottleInstaller::WILL_PROCESS_RESPONSE);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    NavigationHandleImplThrottleResultWithErrorPageBrowserTest,
    testing::Range(NavigationThrottle::ThrottleAction::FIRST,
                   NavigationThrottle::ThrottleAction::LAST));

// The set of tests...
// * NavigationHandleImplDownloadBrowserTest.AllowedResourceDownloaded
// * NavigationHandleImplDownloadBrowserTest.AllowedResourceNotDownloaded
// * NavigationHandleImplDownloadBrowserTest.Disallowed
//
// ...covers every combinaison of possible states for:
// * CommonNavigationParams::allow_download
// * NavigationHandle::IsDownload()
//
// |allow_download| is false only when the URL is a view-source URL. In this
// case, downloads are prohibited (i.e. |is_download| is false in
// NavigationURLLoaderDelegate::OnResponseStarted()).
IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       AllowedResourceDownloaded) {
  GURL simple_url(embedded_test_server()->GetURL("/simple_page.html"));

  TestNavigationManager manager(shell()->web_contents(), simple_url);
  NavigationHandleObserver handle_observer(shell()->web_contents(), simple_url);

  // Download is allowed.
  shell()->LoadURL(simple_url);
  EXPECT_TRUE(manager.WaitForRequestStart());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(root->navigation_request()->common_params().allow_download);

  // The response is not handled as a download.
  manager.WaitForNavigationFinished();
  EXPECT_FALSE(handle_observer.is_download());
}

// See NavigationHandleImplDownloadBrowserTest.AllowedResourceNotDownloaded
IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest,
                       AllowedResourceNotDownloaded) {
  GURL download_url(embedded_test_server()->GetURL("/download-test1.lib"));

  TestNavigationManager manager(shell()->web_contents(), download_url);
  NavigationHandleObserver handle_observer(shell()->web_contents(),
                                           download_url);

  // Download is allowed.
  shell()->LoadURL(download_url);
  EXPECT_TRUE(manager.WaitForRequestStart());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(root->navigation_request()->common_params().allow_download);

  // The response is handled as a download.
  manager.WaitForNavigationFinished();
  EXPECT_TRUE(handle_observer.is_download());
}

// See NavigationHandleImplDownloadBrowserTest.AllowedResourceNotDownloaded
IN_PROC_BROWSER_TEST_F(NavigationHandleImplDownloadBrowserTest, Disallowed) {
  GURL download_url(embedded_test_server()->GetURL("/download-test1.lib"));

  // An URL is allowed to be a download iff it is not a view-source URL.
  GURL view_source_url =
      GURL(content::kViewSourceScheme + std::string(":") + download_url.spec());

  NavigationHandleObserver handle_observer(shell()->web_contents(),
                                           download_url);
  TestNavigationManager manager(shell()->web_contents(), download_url);

  // Download is not allowed.
  shell()->LoadURL(view_source_url);
  EXPECT_TRUE(manager.WaitForRequestStart());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  EXPECT_FALSE(root->navigation_request()->common_params().allow_download);

  // The response is not handled as a download.
  manager.WaitForNavigationFinished();
  EXPECT_FALSE(handle_observer.is_download());
}

}  // namespace content
