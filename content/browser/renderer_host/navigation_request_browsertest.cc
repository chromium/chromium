// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_request.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_read_context.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/scheme_host_port.h"
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
      NavigationThrottle::ThrottleCheckResult
          will_commit_without_url_loader_result,
      base::OnceClosure did_call_will_start,
      base::OnceClosure did_call_will_redirect,
      base::OnceClosure did_call_will_fail,
      base::OnceClosure did_call_will_process,
      base::OnceClosure did_call_will_commit_without_url_loader)
      : NavigationThrottle(handle),
        will_start_result_(will_start_result),
        will_redirect_result_(will_redirect_result),
        will_fail_result_(will_fail_result),
        will_process_result_(will_process_result),
        will_commit_without_url_loader_result_(
            will_commit_without_url_loader_result),
        did_call_will_start_(std::move(did_call_will_start)),
        did_call_will_redirect_(std::move(did_call_will_redirect)),
        did_call_will_fail_(std::move(did_call_will_fail)),
        did_call_will_process_(std::move(did_call_will_process)),
        did_call_will_commit_without_url_loader_(
            std::move(did_call_will_commit_without_url_loader)) {}
  ~TestNavigationThrottle() override = default;

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
    NavigationRequest* navigation_request =
        NavigationRequest::From(navigation_handle());
    CHECK_NE(blink::mojom::RequestContextType::UNSPECIFIED,
             navigation_request->request_context_type());
    request_context_type_ = navigation_request->request_context_type();

    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(did_call_will_start_));
    return will_start_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    NavigationRequest* navigation_request =
        NavigationRequest::From(navigation_handle());
    CHECK_EQ(request_context_type_, navigation_request->request_context_type());

    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(did_call_will_redirect_));
    return will_redirect_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    NavigationRequest* navigation_request =
        NavigationRequest::From(navigation_handle());
    CHECK_EQ(request_context_type_, navigation_request->request_context_type());

    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(did_call_will_fail_));
    return will_fail_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    NavigationRequest* navigation_request =
        NavigationRequest::From(navigation_handle());
    CHECK_EQ(request_context_type_, navigation_request->request_context_type());

    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(did_call_will_process_));
    return will_process_result_;
  }

  NavigationThrottle::ThrottleCheckResult WillCommitWithoutUrlLoader()
      override {
    NavigationRequest* navigation_request =
        NavigationRequest::From(navigation_handle());
    CHECK_NE(blink::mojom::RequestContextType::UNSPECIFIED,
             navigation_request->request_context_type());
    request_context_type_ = navigation_request->request_context_type();

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(did_call_will_commit_without_url_loader_));
    return will_commit_without_url_loader_result_;
  }

  NavigationThrottle::ThrottleCheckResult will_start_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_result_;
  NavigationThrottle::ThrottleCheckResult will_fail_result_;
  NavigationThrottle::ThrottleCheckResult will_process_result_;
  NavigationThrottle::ThrottleCheckResult
      will_commit_without_url_loader_result_;
  base::OnceClosure did_call_will_start_;
  base::OnceClosure did_call_will_redirect_;
  base::OnceClosure did_call_will_fail_;
  base::OnceClosure did_call_will_process_;
  base::OnceClosure did_call_will_commit_without_url_loader_;
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
    WILL_COMMIT_WITHOUT_URL_LOADER,
  };

  TestNavigationThrottleInstaller(
      WebContents* web_contents,
      NavigationThrottle::ThrottleCheckResult will_start_result,
      NavigationThrottle::ThrottleCheckResult will_redirect_result,
      NavigationThrottle::ThrottleCheckResult will_fail_result,
      NavigationThrottle::ThrottleCheckResult will_process_result,
      NavigationThrottle::ThrottleCheckResult
          will_commit_without_url_loader_result,
      const GURL& expected_start_url = GURL())
      : WebContentsObserver(web_contents),
        will_start_result_(will_start_result),
        will_redirect_result_(will_redirect_result),
        will_fail_result_(will_fail_result),
        will_process_result_(will_process_result),
        will_commit_without_url_loader_result_(
            will_commit_without_url_loader_result),
        expected_start_url_(expected_start_url) {}
  ~TestNavigationThrottleInstaller() override = default;

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
    auto will_commit_without_url_loader_result = will_start_result;

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
      case WILL_COMMIT_WITHOUT_URL_LOADER:
        will_commit_without_url_loader_result = result;
        break;
    }

    return std::make_unique<TestNavigationThrottleInstaller>(
        web_contents, will_start_result, will_redirect_result, will_fail_result,
        will_process_result, will_commit_without_url_loader_result);
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

  void WaitForThrottleWillCommitWithoutUrlLoader() {
    if (will_commit_without_url_loader_called_) {
      return;
    }
    will_commit_without_url_loader_loop_runner_ = new MessageLoopRunner();
    will_commit_without_url_loader_loop_runner_->Run();
    will_commit_without_url_loader_loop_runner_ = nullptr;
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
  int will_commit_without_url_loader_called() {
    return will_commit_without_url_loader_called_;
  }

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

  virtual void DidCallWillCommitWithoutUrlLoader() {
    will_commit_without_url_loader_called_++;
    if (will_commit_without_url_loader_loop_runner_) {
      will_commit_without_url_loader_loop_runner_->Quit();
    }
  }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    if (!expected_start_url_.is_empty() &&
        handle->GetURL() != expected_start_url_)
      return;

    std::unique_ptr<NavigationThrottle> throttle(new TestNavigationThrottle(
        handle, will_start_result_, will_redirect_result_, will_fail_result_,
        will_process_result_, will_commit_without_url_loader_result_,
        base::BindOnce(
            &TestNavigationThrottleInstaller::DidCallWillStartRequest,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(
            &TestNavigationThrottleInstaller::DidCallWillRedirectRequest,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&TestNavigationThrottleInstaller::DidCallWillFailRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(
            &TestNavigationThrottleInstaller::DidCallWillProcessResponse,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(
            &TestNavigationThrottleInstaller::DidCallWillCommitWithoutUrlLoader,
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
  NavigationThrottle::ThrottleCheckResult
      will_commit_without_url_loader_result_;
  int will_start_called_ = 0;
  int will_redirect_called_ = 0;
  int will_fail_called_ = 0;
  int will_process_called_ = 0;
  int will_commit_without_url_loader_called_ = 0;
  raw_ptr<TestNavigationThrottle> navigation_throttle_ = nullptr;
  int install_count_ = 0;
  scoped_refptr<MessageLoopRunner> will_start_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_redirect_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_fail_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_process_loop_runner_;
  scoped_refptr<MessageLoopRunner> will_commit_without_url_loader_loop_runner_;
  GURL expected_start_url_;

  // The throttle installer can be deleted before all tasks posted by its
  // throttles are run, so it must be referenced via weak pointers.
  base::WeakPtrFactory<TestNavigationThrottleInstaller> weak_factory_{this};
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
      NavigationThrottle::ThrottleCheckResult
          will_commit_without_url_loader_result,
      GURL expected_start_url = GURL())
      : TestNavigationThrottleInstaller(web_contents,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        NavigationThrottle::DEFER,
                                        expected_start_url),
        will_start_deferred_result_(will_start_result),
        will_redirect_deferred_result_(will_redirect_result),
        will_fail_deferred_result_(will_fail_result),
        will_process_deferred_result_(will_process_result),
        will_commit_without_url_loader_result_(
            will_commit_without_url_loader_result) {}

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

  void DidCallWillCommitWithoutUrlLoader() override {
    TestNavigationThrottleInstaller::DidCallWillCommitWithoutUrlLoader();
    Continue(will_commit_without_url_loader_result_);
  }

 private:
  NavigationThrottle::ThrottleCheckResult will_start_deferred_result_;
  NavigationThrottle::ThrottleCheckResult will_redirect_deferred_result_;
  NavigationThrottle::ThrottleCheckResult will_fail_deferred_result_;
  NavigationThrottle::ThrottleCheckResult will_process_deferred_result_;
  NavigationThrottle::ThrottleCheckResult
      will_commit_without_url_loader_result_;
};

// Records all navigation start URLs from the WebContents.
class NavigationStartUrlRecorder : public WebContentsObserver {
 public:
  explicit NavigationStartUrlRecorder(WebContents* web_contents)
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
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);
  EXPECT_EQ(expect_collapsed, child->is_collapsed());
}

void ExpectChildFrameCollapsedInLayout(Shell* shell,
                                       const std::string& frame_id,
                                       bool expect_collapsed) {
  // Check if the frame is collapsed in practice.
  const char kScript[] = "document.getElementById(\"%s\").clientWidth;";
  int client_width =
      EvalJs(shell, base::StringPrintf(kScript, frame_id.c_str())).ExtractInt();
  EXPECT_EQ(expect_collapsed, !client_width) << client_width;
}

void ExpectChildFrameCollapsed(Shell* shell,
                               const std::string& frame_id,
                               bool expect_collapsed) {
  ExpectChildFrameSetAsCollapsedInFTN(shell, expect_collapsed);
  ExpectChildFrameCollapsedInLayout(shell, frame_id, expect_collapsed);
}

}  // namespace

class NavigationRequestBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

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

    content::RenderFrameHost* rfh =
        shell()->web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(kBodyTextContent, EvalJs(rfh, "document.body.textContent"));
  }
};

// A test class that calls IsolateAllSitesForTesting() early enough in the setup
// that we get consistent results for AreOriginKeyedProcessesEnabledByDefault()
// if kOriginKeyedProcessesByDefault is enabled. Specifically, make sure the
// initial shell's main frame's BrowsingInstance gets the correct default
// isolation state, which depends on AreOriginKeyedProcessesEnabledByDefault().
class NavigationRequestBrowserTest_IsolateAllSites
    : public NavigationRequestBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NavigationRequestBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }
};

class NavigationRequestDownloadBrowserTest
    : public NavigationRequestBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    NavigationRequestBrowserTest::SetUpOnMainThread();

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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifyPageTransition) {
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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifyFrameTree) {
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
                            ->GetPrimaryFrameTree()
                            .root();

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
  EXPECT_EQ(root->child_at(0)->frame_tree_node_id(),
            b_observer.frame_tree_node_id());

  // Verify the c.com frame.
  EXPECT_TRUE(c_observer.has_committed());
  EXPECT_FALSE(c_observer.is_error());
  EXPECT_EQ(c_url, c_observer.last_committed_url());
  EXPECT_FALSE(c_observer.is_main_frame());
  EXPECT_EQ(root->child_at(0)->child_at(0)->frame_tree_node_id(),
            c_observer.frame_tree_node_id());
}

// Ensure that the WasRedirected() method on NavigationHandle behaves correctly.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifyRedirect) {
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
    GURL final_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
    NavigationHandleObserver observer(shell()->web_contents(), url);

    EXPECT_TRUE(
        NavigateToURL(shell(), url, final_url /* expected_commit_url */));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.was_redirected());
  }
}

// Ensure that we can read and write to the browser process's copy of the blink
// runtime-enabled features via NavigationRequest's method
// GetMutableRuntimeFeatureStateContext() during commit. Then ensure that its
// values are persisted to the RenderFrameHostImpl's read-only context.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RuntimeFeatureStatePersisted) {
  // Sets up the NavigationRequest where we can begin accessing the
  // RuntimeFeatureStateContext class.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  TestNavigationManager manager(shell()->web_contents(), url);

  // Begin loading our url and wait for the request to start.
  shell()->LoadURL(url);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Ensure we can get and set values in RuntimeFeatureStateContext.
  blink::RuntimeFeatureStateContext& context =
      NavigationRequest::From(manager.GetNavigationHandle())
          ->GetMutableRuntimeFeatureStateContext();
  bool is_test_feature_enabled(context.IsTestFeatureEnabled());
  context.SetTestFeatureEnabled(!is_test_feature_enabled);
  EXPECT_EQ(context.IsTestFeatureEnabled(), !is_test_feature_enabled);

  // Check the override value map as well.
  base::flat_map<::blink::mojom::RuntimeFeature, bool>
      expected_feature_overrides = context.GetFeatureOverrides();
  EXPECT_EQ(
      expected_feature_overrides[blink::mojom::RuntimeFeature::kTestFeature],
      !is_test_feature_enabled);

  // Continue with the navigation until completion.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_TRUE(manager.was_successful());

  // Check that the changes were saved to the RenderFrameHost.
  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(document_data);
  blink::RuntimeFeatureStateReadContext read_context =
      document_data->runtime_feature_state_read_context();
  EXPECT_EQ(expected_feature_overrides, read_context.GetFeatureOverrides());
}

// Similar to RuntimeFeatureStatePersisted but ensures that even for
// runtime-enabled feature values that are equivalent to the previous state.
// This is important to persist as it helps the renderer determine force-enabled
// and force-disabled values.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RuntimeFeatureStatePersistedForSameValue) {
  // Sets up the NavigationRequest where we can begin accessing the
  // RuntimeFeatureStateContext class.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  TestNavigationManager manager(shell()->web_contents(), url);

  // Begin loading our url and wait for the request to start.
  shell()->LoadURL(url);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Ensure we can set values in RuntimeFeatureStateContext that are
  // equivalent to the feature's previous state.
  blink::RuntimeFeatureStateContext& context =
      NavigationRequest::From(manager.GetNavigationHandle())
          ->GetMutableRuntimeFeatureStateContext();
  bool is_test_feature_enabled(context.IsTestFeatureEnabled());
  context.SetTestFeatureEnabled(is_test_feature_enabled);
  EXPECT_EQ(context.IsTestFeatureEnabled(), is_test_feature_enabled);

  // Check the override value map as well.
  base::flat_map<::blink::mojom::RuntimeFeature, bool>
      expected_feature_overrides = context.GetFeatureOverrides();
  EXPECT_EQ(
      expected_feature_overrides[blink::mojom::RuntimeFeature::kTestFeature],
      is_test_feature_enabled);

  // Continue with the navigation until completion.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_TRUE(manager.was_successful());

  // Check that the changes were saved to the RenderFrameHost's feature
  // overrides.
  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(document_data);
  blink::RuntimeFeatureStateReadContext read_context =
      document_data->runtime_feature_state_read_context();
  EXPECT_EQ(expected_feature_overrides, read_context.GetFeatureOverrides());
}

