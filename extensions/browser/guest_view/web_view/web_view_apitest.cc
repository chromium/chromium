// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_apitest.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_histogram_value.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_browser_context.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/display/display_switches.h"

#if defined(USE_AURA)
#include "third_party/blink/public/common/input/web_mouse_event.h"
#endif

using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;

namespace {

const char kEmptyResponsePath[] = "/close-socket";
const char kRedirectResponsePath[] = "/server-redirect";
const char kRedirectResponseFullPath[] = "/guest_redirect.html";
const char kUserAgentRedirectResponsePath[] = "/detect-user-agent";
const char kTestServerPort[] = "testServer.port";
const char kExpectUserAgentPath[] = "/expect-user-agent";

// Handles |request| by serving a redirect response if the |User-Agent| is
// foobar.
static std::unique_ptr<net::test_server::HttpResponse>
UserAgentRedirectResponseHandler(const std::string& path,
                                 const GURL& redirect_target,
                                 const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  auto it = request.headers.find("User-Agent");
  EXPECT_TRUE(it != request.headers.end());
  if (!base::StartsWith("foobar", it->second, base::CompareCase::SENSITIVE))
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

static std::unique_ptr<net::test_server::HttpResponse>
ExpectUserAgentResponseHandler(const std::string& path,
                               const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  auto it = request.headers.find("User-Agent");
  EXPECT_TRUE(it != request.headers.end());
  EXPECT_TRUE(
      base::StartsWith("foobar", it->second, base::CompareCase::SENSITIVE));

  it = request.headers.find("Sec-CH-UA-Platform");
  EXPECT_TRUE(it != request.headers.end());
  EXPECT_EQ("\"\"", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("html/text");
  return std::move(http_response);
}

class WebContentsHiddenObserver : public content::WebContentsObserver {
 public:
  WebContentsHiddenObserver(content::WebContents* web_contents,
                            base::RepeatingClosure hidden_callback)
      : WebContentsObserver(web_contents),
        hidden_callback_(std::move(hidden_callback)),
        hidden_observed_(false) {}

  WebContentsHiddenObserver(const WebContentsHiddenObserver&) = delete;
  WebContentsHiddenObserver& operator=(const WebContentsHiddenObserver&) =
      delete;

  // WebContentsObserver.
  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::HIDDEN) {
      hidden_observed_ = true;
      hidden_callback_.Run();
    }
  }

  bool hidden_observed() { return hidden_observed_; }

 private:
  base::RepeatingClosure hidden_callback_;
  bool hidden_observed_;
};

// Handles |request| by serving a redirect response.
std::unique_ptr<net::test_server::HttpResponse> RedirectResponseHandler(
    const std::string& path,
    const GURL& redirect_target,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

// Handles |request| by serving an empty response.
std::unique_ptr<net::test_server::HttpResponse> EmptyResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(path, request.relative_url,
                       base::CompareCase::SENSITIVE)) {
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::RawHttpResponse("", ""));
  }

  return nullptr;
}

class RenderWidgetHostVisibilityObserver
    : public content::RenderWidgetHostObserver {
 public:
  RenderWidgetHostVisibilityObserver(content::RenderWidgetHost* host,
                                     base::OnceClosure hidden_callback)
      : hidden_callback_(std::move(hidden_callback)) {
    observation_.Observe(host);
  }
  ~RenderWidgetHostVisibilityObserver() override = default;
  RenderWidgetHostVisibilityObserver(
      const RenderWidgetHostVisibilityObserver&) = delete;
  RenderWidgetHostVisibilityObserver& operator=(
      const RenderWidgetHostVisibilityObserver&) = delete;

  bool hidden_observed() const { return hidden_observed_; }

 private:
  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* host,
                                         bool became_visible) override {
    if (!became_visible) {
      hidden_observed_ = true;
      std::move(hidden_callback_).Run();
    }
  }

  void RenderWidgetHostDestroyed(content::RenderWidgetHost* host) override {
    EXPECT_TRUE(observation_.IsObservingSource(host));
    observation_.Reset();
  }

  base::OnceClosure hidden_callback_;
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      observation_{this};
  bool hidden_observed_ = false;
};

}  // namespace