// Similar to RuntimeFeatureStatePersisted but ensures that even for
// runtime-enabled feature values are cleared across redirects.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RuntimeFeatureStateClearOnRedirect) {
  // Sets up the NavigationRequest where we can begin accessing the
  // RuntimeFeatureStateContext class.
  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  TestNavigationManager redirect_manager(shell()->web_contents(), redirect_url);
  shell()->LoadURL(redirect_url);

  EXPECT_TRUE(redirect_manager.WaitForRequestStart());

  // Set a feature value we expect to be cleared upon redirect.
  blink::RuntimeFeatureStateContext& context =
      NavigationRequest::From(redirect_manager.GetNavigationHandle())
          ->GetMutableRuntimeFeatureStateContext();
  bool is_test_feature_enabled(context.IsTestFeatureEnabled());
  context.SetTestFeatureEnabled(!is_test_feature_enabled);
  EXPECT_FALSE(context.GetFeatureOverrides().empty());

  redirect_manager.ResumeNavigation();
  EXPECT_TRUE(redirect_manager.WaitForResponse());

  // Ensure NavigationRequest's new RuntimeFeatureStateContext is clear
  const blink::RuntimeFeatureStateContext& new_context =
      NavigationRequest::From(redirect_manager.GetNavigationHandle())
          ->GetRuntimeFeatureStateContext();
  EXPECT_TRUE(new_context.GetFeatureOverrides().empty());

  // Continue with the navigation until completion.
  ASSERT_TRUE(redirect_manager.WaitForNavigationFinished());
  EXPECT_TRUE(redirect_manager.was_successful());

  // Ensure that the changes made to the features before redirect do not
  // persist to the read context.
  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(document_data);
  blink::RuntimeFeatureStateReadContext read_context =
      document_data->runtime_feature_state_read_context();
  EXPECT_TRUE(read_context.GetFeatureOverrides().empty());
}

// Ensure that a certificate error results in a committed navigation with
// the appropriate error code on the handle.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifyCertErrorFailure) {
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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifyRendererInitiated) {
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
                              ->GetPrimaryFrameTree()
                              .root();

    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar"));
    EXPECT_TRUE(
        ExecJs(root->child_at(0), "window.history.pushState({}, '', 'bar');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_renderer_initiated());
  }
}

// Ensure that methods on NavigationHandle behave correctly with an iframe that
// navigates to its srcdoc attribute.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifySrcdoc) {
  GURL url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_srcdoc_frame.html"));
  NavigationHandleObserver observer(shell()->web_contents(),
                                    GURL("about:srcdoc"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_error());
  EXPECT_TRUE(observer.last_committed_url().IsAboutSrcdoc());
}

// Ensure that the IsSameDocument() method on NavigationHandle behaves
// correctly.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, VerifySameDocument) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a())"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/foo"));
    EXPECT_TRUE(
        ExecJs(root->child_at(0), "window.history.pushState({}, '', 'foo');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_document());
  }
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar"));
    EXPECT_TRUE(ExecJs(root->child_at(0),
                       "window.history.replaceState({}, '', 'bar');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_document());
  }
  {
    NavigationHandleObserver observer(
        shell()->web_contents(),
        embedded_test_server()->GetURL("a.com", "/bar#frag"));
    EXPECT_TRUE(ExecJs(root->child_at(0), "window.location.replace('#frag');"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_TRUE(observer.is_same_document());
  }

  GURL about_blank_url(url::kAboutBlankURL);
  {
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    EXPECT_TRUE(ExecJs(
        root, "document.body.appendChild(document.createElement('iframe'));"));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_same_document());
    EXPECT_EQ(about_blank_url, observer.last_committed_url());
  }
  {
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), about_blank_url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.is_same_document());
    EXPECT_EQ(about_blank_url, observer.last_committed_url());
  }
}

// Ensure that a NavigationThrottle can cancel the navigation at navigation
// start.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleCancelStart) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());

  // The navigation should have been canceled before being redirected.
  EXPECT_FALSE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can cancel the navigation when a navigation
// is redirected.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleCancelRedirect) {
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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

    EXPECT_TRUE(NavigateToURL(shell(), no_redirect_url));

    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), no_redirect_url);
  }
}

// Ensure that a NavigationThrottle can respond CANCEL when a navigation fails.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleCancelFailure) {
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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

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
    url = embedded_test_server()->GetURL("/title2.html");
    NavigationHandleObserver observer(shell()->web_contents(), url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::CANCEL,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_FALSE(installer.will_fail_called());
    EXPECT_FALSE(observer.is_error());
  }
}

// Ensure that a NavigationThrottle can cancel the navigation when the response
// is received.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleCancelResponse) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::CANCEL, NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  // The navigation should have been redirected first, and then canceled when
  // the response arrived.
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can cancel the navigation when committing
// without a URLLoader.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       ThrottleCancelCommitWithoutUrlLoader) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL about_blank_url(url::kAboutBlankURL);
  NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::CANCEL_AND_IGNORE);

  EXPECT_FALSE(NavigateToURL(shell(), about_blank_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_FALSE(observer.was_redirected());
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), start_url);
}

// Ensure that a NavigationThrottle can defer and resume the navigation at
// navigation start, navigation redirect and response received.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleDefer) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/title2.html"));
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER);

  shell()->LoadURL(redirect_url);

  // Wait for WillStartRequest.
  installer.WaitForThrottleWillStart();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for WillRedirectRequest.
  installer.WaitForThrottleWillRedirect();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(1, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for WillProcessResponse.
  installer.WaitForThrottleWillProcess();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(1, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(1, installer.will_process_called());
  EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleDeferFailure) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());
  GURL failure_url(https_server.GetURL("/title1.html"));

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  NavigationHandleObserver observer(shell()->web_contents(), failure_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER,
      NavigationThrottle::DEFER, NavigationThrottle::DEFER);

  shell()->LoadURL(failure_url);

  // Wait for WillStartRequest.
  installer.WaitForThrottleWillStart();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(0, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for WillFailRequest.
  installer.WaitForThrottleWillFail();
  EXPECT_EQ(1, installer.will_start_called());
  EXPECT_EQ(0, installer.will_redirect_called());
  EXPECT_EQ(1, installer.will_fail_called());
  EXPECT_EQ(0, installer.will_process_called());
  EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
  installer.navigation_throttle()->ResumeNavigation();

  // Wait for the end of the navigation.
  navigation_observer.Wait();

  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, observer.net_error_code());
}

// Ensure that a NavigationThrottle can defer and resume the navigation when
// navigating without a URLLoader. This test covers multiple types of
// navigations that do not require a URLLoader (same-document navigations and
// about:blank).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       ThrottleDeferCommitWithoutUrlLoader) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_fragment(embedded_test_server()->GetURL("/title1.html#id_1"));
  GURL about_blank_url(url::kAboutBlankURL);

  // Perform a new-document navigation (setup).
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
    EXPECT_FALSE(observer.is_same_document());
  }

  // Same-document navigation
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER);

    shell()->LoadURL(url_fragment);

    // Wait for WillCommitWithoutUrlLoader.
    installer.WaitForThrottleWillCommitWithoutUrlLoader();
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(0, installer.will_fail_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());
    installer.navigation_throttle()->ResumeNavigation();

    // Wait for the end of the navigation.
    navigation_observer.Wait();

    EXPECT_TRUE(observer.is_same_document());
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_fragment);
  }

  // about:blank
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER);

    shell()->LoadURL(about_blank_url);

    // Wait for WillCommitWithoutUrlLoader.
    installer.WaitForThrottleWillCommitWithoutUrlLoader();
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(0, installer.will_fail_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());
    installer.navigation_throttle()->ResumeNavigation();

    // Wait for the end of the navigation.
    navigation_observer.Wait();

    EXPECT_FALSE(observer.is_same_document());
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_FALSE(observer.is_error());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), about_blank_url);
  }
}

// Ensure that a NavigationThrottle can defer and cancel the navigation when
// navigating without a URLLoader. This test covers multiple types of
// navigations that do not require a URLLoader (same-document navigations and
// about:blank).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       ThrottleDeferAndCancelCommitWithoutUrlLoader) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_fragment(embedded_test_server()->GetURL("/title1.html#id_1"));
  GURL about_blank_url(url::kAboutBlankURL);

  // Perform a new-document navigation (setup).
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
    EXPECT_FALSE(observer.is_same_document());
  }

  // Same-document navigation
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER);

    shell()->LoadURL(url_fragment);

    // Wait for WillCommitWithoutUrlLoader.
    installer.WaitForThrottleWillCommitWithoutUrlLoader();
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(0, installer.will_fail_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());

    // Cancel the deferred navigation.
    installer.navigation_throttle()->CancelNavigation(
        NavigationThrottle::CANCEL_AND_IGNORE);

    // Wait for the end of the navigation.
    navigation_observer.Wait();

    EXPECT_TRUE(observer.is_same_document());
    EXPECT_FALSE(observer.has_committed());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_TRUE(observer.is_error());
  }

  // about:blank
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER);

    shell()->LoadURL(about_blank_url);

    // Wait for WillCommitWithoutUrlLoader.
    installer.WaitForThrottleWillCommitWithoutUrlLoader();
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(0, installer.will_fail_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());

    // Cancel the deferred navigation.
    installer.navigation_throttle()->CancelNavigation(
        NavigationThrottle::CANCEL_AND_IGNORE);

    // Wait for the end of the navigation.
    navigation_observer.Wait();

    EXPECT_FALSE(observer.is_same_document());
    EXPECT_FALSE(observer.has_committed());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_TRUE(observer.is_error());
  }
}

// Ensure that a NavigationThrottle can block the navigation and collapse the
// frame owner both on request start as well as after a redirect. Plus, ensure
// that the frame is restored on the subsequent non-error-page navigation.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ThrottleBlockAndCollapse) {
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
      subframe_throttle_installer =
          std::make_unique<TestDeferringNavigationThrottleInstaller>(
              shell()->web_contents(), test_case.will_start_result,
              test_case.will_redirect_result, NavigationThrottle::PROCEED,
              NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
              blocked_subframe_url);
    } else {
      subframe_throttle_installer =
          std::make_unique<TestNavigationThrottleInstaller>(
              shell()->web_contents(), test_case.will_start_result,
              test_case.will_redirect_result, NavigationThrottle::PROCEED,
              NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
              blocked_subframe_url);
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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
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
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      blocked_subframe_url);

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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
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
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
  ASSERT_TRUE(main_manager.WaitForNavigationFinished());
  EXPECT_TRUE(b_manager.WaitForRequestStart());
  EXPECT_EQ(2, installer.install_count());
  EXPECT_EQ(b_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::IFRAME,
            installer.navigation_throttle()->request_context_type());

  // Ditto for frame c navigation.
  ASSERT_TRUE(b_manager.WaitForNavigationFinished());
  EXPECT_TRUE(c_manager.WaitForRequestStart());
  EXPECT_EQ(3, installer.install_count());
  EXPECT_EQ(c_url, url_recorder.urls().back());
  EXPECT_EQ(3ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::IFRAME,
            installer.navigation_throttle()->request_context_type());

  // Lets the final navigation finish so that we conclude running the
  // RequestContextType checks that happen in TestNavigationThrottle.
  ASSERT_TRUE(c_manager.WaitForNavigationFinished());
  // Confirms the last navigation did finish.
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the RequestContextType value is properly set for an hyper-link.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       VerifyHyperlinkRequestContextType) {
  GURL link_url(embedded_test_server()->GetURL("/title2.html"));
  GURL document_url(embedded_test_server()->GetURL("/simple_links.html"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
  TestNavigationManager link_manager(shell()->web_contents(), link_url);
  NavigationStartUrlRecorder url_recorder(shell()->web_contents());

  // Navigate to a page with a link.
  EXPECT_TRUE(NavigateToURL(shell(), document_url));
  EXPECT_EQ(document_url, url_recorder.urls().back());
  EXPECT_EQ(1ul, url_recorder.urls().size());

  // Starts the navigation from a link click and then check it.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(true, EvalJs(shell(), "clickSameSiteLink();"));
  EXPECT_TRUE(link_manager.WaitForRequestStart());
  EXPECT_EQ(link_url, url_recorder.urls().back());
  EXPECT_EQ(2ul, url_recorder.urls().size());
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            installer.navigation_throttle()->request_context_type());

  // Finishes the last navigation.
  ASSERT_TRUE(link_manager.WaitForNavigationFinished());
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the RequestContextType value is properly set for an form (POST).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       VerifyFormRequestContextType) {
  GURL document_url(
      embedded_test_server()->GetURL("/session_history/form.html"));
  GURL post_url(embedded_test_server()->GetURL("/echotitle"));

  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
  ASSERT_TRUE(post_manager.WaitForNavigationFinished());
  EXPECT_FALSE(installer.navigation_throttle());
}

// Checks that the error code is properly set on the NavigationHandle when a
// NavigationThrottle cancels.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
        NavigationThrottle::CANCEL_AND_IGNORE, NavigationThrottle::PROCEED);
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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
  NavigationHandleObserver observer(shell()->web_contents(), kRedirectingUrl);

  // Try to navigate to the url. The navigation should be canceled and the
  // NavigationHandle should have the right error code.
  EXPECT_FALSE(NavigateToURL(shell(), kRedirectingUrl));
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.net_error_code());
}

// Checks that there's no UAF if NavigationRequest::WillStartRequest cancels the
// navigation.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       CancelNavigationInWillStartRequest) {
  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  // First make a successful commit, as this issue only reproduces when there
  // are existing entries (i.e. when NavigationControllerImpl::GetVisibleEntry
  // has safe_to_show_pending=false).
  EXPECT_TRUE(NavigateToURL(shell(), kUrl1));

  // To take the path that doesn't run beforeunload, so that
  // NavigationControllerImpl::NavigateToPendingEntry is on the botttom of the
  // stack when NavigationRequest::WillStartRequest is called.
  CrashTab(shell()->web_contents());

  // Set up a NavigationThrottle that will cancel the navigation in
  // WillStartRequest.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL_AND_IGNORE,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), kUrl2));
}

// Verify that a cross-process navigation in a frame for which the current
// renderer process is not live will not result in leaking a
// RenderProcessHost. See https://crbug.com/949977.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       NoLeakFromStartingSiteInstance) {
  IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                           {"b.com"});

  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // Kill the a.com process, to test what happens with the next navigation.
  scoped_refptr<SiteInstance> site_instance_a =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(site_instance_a->HasProcess());
  RenderProcessHost* process_1 = site_instance_a->GetProcess();
  RenderProcessHostWatcher process_exit_observer_1(
      process_1, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  RenderProcessHostWatcher rph_gone_observer_1(
      process_1, content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  process_1->Shutdown(RESULT_CODE_KILLED);
  process_exit_observer_1.Wait();

  // Start to navigate the sad tab to another site.
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title2.html");
  TestNavigationManager navigation_b(shell()->web_contents(), url_b);
  shell()->web_contents()->GetController().LoadURL(
      url_b, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(navigation_b.WaitForRequestStart());

  // The starting SiteInstance should be the SiteInstance of the current
  // RenderFrameHost.
  scoped_refptr<SiteInstance> starting_site_instance =
      navigation_b.GetNavigationHandle()->GetStartingSiteInstance();
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            starting_site_instance);
  if (ShouldSkipEarlyCommitPendingForCrashedFrame()) {
    EXPECT_EQ(GURL("http://a.com"), starting_site_instance->GetSiteURL());
  } else {
    // Because of the sad tab, this is actually the b.com SiteInstance, which
    // commits immediately after starting the navigation and has a process.
    EXPECT_EQ(GURL("http://b.com"), starting_site_instance->GetSiteURL());
  }
  EXPECT_TRUE(starting_site_instance->HasProcess());

  // In https://crbug.com/949977, we used the a.com SiteInstance here and didn't
  // have a process, and an observer called GetProcess, creating a process. This
  // RPH never went away, even after the SiteInstance was gone. Simulate this
  // by creating a new RPH for site_instance_a directly. Note that the actual
  // process may not get created (only if the spare process is in use), so wait
  // for RPH destruction rather than process exit.
  RenderProcessHost* rph_2 = site_instance_a->GetProcess();
  RenderProcessHostWatcher process_exit_observer_2(
      rph_2, content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  ASSERT_TRUE(navigation_b.WaitForNavigationFinished());

  // Ensure RPH 1 is destroyed, which happens at commit time even before the fix
  // for the bug.
  rph_gone_observer_1.Wait();

  // Navigate to another process. This isn't necessary to trigger the original
  // leak (when the starting SiteInstance was a.com), but it lets the test
  // finish in the case that the starting SiteInstance is b.com, since b.com's
  // process goes away with this navigation.
  // TODO(creis): There's still a slight risk that other buggy code could find
  // site_instance_a and call GetProcess() on it, causing a leak. We'll add a
  // backup fix and test for that in a followup CL.
  GURL url_c = embedded_test_server()->GetURL("c.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), url_c));

  // Remove all references to site_instance_a so that we can be sure its process
  // gets cleaned up. Pruning the NavigationEntry for url_a on the tab simulates
  // closing the tab (from that SiteInstance's perspective).
  site_instance_a = nullptr;
  starting_site_instance = nullptr;
  shell()->web_contents()->GetController().PruneAllButLastCommitted();

  // Wait for rph_2 to exit when it's not used. This wouldn't happen when the
  // bug was present.
  process_exit_observer_2.Wait();
}

// Specialized test that verifies the NavigationHandle gets the HTTPS upgraded
// URL from the very beginning of the navigation.
class NavigationRequestHttpsUpgradeBrowserTest
    : public NavigationRequestBrowserTest {
 public:
  void CheckHttpsUpgradedIframeNavigation(const GURL& start_url,
                                          const GURL& iframe_secure_url) {
    ASSERT_TRUE(start_url.SchemeIs(url::kHttpScheme));
    ASSERT_TRUE(iframe_secure_url.SchemeIs(url::kHttpsScheme));

    NavigationStartUrlRecorder url_recorder(shell()->web_contents());
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
IN_PROC_BROWSER_TEST_F(NavigationRequestHttpsUpgradeBrowserTest,
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
IN_PROC_BROWSER_TEST_F(NavigationRequestHttpsUpgradeBrowserTest,
                       StartUrlIsHttpsUpgradedCrossSite) {
  GURL start_url(
      embedded_test_server()->GetURL("/https_upgrade_cross_site.html"));
  GURL cross_site_iframe_secure_url("https://other.com/title1.html");

  CheckHttpsUpgradedIframeNavigation(start_url, cross_site_iframe_secure_url);
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       BrowserInitiatedMainFrameReload) {
  GURL url = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  NavigationHandleObserver handle_observer(shell()->web_contents(), url);
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->Reload();
  navigation_observer.Wait();

  EXPECT_EQ(handle_observer.reload_type(), ReloadType::NORMAL);
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       BrowserInitiatedSubFrameReload) {
  GURL url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  NavigationHandleObserver handle_observer(
      shell()->web_contents(), embedded_test_server()->GetURL("/title1.html"));
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_EQ(1u, main_frame->child_count());
  FrameTreeNode* child = main_frame->child_at(0u);
  auto* navigation_controller = static_cast<NavigationControllerImpl*>(
      &shell()->web_contents()->GetController());
  navigation_controller->ReloadFrame(child);
  navigation_observer.Wait();

  EXPECT_EQ(handle_observer.reload_type(), ReloadType::NORMAL);
  EXPECT_FALSE(handle_observer.is_main_frame());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RendererInitiatedMainFrameReload) {
  GURL url = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(ExecJs(shell(), "location.reload();"));
  EXPECT_EQ(observer.reload_type(), ReloadType::NORMAL);
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RendererInitiatedSubFrameReload) {
  GURL url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  NavigationHandleObserver handle_observer(
      shell()->web_contents(), embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(ExecJs(shell(),
                     "document.getElementById('test_iframe')."
                     "contentWindow.location.reload();"));
  EXPECT_EQ(handle_observer.reload_type(), ReloadType::NORMAL);
  EXPECT_FALSE(handle_observer.is_main_frame());
}

// Ensure that browser-initiated same-document navigations are detected and
// don't issue network requests.  See crbug.com/663777.
// Browser-initiated same-document navigations should trigger a
// WillCommitWithoutUrlLoader() callback, instead of the WillStartRequest()
// and WillProcessResponse() callbacks used when there is a network request.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       SameDocumentBrowserInitiatedNoReload) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_fragment_1(embedded_test_server()->GetURL("/title1.html#id_1"));
  GURL url_fragment_2(embedded_test_server()->GetURL("/title1.html#id_2"));

  // 1) Perform a new-document navigation.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
    EXPECT_FALSE(observer.is_same_document());
  }

  // 2) Perform a same-document navigation by adding a fragment.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment_1);
    EXPECT_TRUE(NavigateToURL(shell(), url_fragment_1));
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());
    EXPECT_TRUE(observer.is_same_document());
  }

  // 3) Perform a same-document navigation by modifying the fragment.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment_2);
    EXPECT_TRUE(NavigateToURL(shell(), url_fragment_2));
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());
    EXPECT_TRUE(observer.is_same_document());
  }

  // 4) Redo the last navigation, but this time it should trigger a reload.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url_fragment_2);
    EXPECT_TRUE(NavigateToURL(shell(), url_fragment_2));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
    EXPECT_FALSE(observer.is_same_document());
  }

  // 5) Perform a new-document navigation by removing the fragment.
  {
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
    NavigationHandleObserver observer(shell()->web_contents(), url);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(1, installer.will_start_called());
    EXPECT_EQ(1, installer.will_process_called());
    EXPECT_EQ(0, installer.will_commit_without_url_loader_called());
    EXPECT_FALSE(observer.is_same_document());
  }
}

class NavigationRequestHostResolutionFailureTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddSimulatedTimeoutFailure("*");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(NavigationRequestHostResolutionFailureTest,
                       HostResolutionFailure) {
  GURL url(embedded_test_server()->GetURL("example.com", "/title1.html"));

  NavigationHandleObserver observer(shell()->web_contents(), url);

  EXPECT_FALSE(NavigateToURL(shell(), url));

  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, observer.net_error_code());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.resolve_error_info().error);
}

// Record and list the navigations that are started and finished.
class NavigationLogger : public WebContentsObserver {
 public:
  explicit NavigationLogger(WebContents* web_contents)
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

// Verifies that when a navigation is blocked after a redirect, the renderer
// doesn't try to commit an error page to the pre-redirect URL. This would cause
// a NavigationHandle mismatch and a new NavigationHandle creation to commit
// the error page. This test makes sure that only one NavigationHandle is used
// for committing the error page. See https://crbug.com/695421
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, BlockedOnRedirect) {
  const GURL kUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kRedirectingUrl =
      embedded_test_server()->GetURL("/server-redirect?" + kUrl.spec());

  // Set up a NavigationThrottle that will block the navigation in
  // WillRedirectRequest.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::BLOCK_REQUEST, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ErrorCodeOnCancel) {
  GURL slow_url = embedded_test_server()->GetURL("/slow?60");
  NavigationHandleObserver observer(shell()->web_contents(), slow_url);
  shell()->LoadURL(slow_url);

  GURL url2(embedded_test_server()->GetURL("/title1.html"));
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  shell()->LoadURL(url2);
  same_tab_observer.Wait();

  EXPECT_EQ(net::ERR_ABORTED, observer.net_error_code());
}

// Tests that when a renderer-initiated request redirects to a URL that the
// renderer can't access, the right error code is set on the NavigationHandle.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ErrorCodeOnRedirect) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  GURL redirect_url =
      embedded_test_server()->GetURL(std::string("/server-redirect?") +
                                     blink::kChromeUINetworkErrorsListingURL);
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), base::StringPrintf("location.href = '%s';",
                                                 redirect_url.spec().c_str())));
  same_tab_observer.Wait();
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, observer.net_error_code());
}