namespace extensions {

WebViewAPITest::WebViewAPITest() = default;

void WebViewAPITest::LaunchApp(const std::string& app_location) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  base::PathService::Get(DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII(app_location.c_str());

  const Extension* extension = extension_system_->LoadApp(test_data_dir);
  ASSERT_TRUE(extension);
  extension_system_->LaunchApp(extension->id());

  ExtensionTestMessageListener launch_listener("LAUNCHED");
  launch_listener.set_failure_message("FAILURE");
  ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

  embedder_web_contents_ = GetFirstAppWindowWebContents();
}

content::WebContents* WebViewAPITest::GetFirstAppWindowWebContents() {
  const AppWindowRegistry::AppWindowList& app_window_list =
      AppWindowRegistry::Get(browser_context_)->app_windows();
  DCHECK_EQ(1u, app_window_list.size());
  return (*app_window_list.begin())->web_contents();
}

void WebViewAPITest::RunTest(const std::string& test_name,
                             const std::string& app_location,
                             bool ad_hoc_framework) {
  LaunchApp(app_location);

  if (ad_hoc_framework) {
    ExtensionTestMessageListener done_listener("TEST_PASSED");
    done_listener.set_failure_message("TEST_FAILED");
    ASSERT_TRUE(
        content::ExecJs(embedder_web_contents_.get(),
                        base::StringPrintf("runTest('%s')", test_name.c_str())))
        << "Unable to start test.";
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  } else {
    ResultCatcher catcher;
    ASSERT_TRUE(
        content::ExecJs(embedder_web_contents_.get(),
                        base::StringPrintf("runTest('%s')", test_name.c_str())))
        << "Unable to start test.";
    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

void WebViewAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  AppShellTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                  "--expose-gc");
}

void WebViewAPITest::SetUpOnMainThread() {
  AppShellTest::SetUpOnMainThread();

  TestGetConfigFunction::set_test_config_state(&test_config_);
}

void WebViewAPITest::StartTestServer(const std::string& app_location) {
  // For serving guest pages.
  if (!embedded_test_server()->InitializeAndListen()) {
    LOG(ERROR) << "Failed to start test server.";
    return;
  }

  test_config_.SetByDottedPath(kTestServerPort, embedded_test_server()->port());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  base::PathService::Get(DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII(app_location.c_str());
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &RedirectResponseHandler, kRedirectResponsePath,
      embedded_test_server()->GetURL(kRedirectResponseFullPath)));

  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&EmptyResponseHandler, kEmptyResponsePath));

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &UserAgentRedirectResponseHandler, kUserAgentRedirectResponsePath,
      embedded_test_server()->GetURL(kRedirectResponseFullPath)));

  net::test_server::RegisterDefaultHandlers(embedded_test_server());

  embedded_test_server()->StartAcceptingConnections();
}

void WebViewAPITest::StopTestServer() {
  if (!embedded_test_server()->ShutdownAndWaitUntilComplete()) {
    LOG(ERROR) << "Failed to shutdown test server.";
  }
}

void WebViewAPITest::TearDownOnMainThread() {
  DesktopController::instance()->CloseAppWindows();
  GetGuestViewManager()->WaitForAllGuestsDeleted();
  TestGetConfigFunction::set_test_config_state(nullptr);

  AppShellTest::TearDownOnMainThread();
}

void WebViewAPITest::SendMessageToEmbedder(const std::string& message) {
  EXPECT_TRUE(content::ExecJs(
      GetEmbedderWebContents(),
      base::StringPrintf("onAppCommand('%s');", message.c_str())));
}