// Test to verify that error pages caused by NavigationThrottle blocking a
// request in the main frame from being made are properly committed in a
// separate error page process.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       ErrorPageBlockedNavigation) {
  GURL start_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL blocked_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));

  {
    NavigationHandleObserver observer(shell()->web_contents(), start_url);
    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
  }

  scoped_refptr<SiteInstance> site_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  auto installer = std::make_unique<TestNavigationThrottleInstaller>(
      shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  {
    // A blocked, renderer-initiated navigation in the main frame should commit
    // an error page in a new process.
    NavigationHandleObserver observer(shell()->web_contents(), blocked_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(
        ExecJs(shell(), base::StringPrintf("location.href = '%s'",
                                           blocked_url.spec().c_str())));
    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_NE(
          site_instance,
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
    } else {
      EXPECT_EQ(
          site_instance,
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
    }
  }

  {
    // Reloading the blocked document from the browser process still ends up
    // in the error page process.
    int process_id =
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
    NavigationHandleObserver observer(shell()->web_contents(), blocked_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

    shell()->Reload();
    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
      EXPECT_EQ(process_id, shell()
                                ->web_contents()
                                ->GetPrimaryMainFrame()
                                ->GetProcess()
                                ->GetID());
    } else if (AreAllSitesIsolatedForTesting()) {
      EXPECT_NE(
          site_instance,
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
    } else {
      EXPECT_EQ(
          site_instance,
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
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
    EXPECT_EQ(
        site_instance,
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  }

  installer = std::make_unique<TestNavigationThrottleInstaller>(
      shell()->web_contents(), NavigationThrottle::BLOCK_REQUEST,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  {
    // A blocked, browser-initiated navigation should commit an error page in a
    // different process.
    NavigationHandleObserver observer(shell()->web_contents(), blocked_url);
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

    EXPECT_FALSE(NavigateToURL(shell(), blocked_url));

    navigation_observer.Wait();
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_NE(
        site_instance,
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
    }
  }

  installer.reset();

  {
    // A blocked subframe navigation should commit an error page in the error
    // page process or stay in the same process, based on the isolation policy.
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
        NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

    content::RenderFrameHost* rfh =
        shell()->web_contents()->GetPrimaryMainFrame();
    scoped_refptr<SiteInstance> initial_site_instance = rfh->GetSiteInstance();
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(content::ExecJs(rfh, javascript));
    navigation_observer.Wait();

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    ASSERT_EQ(1u, root->child_count());
    FrameTreeNode* child = root->child_at(0u);

    EXPECT_TRUE(IsExpectedSubframeErrorTransition(
        initial_site_instance.get(),
        child->current_frame_host()->GetSiteInstance()));
  }
}

// Test to verify that error pages caused by network error or other
// recoverable error are properly committed in the process for the
// destination URL.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, ErrorPageNetworkError) {
  GURL start_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/close-socket"));
  EXPECT_NE(start_url.host(), error_url.host());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));

  {
    NavigationHandleObserver observer(shell()->web_contents(), start_url);
    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.is_error());
  }

  scoped_refptr<SiteInstance> site_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  {
    NavigationHandleObserver observer(shell()->web_contents(), error_url);
    EXPECT_FALSE(NavigateToURL(shell(), error_url));
    EXPECT_TRUE(observer.has_committed());
    EXPECT_TRUE(observer.is_error());
    EXPECT_NE(
        site_instance,
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(kUnreachableWebDataURL, shell()
                                            ->web_contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetSiteInstance()
                                            ->GetSiteURL());
    }
  }
}

class ReadyToCommitObserver : public WebContentsObserver {
 public:
  explicit ReadyToCommitObserver(WebContents* web_contents) {
    WebContentsObserver::Observe(web_contents);
  }

  bool ReadyToCommitNavigationWasCalled() const {
    return ready_to_commit_navigation_called_;
  }

 protected:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    ready_to_commit_navigation_called_ = true;
  }

  bool ready_to_commit_navigation_called_ = false;
};

// Ensure that adding a deferring condition that's already satisfied when
// checked (i.e. can return synchronously) doesn't block commit.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       SynchronouslyCompleteCommitDeferringCondition) {
  GURL simple_url(embedded_test_server()->GetURL("/simple_page.html"));

  TestNavigationManager manager(shell()->web_contents(), simple_url);
  WebContents* web_contents = shell()->web_contents();
  ReadyToCommitObserver observer(web_contents);

  MockCommitDeferringConditionInstaller installer(
      simple_url, CommitDeferringCondition::Result::kProceed);

  shell()->LoadURL(simple_url);
  ASSERT_TRUE(manager.WaitForResponse());
  manager.ResumeNavigation();

  // Ready to commit should be reached synchronously after a response.
  EXPECT_TRUE(installer.condition().WasInvoked());
  EXPECT_TRUE(observer.ReadyToCommitNavigationWasCalled());
  EXPECT_TRUE(manager.GetNavigationHandle()->IsWaitingToCommit());

  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

// Ensure asynchronously deferring conditions block the navigation when it's
// ready to commit.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       AsyncCommitDeferringCondition) {
  GURL simple_url(embedded_test_server()->GetURL("/simple_page.html"));

  TestNavigationManager manager(shell()->web_contents(), simple_url);
  WebContents* web_contents = shell()->web_contents();

  MockCommitDeferringConditionInstaller installer1(
      simple_url, CommitDeferringCondition::Result::kDefer);
  MockCommitDeferringConditionInstaller installer2(
      simple_url, CommitDeferringCondition::Result::kDefer);

  ReadyToCommitObserver observer(web_contents);

  shell()->LoadURL(simple_url);
  ASSERT_TRUE(manager.WaitForResponse());
  manager.ResumeNavigation();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(manager.GetNavigationHandle());

  // The navigation should not have proceeded through to ReadyToCommit because
  // the first condition is deferring it. The second condition should not be
  // checked until the first is resolved.
  EXPECT_LT(request->state(), NavigationRequest::READY_TO_COMMIT);
  EXPECT_FALSE(observer.ReadyToCommitNavigationWasCalled());
  EXPECT_TRUE(installer1.condition().WasInvoked());
  EXPECT_FALSE(installer2.condition().WasInvoked());
  EXPECT_TRUE(request->IsCommitDeferringConditionDeferredForTesting());

  // Resume from the first condition. This should now block on the second
  // condition.
  installer1.condition().CallResumeClosure();
  EXPECT_LT(request->state(), NavigationRequest::READY_TO_COMMIT);
  EXPECT_FALSE(observer.ReadyToCommitNavigationWasCalled());
  EXPECT_TRUE(installer2.condition().WasInvoked());

  // Resuming from the second condition should now resume the navigaiton. This
  // should call ReadyToCommit and commit the navigation.
  installer2.condition().CallResumeClosure();
  EXPECT_TRUE(observer.ReadyToCommitNavigationWasCalled());
  EXPECT_EQ(request->state(), NavigationRequest::READY_TO_COMMIT);
  EXPECT_FALSE(request->IsCommitDeferringConditionDeferredForTesting());
  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

// Ensure a navigation can be cancelled while an asynchronously deferring
// condition is blocking commit.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       CancelWhileCommitDeferred) {
  GURL simple_url(embedded_test_server()->GetURL("/simple_page.html"));

  TestNavigationManager manager(shell()->web_contents(), simple_url);
  WebContents* web_contents = shell()->web_contents();

  MockCommitDeferringConditionInstaller installer1(
      simple_url, CommitDeferringCondition::Result::kDefer);

  // We'll cancel the navigation while the first condition is deferred so this
  // is added only to make sure it's never invoked.
  MockCommitDeferringConditionInstaller installer2(
      simple_url, CommitDeferringCondition::Result::kDefer);

  shell()->LoadURL(simple_url);
  ASSERT_TRUE(manager.WaitForResponse());
  manager.ResumeNavigation();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(manager.GetNavigationHandle());

  // The navigation should have passed all checks but is now deferred from
  // committing by |condition|.
  EXPECT_LT(request->state(), NavigationRequest::READY_TO_COMMIT);
  EXPECT_TRUE(installer1.condition().WasInvoked());
  EXPECT_TRUE(request->IsCommitDeferringConditionDeferredForTesting());

  // While the commit is deferred, cancel the navigation. This should delete
  // the navigation request.
  EXPECT_FALSE(installer1.condition().IsDestroyed());
  web_contents->Stop();
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_EQ(manager.GetNavigationHandle(), nullptr);
  EXPECT_TRUE(installer1.condition().IsDestroyed());
  EXPECT_TRUE(installer2.condition().IsDestroyed());

  // Call resume on `installer1`'s condition, as could happen when e.g. the
  // renderer responds after the navigation is stopped. Make sure we don't
  // crash.
  installer1.condition().CallResumeClosure();

  EXPECT_FALSE(installer2.condition().WasInvoked());
}

// Ensure throttles registered by tests using RegisterThrottleForTesting() are
// executed after those registered by the WebContents' browser client (i.e. how
// non-test throttles are normally registered).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RegisterThrottleForTestingIsLast) {
  WebContents* web_contents = shell()->web_contents();
  GURL simple_url(embedded_test_server()->GetURL("/simple_page.html"));

  TestNavigationThrottle* client_throttle = nullptr;

  // Set the client to register a TestNavigationThrottle that defers in
  // WillStartRequest. We'll save a pointer to this throttle in
  // |client_throttle| when its registered.
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&client_throttle](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
            std::unique_ptr<TestNavigationThrottle> throttle(
                new TestNavigationThrottle(
                    handle, NavigationThrottle::DEFER,
                    NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
                    NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
                    base::DoNothing(), base::DoNothing(), base::DoNothing(),
                    base::DoNothing(), base::DoNothing()));
            client_throttle = throttle.get();
            throttles.push_back(std::move(throttle));
            return throttles;
          }));

  // Add another similar throttle using the installer which will use
  // RegisterThrottleForTesting and registers throttles in DidStartNavigation,
  // before browser client throttles are registered.
  TestNavigationThrottleInstaller test_throttle_installer(
      web_contents, NavigationThrottle::DEFER, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED);

  // Start navigating.
  TestNavigationManager manager(shell()->web_contents(), simple_url);
  shell()->LoadURL(simple_url);
  auto* handle = manager.GetNavigationHandle();
  auto* runner =
      NavigationRequest::From(handle)->GetNavigationThrottleRunnerForTesting();

  // The navigation should have been deferred by one of our throttles. Ensure
  // it's the client throttle since we explicitly want test throttles to
  // execute after all others.
  ASSERT_TRUE(handle->IsDeferredForTesting());
  ASSERT_NE(client_throttle, nullptr);
  EXPECT_EQ(runner->GetDeferringThrottle(), client_throttle);

  // Now when we resume we should get deferred by the other throttle. This
  // should be the throttle installed via RegisterThrottleForTesting.
  client_throttle->ResumeNavigation();
  ASSERT_TRUE(handle->IsDeferredForTesting());
  EXPECT_EQ(runner->GetDeferringThrottle(),
            test_throttle_installer.navigation_throttle());

  // Finish the navigation.
  test_throttle_installer.navigation_throttle()->ResumeNavigation();
  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

// Tests the case where a browser-initiated navigation to a normal webpage is
// blocked (net::ERR_BLOCKED_BY_CLIENT) while departing from a privileged WebUI
// page (chrome://gpu). It is a security risk for the error page to commit in
// the privileged process.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, BlockedRequestAfterWebUI) {
  GURL web_ui_url(GetWebUIURL("gpu"));
  WebContents* web_contents = shell()->web_contents();

  // Navigate to the initial page.
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->GetEnabledBindings().Has(
      BindingsPolicyValue::kWebUi));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->GetEnabledBindings().Has(
      BindingsPolicyValue::kWebUi));
  scoped_refptr<SiteInstance> web_ui_process = web_contents->GetSiteInstance();

  // Start a new, non-webUI navigation that will be blocked by a
  // NavigationThrottle.
  GURL blocked_url("http://blocked-by-throttle.example.cc");
  TestNavigationThrottleInstaller installer(
      web_contents, NavigationThrottle::BLOCK_REQUEST,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);
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
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RedirectToRendererDebugUrl) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  const GURL kTestUrls[] = {GURL("javascript:window.alert('hello')"),
                            GURL(blink::kChromeUIBadCastCrashURL),
                            GURL(blink::kChromeUICrashURL),
                            GURL(blink::kChromeUIDumpURL),
                            GURL(blink::kChromeUIKillURL),
                            GURL(blink::kChromeUIHangURL),
                            GURL(blink::kChromeUIShorthangURL),
                            GURL(blink::kChromeUIMemoryExhaustURL)};

  for (const auto& test_url : kTestUrls) {
    SCOPED_TRACE(testing::Message() << "renderer_debug_url = " << test_url);

    GURL redirecting_url =
        embedded_test_server()->GetURL("/server-redirect?" + test_url.spec());

    NavigationHandleObserver observer(shell()->web_contents(), redirecting_url);
    NavigationLogger logger(shell()->web_contents());

    // Try to navigate to the url. The navigation should be canceled and the
    // NavigationHandle should have the right error code.  Note that javascript
    // URLS use ERR_ABORTED rather than ERR_UNSAFE_REDIRECT due to
    // https://crbug.com/941653.
    EXPECT_FALSE(NavigateToURL(shell(), redirecting_url));
    int expected_err_code = test_url.SchemeIs("javascript")
                                ? net::ERR_ABORTED
                                : net::ERR_UNSAFE_REDIRECT;
    EXPECT_EQ(expected_err_code, observer.net_error_code());

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
// TODO(crbug.com/40799853): Enable the test again.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       DISABLED_BlockCredentialedSubresources) {
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
      // credentials should load correctly.
      {GURL("http://user@a.com/frame_tree/page_with_one_frame.html"),
       GURL("http://user@a.com/title1.html"), false},

      // Same usernames and passwords in both frames.
      // Relative URLs on top-level pages that were loaded with embedded
      // credentials should load correctly.
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
  for (const auto& test_case : kTestCases) {
    // Modify the URLs port to use the embedded test server's port.
    std::string port_str(base::NumberToString(embedded_test_server()->port()));
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
          NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

      NavigateIframeToURL(shell()->web_contents(), "child0", iframe_url);

      FrameTreeNode* root =
          static_cast<WebContentsImpl*>(shell()->web_contents())
              ->GetPrimaryFrameTree()
              .root();
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

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest_IsolateAllSites,
                       StartToCommitMetrics) {
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
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/hello.html")));
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    check_navigation(histograms, ProcessType::kSame, FrameType::kMain,
                     TransitionType::kNew);
  }
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
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
    int previous_process_id =
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
    EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
    bool process_changed =
        (previous_process_id !=
         shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());
    check_navigation(histograms,
                     process_changed ? ProcessType::kCross : ProcessType::kSame,
                     FrameType::kMain, TransitionType::kNew);
  }

  // Subframe tests. All of these tests just navigate a frame within
  // page_with_iframe.html.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html")));
  FrameTreeNode* first_child =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
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
    EXPECT_TRUE(ExecJs(first_child, "location.reload();"));
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
    EXPECT_TRUE(ExecJs(first_child, "clickSameSiteLink();"));
    nav_observer.Wait();
    // Link clicking will trigger PAGE_TRANSITION_MANUAL_SUBFRAME which
    // corresponds to NewNavigation.
    check_navigation(histograms, ProcessType::kSame, FrameType::kSub,
                     TransitionType::kNew);
  }
}

// Verify that the TimeToReadyToCommit2 metrics are correctly logged for
// SameProcess vs CrossProcess as well as MainFrame vs Subframe cases.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       TimeToReadyToCommitMetrics) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/hello.html")));

  // Check that only SameProcess version is logged and not CrossProcess.
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    base::HistogramTester::CountsMap expected_counts = {
        {"Navigation.TimeToReadyToCommit2.MainFrame", 1},
        {"Navigation.TimeToReadyToCommit2.MainFrame.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit2.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit2.SameProcess", 1},
        {"Navigation.TimeToReadyToCommit2.SameProcess.NewNavigation", 1}};
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("Navigation.TimeToReadyToCommit2."),
        testing::ContainerEq(expected_counts));
  }

  // Navigate cross-process and ensure that only CrossProcess is logged.
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("a.com", "/title2.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    base::HistogramTester::CountsMap expected_counts = {
        {"Navigation.TimeToReadyToCommit2.MainFrame", 1},
        {"Navigation.TimeToReadyToCommit2.MainFrame.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit2.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit2.CrossProcess", 1},
        {"Navigation.TimeToReadyToCommit2.CrossProcess.NewNavigation", 1}};
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("Navigation.TimeToReadyToCommit2."),
        testing::ContainerEq(expected_counts));
  }

  // Add a new subframe.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(ExecJs(
      root, "document.body.appendChild(document.createElement('iframe'));"));

  // Navigate subframe cross-site and ensure Subframe metrics are logged.
  {
    base::HistogramTester histograms;
    GURL url(embedded_test_server()->GetURL("b.com", "/title3.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));

    std::string navigation_type =
        AreAllSitesIsolatedForTesting() ? "CrossProcess" : "SameProcess";
    base::HistogramTester::CountsMap expected_counts = {
        {"Navigation.TimeToReadyToCommit2.Subframe", 1},
        {"Navigation.TimeToReadyToCommit2.Subframe.NewNavigation", 1},
        {"Navigation.TimeToReadyToCommit2.NewNavigation", 1},
        {base::StringPrintf("Navigation.TimeToReadyToCommit2.%s",
                            navigation_type.c_str()),
         1},
        {base::StringPrintf("Navigation.TimeToReadyToCommit2.%s.NewNavigation",
                            navigation_type.c_str()),
         1}};
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("Navigation.TimeToReadyToCommit2."),
        testing::ContainerEq(expected_counts));
  }
}

IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest, IsDownload) {
  GURL url(embedded_test_server()->GetURL("/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), url));
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       DownloadFalseForHtmlResponse) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       DownloadFalseFor404) {
  GURL url(embedded_test_server()->GetURL("/page404.html"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(observer.has_committed());
  EXPECT_FALSE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       DownloadFalseForFailedNavigation) {
  GURL url(embedded_test_server()->GetURL("/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::CANCEL,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), url));
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());
  EXPECT_FALSE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       RedirectToDownload) {
  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), redirect_url));
  EXPECT_FALSE(observer.has_committed());
  EXPECT_TRUE(observer.was_redirected());
  EXPECT_TRUE(observer.is_download());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       RedirectToDownloadFails) {
  GURL redirect_url(
      embedded_test_server()->GetURL("/cross-site/bar.com/download-test1.lib"));
  NavigationHandleObserver observer(shell()->web_contents(), redirect_url);
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(), NavigationThrottle::PROCEED,
      NavigationThrottle::CANCEL, NavigationThrottle::PROCEED,
      NavigationThrottle::PROCEED, NavigationThrottle::PROCEED);

  EXPECT_FALSE(NavigateToURL(shell(), redirect_url));

  EXPECT_FALSE(observer.has_committed());
  EXPECT_FALSE(observer.is_download());
  EXPECT_TRUE(observer.is_error());
  EXPECT_TRUE(observer.was_redirected());
}

// Set of tests that check the various NavigationThrottle events can be used
// with custom error pages.
class NavigationRequestThrottleResultWithErrorPageBrowserTest
    : public NavigationRequestBrowserTest,
      public ::testing::WithParamInterface<NavigationThrottle::ThrottleAction> {
};

IN_PROC_BROWSER_TEST_P(NavigationRequestThrottleResultWithErrorPageBrowserTest,
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

IN_PROC_BROWSER_TEST_P(NavigationRequestThrottleResultWithErrorPageBrowserTest,
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

IN_PROC_BROWSER_TEST_P(NavigationRequestThrottleResultWithErrorPageBrowserTest,
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

IN_PROC_BROWSER_TEST_P(NavigationRequestThrottleResultWithErrorPageBrowserTest,
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

INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationRequestThrottleResultWithErrorPageBrowserTest,
    testing::Range(NavigationThrottle::ThrottleAction::FIRST,
                   NavigationThrottle::ThrottleAction::LAST));

// The set of tests...
// * NavigationRequestDownloadBrowserTest.AllowedResourceDownloaded
// * NavigationRequestDownloadBrowserTest.AllowedResourceNotDownloaded
// * NavigationRequestDownloadBrowserTest.Disallowed
//
// ...covers every combination of possible states for:
// * CommonNavigationParams::download_policy (allow vs disallow)
// * NavigationHandle::IsDownload()
//
// Download policies that enumerate allowed / disallowed options are not tested
// here.
IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       AllowedResourceDownloaded) {
  GURL simple_url(embedded_test_server()->GetURL("/simple_page.html"));

  TestNavigationManager manager(shell()->web_contents(), simple_url);
  NavigationHandleObserver handle_observer(shell()->web_contents(), simple_url);

  // Download is allowed.
  shell()->LoadURL(simple_url);
  EXPECT_TRUE(manager.WaitForRequestStart());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(root->navigation_request()
                  ->common_params()
                  .download_policy.IsDownloadAllowed());

  // This is not a download (though allowed), so the response should be
  // rendered.
  EXPECT_TRUE(manager.WaitForResponse());
  EXPECT_TRUE(root->navigation_request()->response_should_be_rendered());

  // The response is not handled as a download.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_FALSE(handle_observer.is_download());
}

// See NavigationRequestDownloadBrowserTest.AllowedResourceNotDownloaded
IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest,
                       AllowedResourceNotDownloaded) {
  GURL download_url(embedded_test_server()->GetURL("/download-test1.lib"));

  TestNavigationManager manager(shell()->web_contents(), download_url);
  NavigationHandleObserver handle_observer(shell()->web_contents(),
                                           download_url);

  // Download is allowed.
  shell()->LoadURL(download_url);
  EXPECT_TRUE(manager.WaitForRequestStart());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(root->navigation_request()
                  ->common_params()
                  .download_policy.IsDownloadAllowed());

  // Downloads do not need to be rendered, and should not be rendered.
  EXPECT_TRUE(manager.WaitForResponse());
  EXPECT_FALSE(root->navigation_request()->response_should_be_rendered());

  // The response is handled as a download.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_TRUE(handle_observer.is_download());
}

// See NavigationRequestDownloadBrowserTest.AllowedResourceNotDownloaded
IN_PROC_BROWSER_TEST_F(NavigationRequestDownloadBrowserTest, Disallowed) {
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
                            ->GetPrimaryMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(
      root->navigation_request()->common_params().download_policy.IsType(
          blink::NavigationDownloadType::kViewSource));
  EXPECT_FALSE(root->navigation_request()
                   ->common_params()
                   .download_policy.IsDownloadAllowed());

  // The response is not handled as a download.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_FALSE(handle_observer.is_download());
}

class NavigationRequestBackForwardBrowserTest
    : public NavigationRequestBrowserTest,
      public WebContentsObserver {
 protected:
  void SetUpOnMainThread() override {
    NavigationRequestBrowserTest::SetUpOnMainThread();

    WebContentsObserver::Observe(shell()->web_contents());
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->HasCommitted()) {
      offsets_.push_back(navigation_handle->GetNavigationEntryOffset());
    }
  }

  std::vector<int64_t> offsets_;
};

IN_PROC_BROWSER_TEST_F(NavigationRequestBackForwardBrowserTest,
                       NavigationEntryOffsets) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));
  const GURL url3(embedded_test_server()->GetURL("/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [*url1].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->Reload();
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [*url1].

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [url1, *url2].

  EXPECT_TRUE(NavigateToURL(shell(), url3));
  // The navigation entries are:
  // [url1, url2, *url3].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, *url2, url3].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [*url1, url2, url3].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, *url2, url3].

  // Navigations 1, 3, 4 are regular navigations.
  // Navigation 2 is a reload.
  // Navigaations 5 and 6 are back navigations.
  // Navigation 7 is a forward navigation.
  EXPECT_THAT(offsets_, testing::ElementsAre(1, 0, 1, 1, -1, -1, 1));
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBackForwardBrowserTest,
                       NavigationEntryOffsetsForSubframes) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url1_fragment1(
      embedded_test_server()->GetURL("/title1.html#id_1"));
  const GURL url2(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_one_frame.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [*url1].

  EXPECT_TRUE(NavigateToURL(shell(), url1_fragment1));
  // The navigation entries are:
  // [url1, *url1_fragment1].

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [url1, url1_fragment1, *url2(subframe)].

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url1));
  // The navigation entries are:
  // [url1, url1_fragment1, url2(subframe), *url2(url1)].

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [url1, url1_fragment1, url2(subframe), url2(url1), *url1].

  {
    // We are waiting for two navigations here: main frame and subframe.
    // However, when back/forward cache is enabled, back navigation to a page
    // with subframes will not trigger a subframe navigation (since the
    // subframe is cached with the page).
    TestNavigationObserver navigation_observer(
        shell()->web_contents(), IsBackForwardCacheEnabled() ? 1 : 2);
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, url1_fragment1, url2(subframe), *url2(url1), url1].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, url1_fragment1, *url2(subframe), url2(url1), url1].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, *url1_fragment1, url2(subframe), url2(url1), url1].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [*url1, url1_fragment1, url2(subframe), url2(url1), url1].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(4);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, url1_fragment1, url2(subframe), url2(url1), *url1].

  // New navigations have offset 1, back navigations have offset -1 and the last
  // navigations have offset 3 as requested.
  // Note that all subframe navigations have offset 1 regardless of whether they
  // result in a new entry being generated or not.
  if (IsBackForwardCacheEnabled()) {
    // When back/forward cache is enabled, back navigation to a page with
    // subframes will not trigger a subframe navigation (since the subframe is
    // cached with the page and won't need to be reconstructed/navigated).
    EXPECT_THAT(offsets_,
                testing::ElementsAre(1, 1, 1, 0, 1, 1, -1, -1, -1, -1, 4));
  } else {
    EXPECT_THAT(offsets_,
                testing::ElementsAre(1, 1, 1, 0, 1, 1, -1, 1, -1, -1, -1, 4));
  }
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBackForwardBrowserTest,
                       NavigationEntryLimit) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));

  static_cast<NavigationControllerImpl*>(
      &shell()->web_contents()->GetController())
      ->set_max_entry_count_for_testing(3);

  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(NavigateToURL(shell(), url1));
  }

  // Expect that the offsets are still 1 even when we hit the entry count limit.
  EXPECT_THAT(offsets_, testing::ElementsAre(1, 1, 1, 1, 1));
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBackForwardBrowserTest,
                       LocationReplace) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, "window.location.replace('#frag');"));
    navigation_observer.WaitForNavigationFinished();
  }

  // The second navigation replaces the current navigation entry and should have
  // offset of zero.
  EXPECT_THAT(offsets_, testing::ElementsAre(1, 0));
}

// Tests that the correct net::AuthChallengeInfo is exposed from the
// NavigationHandle when the page requests authentication.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest, AuthChallengeInfo) {
  GURL url(embedded_test_server()->GetURL("/auth-basic"));
  NavigationHandleObserver observer(shell()->web_contents(), url);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(observer.has_committed());
  ASSERT_TRUE(observer.auth_challenge_info().has_value());
  EXPECT_FALSE(observer.auth_challenge_info()->is_proxy);
  EXPECT_EQ(url::SchemeHostPort(url),
            observer.auth_challenge_info()->challenger);
  EXPECT_EQ("basic", observer.auth_challenge_info()->scheme);
  EXPECT_EQ("testrealm", observer.auth_challenge_info()->realm);
  EXPECT_EQ("Basic realm=\"testrealm\"",
            observer.auth_challenge_info()->challenge);
  EXPECT_EQ("/auth-basic", observer.auth_challenge_info()->path);
}

class TestMixedContentWebContentsDelegate : public WebContentsDelegate {
 public:
  TestMixedContentWebContentsDelegate() = default;
  TestMixedContentWebContentsDelegate(
      const TestMixedContentWebContentsDelegate&) = delete;
  TestMixedContentWebContentsDelegate& operator=(
      const TestMixedContentWebContentsDelegate&) = delete;

  bool passive_insecure_content_found() {
    return passive_insecure_content_found_;
  }

  // WebContentsDelegate:
  void PassiveInsecureContentFound(const GURL& resource_url) override {
    passive_insecure_content_found_ = true;
  }

 private:
  bool passive_insecure_content_found_ = false;
};

// Tests that an iframe with a non-webby scheme is not treated as mixed
// content. See https://crbug.com/621131.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       NonWebbyIframeIsNotMixedContent) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL url(https_server.GetURL("/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Inject a test delegate to observe when mixed content is detected.
  WebContents* contents = shell()->web_contents();
  TestMixedContentWebContentsDelegate test_delegate;
  contents->SetDelegate(&test_delegate);

  // Insert an iframe and navigate it to a non-webby scheme. It shouldn't be
  // treated as mixed content.
  GURL non_webby_url("foo://bar");
  TestNavigationObserver observer(contents);
  ASSERT_NE(false,
            EvalJs(contents,
                   JsReplace("var iframe = document.createElement('iframe');"
                             "iframe.src = $1;"
                             "document.body.appendChild(iframe);",
                             non_webby_url)));
  observer.Wait();
  EXPECT_FALSE(test_delegate.passive_insecure_content_found());
}

// Tests that a NavigationRequest's RFH can be retrieved during a synchronous
// renderer commit same-document navigation (regardless of whether the
// navigation commits or not).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       GetRFHDuringSyncRendererCommitSameDocumentNavigation) {
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  const GURL same_doc_url(embedded_test_server()->GetURL("/title1.html#foo"));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  WebContents* web_contents = shell()->web_contents();

  // Test sync-renderer-commit same-document navigation that commits.
  {
    TestNavigationManager navigation_manager(web_contents, same_doc_url);
    testing::NiceMock<MockWebContentsObserver> observer(web_contents);
    EXPECT_CALL(observer, DidFinishNavigation(testing::_))
        .WillOnce(testing::Invoke([](NavigationHandle* navigation_handle) {
          NavigationRequest* request =
              NavigationRequest::From(navigation_handle);
          EXPECT_TRUE(request->is_synchronous_renderer_commit());
          EXPECT_TRUE(navigation_handle->GetRenderFrameHost());
        }));
    EXPECT_TRUE(ExecJs(web_contents, "location.href = '#foo';"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  }

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WebContents* popup = nullptr;
  {
    WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(
        ExecJs(web_contents,
               JsReplace("var w = window.open($1, 'my-popup')", GURL())));
    popup = popup_observer.GetWebContents();
  }
  // Test sync-renderer-commit same-document navigation that doesn't commit.
  {
    testing::NiceMock<MockWebContentsObserver> observer(popup);
    EXPECT_CALL(observer, DidFinishNavigation(testing::_))
        .WillOnce(testing::Invoke([](NavigationHandle* navigation_handle) {
          NavigationRequest* request =
              NavigationRequest::From(navigation_handle);
          EXPECT_TRUE(request->is_synchronous_renderer_commit());
          EXPECT_TRUE(navigation_handle->GetRenderFrameHost());
        }));
    TestNavigationManager navigation_manager(popup, GURL("about:blank#foo"));
    EXPECT_TRUE(
        ExecJs(web_contents, "w.history.replaceState({}, '', '#foo');"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  }
}

// Tests that a NavigationRequest's RFH can be retrieved during a synchronous
// renderer commit initial-about-blank navigation (regardless of whether the
// navigation commits or not).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       GetRFHDuringInitialAboutBlankNavigation) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Test initial-about-blank navigation that commits.
  {
    testing::NiceMock<MockWebContentsObserver> observer(web_contents);
    EXPECT_CALL(observer, DidFinishNavigation(testing::_))
        .WillOnce(testing::Invoke([](NavigationHandle* navigation_handle) {
          NavigationRequest* request =
              NavigationRequest::From(navigation_handle);
          EXPECT_TRUE(request->is_synchronous_renderer_commit());
          EXPECT_TRUE(navigation_handle->GetRenderFrameHost());
        }));
    CreateSubframe(web_contents, "subframe", GURL(),
                   /*wait_for_navigation*/ true);
  }

  WebContentsImpl* popup = nullptr;
  {
    WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(
        ExecJs(web_contents,
               JsReplace("var w = window.open($1, 'my-popup')", GURL())));
    popup = static_cast<WebContentsImpl*>(popup_observer.GetWebContents());
  }
  // Test initial-about-blank navigation that doesn't commit.
  {
    testing::NiceMock<MockWebContentsObserver> observer(popup);
    EXPECT_CALL(observer, DidFinishNavigation(testing::_))
        .WillOnce(testing::Invoke([](NavigationHandle* navigation_handle) {
          NavigationRequest* request =
              NavigationRequest::From(navigation_handle);
          EXPECT_TRUE(request->is_synchronous_renderer_commit());
          EXPECT_TRUE(navigation_handle->GetRenderFrameHost());

          // Ensure that response_should_be_rendered() is true even for pages
          // that do not require a URLLoader.
          EXPECT_TRUE(request->response_should_be_rendered());
        }));
    CreateSubframe(popup, "popup_subframe", GURL(),
                   /*wait_for_navigation*/ true);
  }
}

// Verify that when navigating to a site that doesn't require a dedicated
// process from a initial siteless SiteInstance, the SiteInstance sets its site
// at ready-to-commit time (rather than at DidCommitNavigation time).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       SiteIsSetAtResponseTimeWithoutSiteIsolation) {
  // A custom ContentBrowserClient to turn off strict site isolation.
  class NoSiteIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldEnableStrictSiteIsolation() override { return false; }
  } no_site_isolation_client;

  // The test should start in a blank shell with a siteless SiteInstance.
  EXPECT_FALSE(
      static_cast<SiteInstanceImpl*>(shell()->web_contents()->GetSiteInstance())
          ->HasSite());

  // Start a navigation and wait for response.  Note that this won't require a
  // dedicated process due to the custom ContentBrowserClient.
  GURL main_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  TestNavigationManager manager(shell()->web_contents(), main_url);
  shell()->web_contents()->GetController().LoadURL(
      main_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForResponse());

  // At this point, the navigation should be processing the response but not
  // committed yet. It should have already determined the final
  // RenderFrameHost, which should just be the initial RenderFrameHost.
  NavigationRequest* request =
      static_cast<NavigationRequest*>(manager.GetNavigationHandle());
  EXPECT_EQ(request->state(), NavigationRequest::WILL_PROCESS_RESPONSE);
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame(),
            request->GetRenderFrameHost());

  // The navigation will stay in the initial SiteInstance, and that
  // SiteInstance's site should now be set.
  EXPECT_TRUE(
      static_cast<SiteInstanceImpl*>(shell()->web_contents()->GetSiteInstance())
          ->HasSite());

  // The process should also be considered used at this point.
  EXPECT_FALSE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Ensure the navigation finishes before we restore the ContentBrowserClient
  // which would turn strict site isolation back on.  Otherwise, the navigation
  // commit may fail citadel protection checks at test teardown.
  EXPECT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_TRUE(manager.was_successful());
}