content::WebContents* WebViewAPITest::GetEmbedderWebContents() {
  if (!embedder_web_contents_)
    embedder_web_contents_ = GetFirstAppWindowWebContents();
  return embedder_web_contents_;
}

TestGuestViewManager* WebViewAPITest::GetGuestViewManager() {
  content::BrowserContext* context =
      ShellContentBrowserClient::Get()->GetBrowserContext();
  return factory_.GetOrCreateTestGuestViewManager(
      context, ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
}

void WebViewDPIAPITest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor,
                                  base::StringPrintf("%f", scale()));
  WebViewAPITest::SetUp();
}

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, EmbedderVisibilityChanged) {
  // In this test, we also sanity-check that guest view metrics are being
  // recorded.
  base::HistogramTester histogram_tester;

  LaunchApp("web_view/visibility_changed");

  base::RunLoop run_loop;
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  EXPECT_TRUE(guest_view);
  RenderWidgetHostVisibilityObserver observer(
      guest_view->GetGuestMainFrame()->GetRenderWidgetHost(),
      run_loop.QuitClosure());

  // Handled in web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-embedder");
  if (!observer.hidden_observed())
    run_loop.Run();

  // We should have created a webview, and should not have records for any other
  // guest view type (`ExpectUniqueSample` guarantees both of these).
  histogram_tester.ExpectUniqueSample(
      "GuestView.GuestViewCreated",
      guest_view::GuestViewHistogramValue::kWebView, 1);
}

// Test for http://crbug.com/419611.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, DisplayNoneSetSrc) {
  LaunchApp("web_view/display_none_set_src");
  // Navigate the guest while it's in "display: none" state.
  SendMessageToEmbedder("navigate-guest");
  GetGuestViewManager()->WaitForSingleGuestViewCreated();

  // Now attempt to navigate the guest again.
  SendMessageToEmbedder("navigate-guest");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED");
  // Making the guest visible would trigger loadstop.
  SendMessageToEmbedder("show-guest");
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// This test verifies that hiding the guest triggers visibility change notifications.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, GuestVisibilityChanged) {
  LaunchApp("web_view/visibility_changed");

  base::RunLoop run_loop;
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  EXPECT_TRUE(guest_view);
  RenderWidgetHostVisibilityObserver observer(
      guest_view->GetGuestMainFrame()->GetRenderWidgetHost(),
      run_loop.QuitClosure());

  // Handled in web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-guest");
  if (!observer.hidden_observed())
    run_loop.Run();
}

// This test ensures that closing app window on 'loadcommit' does not crash.
// The test launches an app with guest and closes the window on loadcommit. It
// then launches the app window again. The process is repeated 3 times.
// TODO(http://crbug.com/291278): Re-enable this test
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_CloseOnLoadcommit DISABLED_CloseOnLoadcommit
#else
#define MAYBE_CloseOnLoadcommit CloseOnLoadcommit
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_CloseOnLoadcommit) {
  LaunchApp("web_view/close_on_loadcommit");
  ExtensionTestMessageListener test_done_listener("done-close-on-loadcommit");
  ASSERT_TRUE(test_done_listener.WaitUntilSatisfied());
}