// Check that a subframe can load an error page with an about:srcdoc URL, and
// that the origin does not inherit the parent's origin (i.e., behaves like all
// error pages) in this case.  In practice, this path may be taken by the heavy
// ads intervention (for an example, see the
// HeavyAdInterventionEnabled_ErrorPageLoaded test).
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       OriginForSrcdocErrorPageInSubframe) {
  // Start on a page with a blank subframe.
  GURL start_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Do a srcdoc navigation in the subframe.
  EXPECT_TRUE(
      ExecJs(shell(), "document.querySelector('iframe').srcdoc='foo';"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* subframe_rfh = root->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL("about:srcdoc"), subframe_rfh->GetLastCommittedURL());

  // Navigate the subframe to a post-commit error page, reusing its current
  // srcdoc URL.  A post-commit error page provides a way to reach an error
  // page for a srcdoc subframe; note that it isn't possible to use
  // NavigationThrottles to block srcdoc navigations, since throttles don't
  // currently run in that case.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().LoadPostCommitErrorPage(
      subframe_rfh, subframe_rfh->GetLastCommittedURL(), "error_page_contents");
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());

  // Verify that the origin of the srcdoc frame's parent wasn't inherited and
  // also wasn't used for the precursor.  The error page's origin should be
  // opaque without a valid precursor.
  url::Origin origin =
      root->child_at(0)->current_frame_host()->GetLastCommittedOrigin();
  EXPECT_TRUE(origin.opaque());
  EXPECT_FALSE(origin.GetTupleOrPrecursorTupleIfOpaque().IsValid());
}

// Verify that when navigation 1, which starts in an initial siteless
// SiteInstance and results in an error page, races with navigation 2, which
// requires a dedicated process and wants to reuse an existing process,
// navigation 2 does not incorrectly reuse navigation 1's process.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       ErrorPageMarksProcessAsUsed) {
  // The scenario in this test originally led to a site isolation bypass only
  // when error page isolation for main frames is turned off.  Do this via a
  // custom ContentBrowserClient.
  class NoErrorPageIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldIsolateErrorPage(bool in_main_frame) override { return false; }
  } no_error_isolation_client;

  // Set the process limit to 1.  This will force main frame navigations to
  // attempt to reuse existing processes.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // The test should start in a blank shell with a siteless SiteInstance.
  EXPECT_FALSE(
      static_cast<SiteInstanceImpl*>(shell()->web_contents()->GetSiteInstance())
          ->HasSite());

  // Set up a foo.com URL which will fail to load.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  std::unique_ptr<URLLoaderInterceptor> interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(foo_url,
                                                   net::ERR_CONNECTION_REFUSED);

  // Set up a throttle that will be used to wait for WillFailRequest() and then
  // defer the navigation.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(),
      NavigationThrottle::PROCEED /* will_start_result */,
      NavigationThrottle::PROCEED /* will_redirect_result */,
      NavigationThrottle::DEFER /* will_fail_result */,
      NavigationThrottle::PROCEED /* will_process_result */,
      NavigationThrottle::PROCEED /* will_commit_without_url_loader_result */);

  // Start a navigation to foo.com that will result in an error.
  shell()->web_contents()->GetController().LoadURL(
      foo_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Wait for WillFailRequest(). After this point, we will have picked the
  // final RenderFrameHost for the error page.
  installer.WaitForThrottleWillFail();

  // Create a new tab and navigate it to a different site.  Ensure this site
  // requires a dedicated process, even on Android.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(bar_url)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, bar_url));

  // Resume the error page navigation.  It should be able to finish without
  // crashing.
  installer.navigation_throttle()->ResumeNavigation();
  EXPECT_FALSE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->IsErrorDocument());

  // Ensure that bar.com didn't reuse the foo.com error page process.
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
}

// Check that a renderer-initiated navigation from an error page to about:blank
// honors the initiator origin when selecting the SiteInstance and process for
// about:blank.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       NavigateToAboutBlankFromErrorPage) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(url, net::ERR_DNS_TIMED_OUT);

  // Start off with navigation to a.com, which results in an error page.
  WebContents* web_contents = shell()->web_contents();
  {
    TestNavigationObserver observer(web_contents);
    ASSERT_FALSE(NavigateToURL(shell(), url));
    EXPECT_FALSE(observer.last_navigation_succeeded());
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(true)) {
      EXPECT_EQ(
          GURL(kUnreachableWebDataURL),
          web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
    }
  }

  // Now, do a renderer-initiated navigation to about:blank out of the error
  // page. We don't expect error pages to normally do this, but this might
  // still be possible via DevTools or automation.
  GURL about_blank(url::kAboutBlankURL);
  {
    TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(ExecJs(web_contents, "location = 'about:blank';"));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(about_blank, observer.last_navigation_url());
  }
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
  EXPECT_NE(GURL(kUnreachableWebDataURL), rfh->GetSiteInstance()->GetSiteURL());

  // Note that the error page's origin was opaque with a.com as the precursor.
  // This becomes the initiator origin for the about:blank navigation, and it
  // should end up as the final origin for the blank document.  See
  // https://crbug.com/585649.
  EXPECT_TRUE(rfh->GetLastCommittedOrigin().opaque());
  EXPECT_EQ(
      "a.com",
      rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque().host());

  // Because about:blank's origin is opaque with a.com as the precursor, its
  // SiteInstance and process should also correspond to a.com, rather than be
  // left unassigned/unused.
  //
  // This covers an interesting and rare corner case, where an about:blank
  // navigation can't use the source SiteInstance, which would normally keep it
  // in the initiator's process and SiteInstance.  This is because the
  // navigation originates from an error page process, which is incompatible
  // with a non-error navigation to about:blank.  In this case, a new
  // SiteInstance and process will be created, and they should still reflect
  // about:blank's committed origin, rather than end up in an unlocked process
  // and an unassigned SiteInstance. See https://crbug.com/1426928.
  EXPECT_FALSE(rfh->GetProcess()->IsUnused());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ("http://a.com/", rfh->GetSiteInstance()->GetSiteURL());
    EXPECT_TRUE(rfh->GetProcess()->GetProcessLock().is_locked_to_site());
    EXPECT_EQ("http://a.com/", rfh->GetProcess()->GetProcessLock().site_url());
  } else {
    EXPECT_TRUE(rfh->GetProcess()->GetProcessLock().allows_any_site());
  }
}

using CSPEmbeddedEnforcementBrowserTest = NavigationRequestBrowserTest;

IN_PROC_BROWSER_TEST_F(CSPEmbeddedEnforcementBrowserTest,
                       CheckCSPEmbeddedEnforcement) {
  // We need one initial navigation to set up everything.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "www.example.org", "/empty.html")));

  struct TestCase {
    const char* name;
    const char* required_csp;
    const char* frame_url;
    const char* allow_csp_from;
    const char* returned_csp;
    bool expect_allow;
  } cases[] = {
      {
          "No required csp",
          "",
          "www.not-example.org",
          nullptr,
          nullptr,
          true,
      },
      {
          "Required csp - Same origin",
          "script-src 'none'",
          "www.example.org",
          nullptr,
          nullptr,
          false,
      },
      {
          "Required csp - Cross origin",
          "script-src 'none'",
          "www.not-example.org",
          nullptr,
          nullptr,
          false,
      },
      {
          "Required csp - Cross origin with Allow-CSP-From",
          "script-src 'none'",
          "www.not-example.org",
          "*",
          nullptr,
          true,
      },
      {
          "Required csp - Cross origin with wrong Allow-CSP-From",
          "script-src 'none'",
          "www.not-example.org",
          "www.another-example.org",
          nullptr,
          false,
      },
      {
          "Required csp - Cross origin with non-subsuming CSPs",
          "script-src 'none'",
          "www.not-example.org",
          nullptr,
          "style-src 'none'",
          false,
      },
      {
          "Required csp - Cross origin with subsuming CSPs",
          "script-src 'none'",
          "www.not-example.org",
          nullptr,
          "script-src 'none'",
          true,
      },
      {
          "Required csp - Cross origin with wrong Allow-CSP-From but subsuming "
          "CSPs",
          "script-src 'none'",
          "www.not-example.org",
          "www.another-example.org",
          "script-src 'none'",
          true,
      },
  };

  for (auto test : cases) {
    SCOPED_TRACE(test.name);

    std::string headers;
    if (test.returned_csp) {
      headers +=
          base::StringPrintf("Content-Security-Policy: %s&", test.returned_csp);
    }
    if (test.allow_csp_from) {
      headers += base::StringPrintf("Allow-CSP-From: %s&", test.allow_csp_from);
    }

    GURL frame_url = embedded_test_server()->GetURL(test.frame_url,
                                                    "/set-header?" + headers);
    content::TestNavigationManager observer(shell()->web_contents(), frame_url);

    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace(R"(
      const iframe = document.createElement("iframe");
      iframe.src = $2;
      if ($1)
        iframe.csp = $1;
      document.body.appendChild(iframe);
    )",
                                 test.required_csp, frame_url)));

    ASSERT_TRUE(observer.WaitForNavigationFinished());
    EXPECT_EQ(test.expect_allow, observer.was_successful());
  }
}

class NavigationRequestFencedFrameBrowserTest
    : public NavigationRequestBrowserTest {
 public:
  NavigationRequestFencedFrameBrowserTest() = default;
  ~NavigationRequestFencedFrameBrowserTest() override = default;
  NavigationRequestFencedFrameBrowserTest(
      const NavigationRequestFencedFrameBrowserTest&) = delete;

  NavigationRequestFencedFrameBrowserTest& operator=(
      const NavigationRequestFencedFrameBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(https_server());
    ASSERT_TRUE(https_server()->Start());
    NavigationRequestBrowserTest::SetUpOnMainThread();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(
    NavigationRequestFencedFrameBrowserTest,
    ShouldRespectOutermostFrameCOEPParentAndChildOnInsecureContent) {
  // Navigate |untrustworthy_url| to test if a fenced frame sets the outermost
  // main frame's COEP.
  GURL untrustworthy_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), untrustworthy_url));

  // Create a fenced frame on an insecure content and its document should have
  // the COEP of the outermost main frame.
  GURL fenced_frame_url = embedded_test_server()->GetURL(
      "a.test",
      "/set-header?"
      "Supports-Loading-Mode: fenced-frame&"
      "Cross-Origin-Embedder-Policy: require-corp");
  content::RenderFrameHostImpl* fenced_frame_host =
      static_cast<content::RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              shell()->web_contents()->GetPrimaryMainFrame(),
              fenced_frame_url));
  ASSERT_TRUE(fenced_frame_host);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            fenced_frame_host->cross_origin_embedder_policy().value);
}

IN_PROC_BROWSER_TEST_F(
    NavigationRequestFencedFrameBrowserTest,
    RespectOutermostFrameCOEPParentOnInsecureContentAndChildOnSecureContent) {
  // Navigate |untrustworthy_url| to test if a fenced frame sets the outermost
  // main frame's COEP.
  GURL untrustworthy_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), untrustworthy_url));

  // Create a fenced frame on a secure content and its document should have the
  // COEP of the outermost main frame.
  GURL fenced_frame_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Supports-Loading-Mode: fenced-frame&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  content::RenderFrameHostImpl* fenced_frame_host =
      static_cast<content::RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              shell()->web_contents()->GetPrimaryMainFrame(),
              fenced_frame_url));
  ASSERT_TRUE(fenced_frame_host);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            fenced_frame_host->cross_origin_embedder_policy().value);
}

// Ensure that fenced frames don't enable the view source mode since navigations
// in fenced frames to view-sources URLs are blocked.
IN_PROC_BROWSER_TEST_F(NavigationRequestFencedFrameBrowserTest,
                       ViewSourceNavigation_FencedFrame) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);

  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       fenced_frame_url.spec());
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("Not allowed to load local resource: " +
                              view_source_url.spec());

  // Attempt to navigate to a view source url in the fenced frame.
  EXPECT_EQ(view_source_url.spec(),
            EvalJs(fenced_frame_host,
                   JsReplace(R"({location.href = $1;})", view_source_url)));
  ASSERT_TRUE(console_observer.Wait());

  // Original page shouldn't navigate away.
  EXPECT_EQ(fenced_frame_url, fenced_frame_host->GetLastCommittedURL());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->IsViewSourceMode());
}

class NavigationRequestPrerenderBrowserTest
    : public NavigationRequestBrowserTest {
 public:
  NavigationRequestPrerenderBrowserTest() {
    prerender_helper_ =
        std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
            &NavigationRequestPrerenderBrowserTest::web_contents,
            base::Unretained(this)));
  }
  ~NavigationRequestPrerenderBrowserTest() override = default;

  NavigationRequestPrerenderBrowserTest(
      const NavigationRequestPrerenderBrowserTest&) = delete;
  NavigationRequestPrerenderBrowserTest& operator=(
      const NavigationRequestPrerenderBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    NavigationRequestBrowserTest::SetUpOnMainThread();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    ASSERT_TRUE(https_server()->Start());
  }

 protected:
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  test::PrerenderTestHelper& prerender_helper() { return *prerender_helper_; }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;
};

// Make sure if a main frame page served with a COOP header attempts to navigate
// itself to about:srcdoc that we handle it correctly.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       CoopWithMainframeAboutSrcdocNavigation) {
  std::unique_ptr<net::EmbeddedTestServer> https_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->AddDefaultHandlers(GetTestDataFilePath());
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

  ASSERT_TRUE(https_server->Start());

  GURL url(https_server->GetURL("a.test",
                                "/location_equals_about_srcdoc_script.html"));

  // Navigate to a document that sets COOP and immediately navigates the
  // mainframe to about:srcdoc.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  // Since the redirect to about:srcdoc in the page's script is expected to
  // fail, use EXPECT_FALSE here.
  EXPECT_FALSE(NavigateToURL(shell(), url));
  navigation_observer.Wait();

  // Verify that the second navigation was attempted and failed.
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_INVALID_URL, navigation_observer.last_net_error_code());
  EXPECT_EQ(GURL(url::kAboutSrcdocURL),
            navigation_observer.last_navigation_url());
}

// Same as CoopWithMainframeAboutSrcdocNavigation above, except instead of a
// single, redirected navigation, we get two distinct navigations in this case.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       CoopWithMainframeAboutSrcdocNavigation2) {
  std::unique_ptr<net::EmbeddedTestServer> https_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->AddDefaultHandlers(GetTestDataFilePath());
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

  ASSERT_TRUE(https_server->Start());

  GURL url(https_server->GetURL("a.test",
                                "/set-header?"
                                "cross-origin-opener-policy: same-origin"));

  // Navigate to a document that sets COOP.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  content::RenderFrameHostImpl* main_frame =
      static_cast<content::RenderFrameHostImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin,
            main_frame->cross_origin_opener_policy().value);

  // Navigate main frame to about:srcdoc.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(main_frame, "location = 'about:srcdoc';"));
  navigation_observer.Wait();

  // Verify that the second navigation was attempted and failed.
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_INVALID_URL, navigation_observer.last_net_error_code());
  EXPECT_EQ(GURL(url::kAboutSrcdocURL),
            navigation_observer.last_navigation_url());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestPrerenderBrowserTest,
                       CoopCoepCheckWithPrerender) {
  GURL url(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"
                             "&cross-origin-embedder-policy: require-corp"));

  // Navigate to a document that sets COOP and COEP.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  content::RenderFrameHostImpl* primary_main_frame =
      static_cast<content::RenderFrameHostImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep,
            primary_main_frame->cross_origin_opener_policy().value);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
            primary_main_frame->cross_origin_embedder_policy().value);

  // Add a prerender.
  FrameTreeNodeId host_id = prerender_helper().AddPrerender(
      https_server()->GetURL("a.test", "/title1.html?prerendering"));
  content::RenderFrameHostImpl* prerender_main_frame =
      static_cast<content::RenderFrameHostImpl*>(
          prerender_helper().GetPrerenderedMainFrameHost(host_id));

  // The prerender rfh's polices are none.
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            prerender_main_frame->cross_origin_embedder_policy().value);
  EXPECT_EQ(network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
            prerender_main_frame->cross_origin_opener_policy().value);

  // Prerendering should not affect the primary rfh's polices.
  EXPECT_EQ(network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep,
            primary_main_frame->cross_origin_opener_policy().value);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
            primary_main_frame->cross_origin_embedder_policy().value);
}

enum class TestMPArchType {
  kPrerender,
  kFencedFrame,
};

class NavigationRequestMPArchBrowserTest
    : public NavigationRequestBrowserTest,
      public testing::WithParamInterface<TestMPArchType> {
 public:
  NavigationRequestMPArchBrowserTest() {
    switch (GetParam()) {
      case TestMPArchType::kPrerender:
        prerender_helper_ =
            std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
                &NavigationRequestMPArchBrowserTest::web_contents,
                base::Unretained(this)));
        break;

      case TestMPArchType::kFencedFrame:
        fenced_frame_helper_ =
            std::make_unique<content::test::FencedFrameTestHelper>();
        break;
    }
  }
  ~NavigationRequestMPArchBrowserTest() override = default;
  NavigationRequestMPArchBrowserTest(
      const NavigationRequestMPArchBrowserTest&) = delete;

  NavigationRequestMPArchBrowserTest& operator=(
      const NavigationRequestMPArchBrowserTest&) = delete;

 protected:
  test::PrerenderTestHelper& prerender_helper() { return *prerender_helper_; }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_helper_;
  }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationRequestMPArchBrowserTest,
                         ::testing::Values(TestMPArchType::kPrerender,
                                           TestMPArchType::kFencedFrame));

IN_PROC_BROWSER_TEST_P(NavigationRequestMPArchBrowserTest,
                       ShouldNotUpdateHistory) {
  const auto get_observer = [&](WebContents* web_contents) {
    return DidFinishNavigationObserver(
        web_contents,
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    GetParam() == TestMPArchType::kPrerender
                        ? FrameType::kPrerenderMainFrame
                        : GetParam() == TestMPArchType::kFencedFrame
                              ? FrameType::kFencedFrameRoot
                              : FrameType::kPrimaryMainFrame);
          EXPECT_FALSE(navigation_handle->ShouldUpdateHistory());
        }));
  };

  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kPrimaryMainFrame);
          EXPECT_TRUE(navigation_handle->ShouldUpdateHistory());
        }));

    // Navigate the primary page.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  }
  {
    switch (GetParam()) {
      case TestMPArchType::kPrerender: {
        const auto prerender_observer = get_observer(web_contents());
        // Load a page in the prerender.
        prerender_helper().AddPrerender(
            embedded_test_server()->GetURL("/title1.html?prendering"));
        break;
      }

      case TestMPArchType::kFencedFrame: {
        const auto fenced_frame_observer = get_observer(web_contents());
        // Create a fenced frame.
        ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
            web_contents()->GetPrimaryMainFrame(),
            embedded_test_server()->GetURL("/fenced_frames/title1.html")));
        break;
      }
    }
  }
}

// Tests that when trying to commit an error page for a failed navigation, but
// the renderer process of the error page crashed, the navigation won't commit
// and the browser won't crash.
// Regression test for https://crbug.com/1444360.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       RendererCrashedBeforeCommitErrorPage) {
  // Navigate to `url_a` first.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));

  // Set up an URLLoaderInterceptor which will cause future navigations to fail.
  auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_NOT_IMPLEMENTED;
        params->client->OnComplete(status);
        return true;
      }));

  // Do a navigation to `url_b1` that will fail and commit an error page. This
  // is important so that the next error page navigation won't need to create a
  // speculative RenderFrameHost (unless RenderDocument is enabled) and won't
  // get cancelled earlier than commit time due to speculative RFH deletion.
  GURL url_b1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_FALSE(NavigateToURL(shell(), url_b1));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_b1);
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->IsErrorDocument());

  // For the next navigation, set up a throttle that will be used to wait for
  // WillFailRequest() and then defer the navigation, so that we can crash the
  // error page process first.
  TestNavigationThrottleInstaller installer(
      shell()->web_contents(),
      NavigationThrottle::PROCEED /* will_start_result */,
      NavigationThrottle::PROCEED /* will_redirect_result */,
      NavigationThrottle::DEFER /* will_fail_result */,
      NavigationThrottle::PROCEED /* will_process_result */,
      NavigationThrottle::PROCEED /* will_commit_without_url_loader_result */);

  // Start a navigation to `url_b2` that will also fail, but before it commits
  // an error page, cause the error page process to crash.
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager manager(shell()->web_contents(), url_b2);
  shell()->LoadURL(url_b2);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Resume the navigation and wait for WillFailRequest(). After this point, we
  // will have picked the final RenderFrameHost & RenderProcessHost for the
  // failed navigation.
  manager.ResumeNavigation();
  installer.WaitForThrottleWillFail();

  // Ensure that response_should_be_rendered() is true even for error pages.
  EXPECT_TRUE(static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetPrimaryMainFrame()
                  ->frame_tree_node()
                  ->navigation_request()
                  ->response_should_be_rendered());

  // Kill the error page process. This will cause the navigation to `url_b2` to
  // return early in `NavigationRequest::ReadyToCommitNavigation()` and not
  // commit a new error page.
  RenderProcessHost* process_to_crash =
      manager.GetNavigationHandle()->GetRenderFrameHost()->GetProcess();
  ASSERT_TRUE(process_to_crash->IsInitializedAndNotDead());
  RenderProcessHostWatcher crash_observer(
      process_to_crash, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process_to_crash->Shutdown(0);
  crash_observer.Wait();
  ASSERT_FALSE(process_to_crash->IsInitializedAndNotDead());

  // Resume the navigation, which won't commit.
  if (!ShouldCreateNewHostForAllFrames()) {
    installer.navigation_throttle()->ResumeNavigation();
  }
  EXPECT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_FALSE(WaitForLoadStop(shell()->web_contents()));

  // The tab stayed at `url_b1` as the `url_b2` navigation didn't commit.
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url_b1);
}

namespace {

constexpr char kResponseBody[] = "response-body-contents";

// HTTP response template with adjustable header and body contents.
const char kResponseTemplate[] =
    "HTTP/1.1 200 OK\r\n"
    "%s"
    "\r\n"
    "%s";

// Test version of a NavigationThrottle that requests the response body.
class ResponseBodyNavigationThrottle : public NavigationThrottle {
 public:
  explicit ResponseBodyNavigationThrottle(NavigationHandle* handle)
      : NavigationThrottle(handle) {}
  ResponseBodyNavigationThrottle(const ResponseBodyNavigationThrottle&) =
      delete;
  ResponseBodyNavigationThrottle& operator=(
      const ResponseBodyNavigationThrottle&) = delete;
  ~ResponseBodyNavigationThrottle() override = default;

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    // It is safe to use base::Unretained as the NavigationThrottle will not be
    // destroyed before the callback is called.
    navigation_handle()->GetResponseBody(
        base::BindOnce(&ResponseBodyNavigationThrottle::OnResponseBodyReady,
                       base::Unretained(this)));
    return NavigationThrottle::DEFER;
  }

  const char* GetNameForLogging() override {
    return "ResponseBodyNavigationThrottle";
  }

  bool was_callback_called() const { return was_callback_called_; }

  const std::string& response_body() const { return response_body_; }

 private:
  void OnResponseBodyReady(const std::string& response_body) {
    was_callback_called_ = true;
    response_body_ = response_body;
    Resume();
  }

  bool was_callback_called_ = false;
  std::string response_body_;
};

}  // namespace

class NavigationRequestResponseBodyBrowserTest
    : public NavigationRequestBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(NavigationRequestResponseBodyBrowserTest, Received) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/target.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  ResponseBodyNavigationThrottle* client_throttle = nullptr;

  // Set the client to register a ResponseBodyNavigationThrottle. Save a pointer
  // to this throttle in `client_throttle` on registration.
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&client_throttle](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
            auto throttle =
                std::make_unique<ResponseBodyNavigationThrottle>(handle);
            client_throttle = throttle.get();
            throttles.push_back(std::move(throttle));
            return throttles;
          }));

  // Start navigating.
  GURL simple_url(embedded_test_server()->GetURL("/target.html"));
  TestNavigationManager manager(shell()->web_contents(), simple_url);
  shell()->LoadURL(simple_url);

  EXPECT_TRUE(manager.WaitForRequestStart());
  manager.ResumeNavigation();

  // Build the response with no headers and some body text.
  response.WaitForRequest();
  response.Send(base::StringPrintf(kResponseTemplate, "", kResponseBody));
  response.Done();
  ASSERT_TRUE(manager.WaitForResponse());
  ASSERT_NE(nullptr, client_throttle);
  EXPECT_TRUE(client_throttle->was_callback_called());
  EXPECT_EQ(kResponseBody, client_throttle->response_body());

  // Finish the navigation.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestResponseBodyBrowserTest,
                       ContentLengthZero) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/target.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  ResponseBodyNavigationThrottle* client_throttle = nullptr;

  // Set the client to register a ResponseBodyNavigationThrottle. Save a pointer
  // to this throttle in `client_throttle` on registration.
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&client_throttle](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
            auto throttle =
                std::make_unique<ResponseBodyNavigationThrottle>(handle);
            client_throttle = throttle.get();
            throttles.push_back(std::move(throttle));
            return throttles;
          }));

  // Start navigating.
  GURL simple_url(embedded_test_server()->GetURL("/target.html"));
  TestNavigationManager manager(shell()->web_contents(), simple_url);
  shell()->LoadURL(simple_url);

  EXPECT_TRUE(manager.WaitForRequestStart());
  manager.ResumeNavigation();

  // Build the response with Content-Length: 0 and some body text.
  response.WaitForRequest();
  response.Send(base::StringPrintf(kResponseTemplate, "Content-Length: 0",
                                   kResponseBody));
  response.Done();
  ASSERT_TRUE(manager.WaitForResponse());
  ASSERT_NE(nullptr, client_throttle);
  EXPECT_TRUE(client_throttle->was_callback_called());
  // The received response body is empty due to the Content-Length value.
  EXPECT_EQ(std::string(), client_throttle->response_body());

  // Finish the navigation.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