// This test verifies that reloading the embedder reloads the guest (and doest
// not crash).
IN_PROC_BROWSER_TEST_F(WebViewAPITest, ReloadEmbedder) {
  // Just load a guest from other test, we do not want to add a separate
  // app for this test.
  LaunchApp("web_view/visibility_changed");

  ExtensionTestMessageListener launched_again_listener("LAUNCHED");
  embedder_web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                                 false);
  ASSERT_TRUE(launched_again_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAllowTransparencyAttribute) {
  RunTest("testAllowTransparencyAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAPIMethodExistence) {
  RunTest("testAPIMethodExistence", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestCustomElementCallbacksInaccessible) {
  RunTest("testCustomElementCallbacksInaccessible", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAssignSrcAfterCrash) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  RunTest("testAssignSrcAfterCrash", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeAfterNavigation) {
  RunTest("testAutosizeAfterNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeBeforeNavigation) {
  RunTest("testAutosizeBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewDPIAPITest, TestAutosizeBeforeNavigation) {
  RunTest("testAutosizeBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeHeight) {
  RunTest("testAutosizeHeight", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewDPIAPITest, TestAutosizeHeight) {
  RunTest("testAutosizeHeight", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeRemoveAttributes) {
  RunTest("testAutosizeRemoveAttributes", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewDPIAPITest, TestAutosizeRemoveAttributes) {
  RunTest("testAutosizeRemoveAttributes", "web_view/apitest");
}

// http://crbug.com/473177
IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       DISABLED_TestAutosizeWithPartialAttributes) {
  RunTest("testAutosizeWithPartialAttributes", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestCannotMutateEventName) {
  RunTest("testCannotMutateEventName", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestChromeExtensionRelativePath) {
  RunTest("testChromeExtensionRelativePath", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestChromeExtensionURL) {
  RunTest("testChromeExtensionURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestContentLoadEvent) {
  RunTest("testContentLoadEvent", "web_view/apitest");
}

#if defined(USE_AURA)
// Verifies that trying to show the context menu doesn't crash
// (https://crbug.com/820604).
IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestContextMenu) {
  // Launch some test app that displays a webview.
  LaunchApp("web_view/visibility_changed");

  // Ensure the webview's surface is ready for hit testing.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);

  auto* guest_render_frame_host = guest_view->GetGuestMainFrame();
  content::WaitForHitTestData(guest_render_frame_host);

  // Create a ContextMenuInterceptor to intercept the ShowContextMenu event
  // before RenderFrameHost receives.
  auto context_menu_interceptor =
      std::make_unique<content::ContextMenuInterceptor>(
          guest_render_frame_host);

  // Trigger the context menu. AppShell doesn't show a context menu; this is
  // just a sanity check that nothing breaks.
  content::WebContents* embedder_web_contents = GetEmbedderWebContents();

  content::RenderWidgetHostView* guest_rwhv =
      guest_render_frame_host->GetRenderWidgetHost()->GetView();
  gfx::Point guest_context_menu_position(5, 5);
  gfx::Point root_context_menu_position =
      guest_rwhv->TransformPointToRootCoordSpace(guest_context_menu_position);
  content::SimulateMouseClickAt(
      embedder_web_contents, blink::WebInputEvent::kNoModifiers,
      blink::WebMouseEvent::Button::kRight, root_context_menu_position);
  context_menu_interceptor->Wait();
}
#endif

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDeclarativeWebRequestAPI) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testDeclarativeWebRequestAPI", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestDeclarativeWebRequestAPISendMessage) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testDeclarativeWebRequestAPISendMessage", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDestroyOnEventListener) {
  RunTest("testDestroyOnEventListener", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogAlert) {
  RunTest("testDialogAlert", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirm) {
  RunTest("testDialogConfirm", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirmCancel) {
  RunTest("testDialogConfirmCancel", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirmDefaultCancel) {
  RunTest("testDialogConfirmDefaultCancel", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirmDefaultGCCancel) {
  RunTest("testDialogConfirmDefaultGCCancel", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogPrompt) {
  RunTest("testDialogPrompt", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDisplayNoneWebviewLoad) {
  RunTest("testDisplayNoneWebviewLoad", "web_view/apitest");
}

// Flaky. See http://crbug.com/769467.
IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       DISABLED_TestDisplayNoneWebviewRemoveChild) {
  RunTest("testDisplayNoneWebviewRemoveChild", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestEventName) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  RunTest("testEventName", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestExecuteScript) {
  RunTest("testExecuteScript", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestExecuteScriptFail) {
  RunTest("testExecuteScriptFail", "web_view/apitest");
}

// Flaky and likely not testing the right assertion.  https://crbug.com/702918
IN_PROC_BROWSER_TEST_F(
    WebViewAPITest,
    DISABLED_TestExecuteScriptIsAbortedWhenWebViewSourceIsChanged) {
  RunTest("testExecuteScriptIsAbortedWhenWebViewSourceIsChanged",
          "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestFindAPI) {
  RunTest("testFindAPI", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestFindAPI_findupdate) {
  RunTest("testFindAPI_findupdate", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestGetProcessId) {
  RunTest("testGetProcessId", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestHiddenBeforeNavigation) {
  RunTest("testHiddenBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestInlineScriptFromAccessibleResources) {
  RunTest("testInlineScriptFromAccessibleResources", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestInvalidChromeExtensionURL) {
  RunTest("testInvalidChromeExtensionURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestLoadAbortChromeExtensionURLWrongPartition) {
  RunTest("testLoadAbortChromeExtensionURLWrongPartition", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortEmptyResponse) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadAbortEmptyResponse", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortIllegalChromeURL) {
  RunTest("testLoadAbortIllegalChromeURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortIllegalFileURL) {
  RunTest("testLoadAbortIllegalFileURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortIllegalJavaScriptURL) {
  RunTest("testLoadAbortIllegalJavaScriptURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortInvalidNavigation) {
  RunTest("testLoadAbortInvalidNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortNonWebSafeScheme) {
  RunTest("testLoadAbortNonWebSafeScheme", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortUnknownScheme) {
  RunTest("testLoadAbortUnknownScheme", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadProgressEvent) {
  RunTest("testLoadProgressEvent", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestCanGoBack) {
  RunTest("testCanGoBack", "web_view/apitest");
}

// Crashes on Win only.  http://crbug.com/805903
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestLoadStartLoadRedirect DISABLED_TestLoadStartLoadRedirect
#else
#define MAYBE_TestLoadStartLoadRedirect TestLoadStartLoadRedirect
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestLoadStartLoadRedirect) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadStartLoadRedirect", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavigateAfterResize) {
  RunTest("testNavigateAfterResize", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestContentInitiatedNavigationToDataUrlBlocked) {
  RunTest("testContentInitiatedNavigationToDataUrlBlocked", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestNavOnConsecutiveSrcAttributeChanges) {
  RunTest("testNavOnConsecutiveSrcAttributeChanges", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavOnSrcAttributeChange) {
  RunTest("testNavOnSrcAttributeChange", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadCommitUrlsWithIframe) {
  const std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadCommitUrlsWithIframe", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindow) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindow", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowNoPreventDefault) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindowNoPreventDefault", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowNoReferrerLink) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindowNoReferrerLink", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowTwoListeners) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindowTwoListeners", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestOnEventProperty) {
  RunTest("testOnEventProperties", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestPartitionChangeAfterNavigation) {
  RunTest("testPartitionChangeAfterNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestPartitionRemovalAfterNavigationFails) {
  RunTest("testPartitionRemovalAfterNavigationFails", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReassignSrcAttribute) {
  RunTest("testReassignSrcAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveWebviewOnExit) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);

  // Launch the app and wait until it's ready to load a test.
  LaunchApp("web_view/apitest");

  // Run the test and wait until the guest is available and has finished
  // loading.
  ExtensionTestMessageListener guest_loaded_listener("guest-loaded");
  EXPECT_TRUE(content::ExecJs(embedder_web_contents_.get(),
                              "runTest('testRemoveWebviewOnExit')"));

  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  EXPECT_TRUE(guest_view);
  EXPECT_TRUE(guest_view->GetGuestMainFrame()->GetProcess()->IsForGuestsOnly());
  ASSERT_TRUE(guest_loaded_listener.WaitUntilSatisfied());

  // Tell the embedder to kill the guest.
  EXPECT_TRUE(content::ExecJs(embedder_web_contents_.get(),
                              "removeWebviewOnExitDoCrash()"));

  // Wait until the guest is destroyed.
  GetGuestViewManager()->WaitForLastGuestDeleted();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReload) {
  RunTest("testReload", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReloadAfterTerminate) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  RunTest("testReloadAfterTerminate", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveSrcAttribute) {
  RunTest("testRemoveSrcAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveWebviewAfterNavigation) {
  RunTest("testRemoveWebviewAfterNavigation", "web_view/apitest");
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_TestResizeWebviewResizesContent \
  DISABLED_TestResizeWebviewResizesContent
#else
#define MAYBE_TestResizeWebviewResizesContent TestResizeWebviewResizesContent
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestResizeWebviewResizesContent) {
  RunTest("testResizeWebviewResizesContent", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestTerminateAfterExit) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  RunTest("testTerminateAfterExit", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPI) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testWebRequestAPI", app_location);
  StopTestServer();
}

// Crashes on Win only.  http://crbug.com/805903
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestWebRequestAPIWithHeaders DISABLED_TestWebRequestAPIWithHeaders
#else
#define MAYBE_TestWebRequestAPIWithHeaders TestWebRequestAPIWithHeaders
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestWebRequestAPIWithHeaders) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testWebRequestAPIWithHeaders", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIWithExtraHeaders) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testWebRequestAPIWithExtraHeaders", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadEventsSameDocumentNavigation) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadEventsSameDocumentNavigation", app_location);
  StopTestServer();
}

// Tests the existence of WebRequest API event objects on the request
// object, on the webview element, and hanging directly off webview.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIExistence) {
  RunTest("testWebRequestAPIExistence", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIGoogleProperty) {
  RunTest("testWebRequestAPIGoogleProperty", "web_view/apitest");
}

// This test verifies that webview.contentWindow works inside an iframe
IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebViewInsideFrame) {
  LaunchApp("web_view/inside_iframe");
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_TestCaptureVisibleRegion DISABLED_TestCaptureVisibleRegion
#else
#define MAYBE_TestCaptureVisibleRegion TestCaptureVisibleRegion
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestCaptureVisibleRegion) {
  RunTest("testCaptureVisibleRegion", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNoUserCodeCreate) {
  RunTest("testCreate", "web_view/no_internal_calls_to_user_code", false);
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNoUserCodeSetOnEventProperty) {
  RunTest("testSetOnEventProperty", "web_view/no_internal_calls_to_user_code",
          false);
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNoUserCodeGetSetAttributes) {
  RunTest("testGetSetAttributes", "web_view/no_internal_calls_to_user_code",
          false);
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNoUserCodeBackForward) {
  RunTest("testBackForward", "web_view/no_internal_calls_to_user_code", false);
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNoUserCodeFocus) {
  RunTest("testFocus", "web_view/no_internal_calls_to_user_code", false);
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestClosedShadowRoot) {
  RunTest("testClosedShadowRoot", "web_view/apitest");
}

class WebViewAPITestUserAgentOverride
    : public WebViewAPITest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({blink::features::kUACHOverrideBlank},
                                          {});
    WebViewAPITest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebViewAPITestUserAgentOverride, TestSetUserAgentOverride) {
  blink::UserAgentMetadata ua_metadata;
  ua_metadata.platform = "foobar";
  content::MockClientHintsControllerDelegate client_hints_controller_delegate(
      ua_metadata);

  static_cast<ShellBrowserContext*>(
      ShellContentBrowserClient::Get()->GetBrowserContext())
      ->set_client_hints_controller_delegate(&client_hints_controller_delegate);

  // The 'Sec-CH-UA' header is only sent over HTTPS
  net::test_server::EmbeddedTestServer https_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(base::BindRepeating(
      &ExpectUserAgentResponseHandler, kExpectUserAgentPath));
  https_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  ASSERT_TRUE(https_server.Start());
  test_config_.SetByDottedPath(kTestServerPort, https_server.port());
  RunTest("testSetUserAgentOverride", "web_view/apitest");
}

}  // namespace extensions