IN_PROC_BROWSER_TEST_F(NavigationRequestResponseBodyBrowserTest,
                       BodyLargerThanDataPipeSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ResponseBodyNavigationThrottle* client_throttle = nullptr;

  // Set the client to register a ResponseBodyNavigationThrottle. Save a pointer
  // to this throttle in `client_throttle` on registration.
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&client_throttle](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
            auto throttle =
                std::make_unique<ResponseBodyNavigationThrottle>(handle);
            client_throttle = throttle.get();
            throttles.push_back(std::move(throttle));
            return throttles;
          }));

  // Start navigating to a page with a large body (>5 million characters).
  GURL simple_url(embedded_test_server()->GetURL("/long_response_body.html"));
  TestNavigationManager manager(shell()->web_contents(), simple_url);
  shell()->LoadURL(simple_url);

  ASSERT_TRUE(manager.WaitForResponse());
  ASSERT_NE(nullptr, client_throttle);
  EXPECT_TRUE(client_throttle->was_callback_called());
  // Ensure that the received response body contains text from the target page.
  EXPECT_NE(client_throttle->response_body().npos,
            client_throttle->response_body().find(
                "Test page with a long response body"));
  // The initial response body chunk may be smaller than the max data pipe size.
  EXPECT_LE(
      client_throttle->response_body().length(),
      network::features::GetDataPipeDefaultAllocationSize(
          network::features::DataPipeAllocationSize::kLargerSizeIfPossible));

  // Finish the navigation.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

// Helper class to turn off strict site isolation, to allow testing dynamic
// isolated origins added for future BrowsingInstances.
class NavigationRequestNoSiteIsolationBrowserTest
    : public NavigationRequestBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    command_line->RemoveSwitch(switches::kSitePerProcess);
    NavigationRequestBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test the early swap metrics logged when performing the early swap out of the
// initial RenderFrameHost.  Currently, the vast majority of navigations are
// allowed to reuse the initial RFH.  One exception is a navigation to a
// future-isolated origin, which forces a BrowsingInstance swap so that the
// isolation can take effect right away, so this is the case this test
// exercises.
IN_PROC_BROWSER_TEST_F(NavigationRequestNoSiteIsolationBrowserTest,
                       EarlySwapMetrics_InitialFrame) {
  base::HistogramTester histograms;

  EXPECT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());

  // Isolate bar.com in future BrowsingInstances.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(bar_url)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  // Navigate the initial frame to bar.com which requires isolation. Currently,
  // the isolation heuristics force a BrowsingInstance and a RenderFrameHost
  // swap, so the initial RFH cannot be reused for such a URL, and hence we
  // should create a speculative RenderFrameHost and swap it in early because
  // the initial frame is not live.
  ASSERT_FALSE(
      shell()->web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());
  ASSERT_TRUE(NavigateToURL(shell(), bar_url));
  histograms.ExpectUniqueSample(
      "Navigation.EarlyRenderFrameHostSwapType",
      NavigationRequest::EarlyRenderFrameHostSwapType::kInitialFrame, 1);
  histograms.ExpectUniqueSample(
      "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 1, 1);
  histograms.ExpectUniqueSample(
      "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 1, 1);
}

// Test that same-site cross-origin navigations keep user activation even when
// site isolation is disabled.
IN_PROC_BROWSER_TEST_F(NavigationRequestNoSiteIsolationBrowserTest,
                       UserActivationSameSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load cross-origin same-site page into iframe and verify there is still no
  // sticky user activation.
  GURL first_http_url(
      embedded_test_server()->GetURL("subdomain.b.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(child, first_http_url));
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, "// No-op script"));
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform another cross-origin same-site navigation in the iframe.
  GURL second_http_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(child, second_http_url));

  // The cross-origin same-site navigation should keep the sticky user
  // activation from the previous page.
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Ensure that top-level navigations can still happen.
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     JsReplace("window.open($1, $2)", first_http_url, "_top"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(first_http_url, shell()->web_contents()->GetLastCommittedURL());
}

// Test that navigating the outermost main frame to a javascript: url does not
// preserve user activation state.
IN_PROC_BROWSER_TEST_F(NavigationRequestNoSiteIsolationBrowserTest,
                       UserActivationJavascriptUrlMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(root->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(root->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the root frame user activation.
  EXPECT_TRUE(ExecJs(root, "// No-op script"));
  EXPECT_TRUE(root->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(root->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform a javascript URL navigation.
  GURL javascript_url("javascript:'foo'");
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("location.href = $1", javascript_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ("foo", EvalJs(shell()->web_contents(), "document.body.innerText",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // The navigation to the javascript: URL should not keep the sticky user
  // activation from the previous page.
  // TODO(crbug.com/328296079) The browser and renderer's sticky activation
  // state should not be out of sync. Update this test when fixing that bug to
  // check that the browser-side clears sticky user activation.
  EXPECT_TRUE(root->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(root->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Test that navigating an iframe to a javascript: url preserves the user
// activation state.
IN_PROC_BROWSER_TEST_F(NavigationRequestNoSiteIsolationBrowserTest,
                       UserActivationJavascriptUrlChildFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, "// No-op script"));
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform a javascript URL navigation in the iframe.
  GURL javascript_url("javascript:'foo'");
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     JsReplace("location.href = $1", javascript_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ("foo",
            EvalJs(child->current_frame_host(), "document.body.innerText",
                   EXECUTE_SCRIPT_NO_USER_GESTURE));

  // The navigation to the javascript: URL should keep the sticky user
  // activation from the previous page.
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       EarlySwapMetrics_CrashNoCommit) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  {
    base::HistogramTester histograms;
    // Ensure that a normal navigation doesn't log early swap metrics.
    ASSERT_TRUE(NavigateToURL(shell(), url_a));
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwapType",
        NavigationRequest::EarlyRenderFrameHostSwapType::kNone, 1);
    histograms.ExpectTotalCount(
        "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 0);
    histograms.ExpectTotalCount(
        "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 0);
  }

  // Crash the main frame.
  RenderProcessHost* process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  ASSERT_FALSE(
      shell()->web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());

  {
    base::HistogramTester histograms;

    // Load a page that results in a 204 error.  This should result in an early
    // RFH swap that leaves the new RFH in a blank state.  The HasCommitted
    // early swap metric should be logged as false.
    GURL url_204(embedded_test_server()->GetURL("a.test", "/nocontent"));
    EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), url_204));

    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwapType",
        NavigationRequest::EarlyRenderFrameHostSwapType::kCrashedFrame, 1);
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 0, 1);
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 1, 1);
  }
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       EarlySwapMetrics_CrashedSubframe) {
  // Needed to guarantee that the subframe will be an OOPIF on Android.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* subframe_rfh = root->child_at(0)->current_frame_host();

  // Crash the subframe.
  RenderProcessHost* child_process = subframe_rfh->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  ASSERT_FALSE(subframe_rfh->IsRenderFrameLive());

  {
    base::HistogramTester histograms;

    // Navigate the subframe, which should result in an early RFH swap.
    GURL subframe_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
    TestNavigationObserver load_observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(shell(), JsReplace("frames[0].location = $1", subframe_url)));
    load_observer.Wait();

    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwapType",
        NavigationRequest::EarlyRenderFrameHostSwapType::kCrashedFrame, 1);
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 1, 1);
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 0, 1);
  }
}

IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       EarlySwapMetrics_NoSwapForWebUI) {
  base::HistogramTester histograms;

  // Navigate the initial frame to a WebUI URL. The initial RFH should be reused
  // for such a URL, and hence there should be no early swap recorded in the
  // metrics.
  ASSERT_FALSE(
      shell()->web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());
  GURL web_ui_url(GetWebUIURL("gpu"));
  ASSERT_TRUE(NavigateToURL(shell(), web_ui_url));
  histograms.ExpectUniqueSample(
      "Navigation.EarlyRenderFrameHostSwapType",
      NavigationRequest::EarlyRenderFrameHostSwapType::kNone, 1);
  histograms.ExpectTotalCount(
      "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 0);
  histograms.ExpectTotalCount(
      "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 0);
}

// Check the output of NavigationHandle::SandboxFlagsInitiator() when the
// navigation is initiated from the omnibox. It must be `kNone`.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       InitiatorSandboxFlags_BrowserInitiated) {
  GURL url_initiator = embedded_test_server()->GetURL(
      "a.com", "/set-header?Content-Security-Policy: sandbox allow-scripts");
  ASSERT_TRUE(NavigateToURL(shell(), url_initiator));

  GURL url_target = embedded_test_server()->GetURL("a.com", "/");
  TestNavigationManager manager(shell()->web_contents(), url_target);
  shell()->LoadURL(url_target);
  EXPECT_TRUE(manager.WaitForRequestStart());
  ASSERT_TRUE(manager.GetNavigationHandle());
  EXPECT_EQ(manager.GetNavigationHandle()->SandboxFlagsInitiator(),
            network::mojom::WebSandboxFlags::kNone);
  EXPECT_EQ(manager.GetNavigationHandle()->SandboxFlagsInherited(),
            network::mojom::WebSandboxFlags::kNone);
}

// Check the output of NavigationHandle::SandboxFlagsInitiator() when the
// navigation is initiated by the top-level document. It must match.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       InitiatorSandboxFlags_DocumentInitiated) {
  GURL url_initiator = embedded_test_server()->GetURL(
      "a.com", "/set-header?Content-Security-Policy: sandbox allow-scripts");
  ASSERT_TRUE(NavigateToURL(shell(), url_initiator));

  GURL url_target = embedded_test_server()->GetURL("a.com", "/");
  TestNavigationManager manager(shell()->web_contents(), url_target);
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("location.href = $1", url_target)));
  EXPECT_TRUE(manager.WaitForRequestStart());
  ASSERT_TRUE(manager.GetNavigationHandle());
  EXPECT_EQ(manager.GetNavigationHandle()->SandboxFlagsInitiator(),
            network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
  EXPECT_EQ(manager.GetNavigationHandle()->SandboxFlagsInherited(),
            network::mojom::WebSandboxFlags::kNone);
}

// Check the output of NavigationHandle::SandboxFlagsInitiator() when the
// navigation is initiated by a sandboxed iframe.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       InitiatorSandboxFlags_SandboxedIframe) {
  // Create the parent document:
  GURL parent_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), parent_url));

  // Create the child frame. It has sandbox flags from the
  // Content-Security-Policy and from its iframe.sandbox flags:
  GURL url_initiator = embedded_test_server()->GetURL(
      "a.com",
      "/set-header?Content-Security-Policy: sandbox allow-scripts allow-forms");
  EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.src = $1;
    iframe.sandbox = "allow-scripts allow-popups";
    document.body.appendChild(iframe);
  )",
                                                        url_initiator)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_EQ(1u, main_frame->child_count());

  // Navigate the child frame.
  GURL url_target = embedded_test_server()->GetURL("a.com", "/");
  TestNavigationManager manager(shell()->web_contents(), url_target);
  EXPECT_TRUE(ExecJs(main_frame->child_at(0),
                     JsReplace("location.href = $1", url_target)));
  EXPECT_TRUE(manager.WaitForRequestStart());
  ASSERT_TRUE(manager.GetNavigationHandle());
  EXPECT_EQ(manager.GetNavigationHandle()->SandboxFlagsInitiator(),
            // `allow-script`:
            network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
  EXPECT_EQ(
      manager.GetNavigationHandle()->SandboxFlagsInherited(),
      //`allow-scripts allow-popups`:
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures);
}

// Tests the scenario when a navigation without URLLoader is cancelled and an
// error page is committed using the same NavigationRequest.
// See https://crbug.com/1487944.
IN_PROC_BROWSER_TEST_F(
    NavigationRequestBrowserTest,
    ThrottleDeferAndCancelCommitWithoutUrlLoaderWithErrorPage) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL about_blank_url(url::kAboutBlankURL);

  // Perform a new-document navigation (setup).
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Navigate to about:blank so the NavigationRequest is expected to commit
  // without URL loader.
  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    NavigationHandleObserver observer(shell()->web_contents(), about_blank_url);
    TestNavigationThrottleInstaller installer(
        shell()->web_contents(), NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER,
        NavigationThrottle::DEFER, NavigationThrottle::DEFER);

    shell()->LoadURL(about_blank_url);

    // Wait for WillCommitWithoutUrlLoader.
    installer.WaitForThrottleWillCommitWithoutUrlLoader();
    EXPECT_EQ(0, installer.will_start_called());
    EXPECT_EQ(0, installer.will_redirect_called());
    EXPECT_EQ(0, installer.will_fail_called());
    EXPECT_EQ(0, installer.will_process_called());
    EXPECT_EQ(1, installer.will_commit_without_url_loader_called());

    // Cancel the deferred navigation with `net::ERR_BLOCKED_BY_RESPONSE`, so
    // the NavigationRequest will be used for an error page commit.
    installer.navigation_throttle()->CancelNavigation(
        {NavigationThrottle::CANCEL_AND_IGNORE, net::ERR_BLOCKED_BY_RESPONSE});

    // Wait for the end of the navigation.
    navigation_observer.Wait();

    EXPECT_FALSE(observer.is_same_document());
    EXPECT_TRUE(observer.has_committed());
    EXPECT_FALSE(observer.was_redirected());
    EXPECT_TRUE(observer.is_error());

    EXPECT_TRUE(
        shell()->web_contents()->GetPrimaryMainFrame()->IsErrorDocument());
  }
}

// data: URLs should have opaque origins with nonce that is stable across a
// navigation, which is stored as the tentative origin to commit.
IN_PROC_BROWSER_TEST_F(NavigationRequestBrowserTest,
                       TentativeOriginToCommitIsStable_Data) {
  // Start a navigation to a data URL with an opaque origin.
  const std::string data = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);
  TestNavigationManager navigation_manager(contents(), data_url);
  shell()->LoadURL(data_url);
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());

  // Get a copy of the computed origin at an early stage, both from the
  // NavigationRequest and the UrlInfo, where the values are set and used.
  NavigationRequest* data_request =
      contents()->GetPrimaryFrameTree().root()->navigation_request();
  std::optional<url::Origin> data_tentative_origin_to_commit =
      data_request->GetTentativeOriginAtRequestTime();
  EXPECT_TRUE(data_tentative_origin_to_commit.has_value());
  std::optional<url::Origin> url_info_origin =
      data_request->GetUrlInfo().origin;
  EXPECT_TRUE(url_info_origin.has_value());
  EXPECT_EQ(data_tentative_origin_to_commit, url_info_origin);

  // Verify that the committed origin at the end matches the initial one.
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  url::Origin data_committed_origin =
      contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_EQ(data_tentative_origin_to_commit.value(), data_committed_origin);
  EXPECT_EQ(url_info_origin.value(), data_committed_origin);
}

}  // namespace content
