// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock_browsertest.h"

#include <string>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/keyboard_lock/keyboard_lock_metrics.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

#ifdef USE_AURA
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#endif  // USE_AURA

namespace content {

namespace {

constexpr char kFullscreenFramePath[] = "/fullscreen_frame.html";

constexpr char kHelloFramePath[] = "/hello.html";

constexpr char kInputFieldFramePath[] = "/page_with_input_field.html";

// Set up a DOM structure which contains three inner iframes for testing:
// - Same domain iframe w/ fullscreen attribute.
// - Cross domain iframe.
// - Cross domain iframe w/ fullscreen attribute.
constexpr char kCrossSiteFramePath[] =
    "/cross_site_iframe_factory.html"
    "?a(a{allowfullscreen}(),b(),c{allowfullscreen}())";

constexpr char kCrossSiteTopLevelDomain[] = "a.com";

constexpr char kChildIframeName_0[] = "child-0";

constexpr char kChildIframeName_1[] = "child-1";

constexpr char kChildIframeName_2[] = "child-2";

constexpr char kCrossSiteChildDomain1[] = "b.com";

constexpr char kCrossSiteChildDomain2[] = "c.com";

constexpr char kKeyboardLockMethodExistanceCheck[] =
    "(navigator.keyboard != undefined) &&"
    "(navigator.keyboard.lock != undefined);";

constexpr char kKeyboardLockMethodCallWithAllKeys[] =
    "navigator.keyboard.lock().then("
    "  () => true,"
    "  () => false,"
    ");";

constexpr char kKeyboardLockMethodCallWithSomeKeys[] =
    "navigator.keyboard.lock(['MetaLeft', 'Tab', 'AltLeft']).then("
    "  () => true,"
    "  () => false,"
    ");";

// Calling lock() with no valid key codes will cause the promise to be rejected.
constexpr char kKeyboardLockMethodCallWithAllInvalidKeys[] =
    "navigator.keyboard.lock(['BlerghLeft', 'BlarghRight']).then("
    "  () => false,"
    "  () => true,"
    ");";

// Calling lock() with some invalid key codes will reject the promise.
constexpr char kKeyboardLockMethodCallWithSomeInvalidKeys[] =
    "navigator.keyboard.lock(['Tab', 'BlarghTab', 'Space', 'BlerghLeft']).then("
    "  () => false,"
    "  () => true,"
    ");";

constexpr char kKeyboardUnlockMethodCall[] = "navigator.keyboard.unlock()";

constexpr char kFocusInputFieldScript[] =
    "function onInput(e) {"
    "  resultQueue.push(getInputFieldText());"
    "}"
    "inputField = document.getElementById('text-field');"
    "inputField.addEventListener('input', onInput, false);";

void SimulateKeyPress(WebContents* web_contents,
                      RenderFrameHost* event_recipient,
                      const std::string& code_string,
                      const std::string& expected_result) {
  ui::DomKey dom_key = ui::KeycodeConverter::KeyStringToDomKey(code_string);
  ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(code_string);
  SimulateKeyPress(web_contents, dom_key, dom_code,
                   ui::DomCodeToUsLayoutKeyboardCode(dom_code), false, false,
                   false, false);
  ASSERT_EQ(expected_result, EvalJs(event_recipient, "waitForInput()"));
}

#if defined(USE_AURA)

bool g_window_has_focus = false;

class TestRenderWidgetHostView : public RenderWidgetHostViewAura {
 public:
  explicit TestRenderWidgetHostView(RenderWidgetHost* host)
      : RenderWidgetHostViewAura(host) {}
  ~TestRenderWidgetHostView() override {}

  bool HasFocus() override { return g_window_has_focus; }

  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    // Ignore all focus events.
  }
};

#endif  // USE_AURA

class FakeKeyboardLockWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeKeyboardLockWebContentsDelegate() {}

  FakeKeyboardLockWebContentsDelegate(
      const FakeKeyboardLockWebContentsDelegate&) = delete;
  FakeKeyboardLockWebContentsDelegate& operator=(
      const FakeKeyboardLockWebContentsDelegate&) = delete;

  ~FakeKeyboardLockWebContentsDelegate() override {}

  // WebContentsDelegate overrides.
  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override;
  void RequestKeyboardLock(WebContents* web_contents,
                           bool esc_key_locked) override;
  void CancelKeyboardLockRequest(WebContents* web_contents) override;

 private:
  bool is_fullscreen_ = false;
  bool keyboard_lock_requested_ = false;
};

void FakeKeyboardLockWebContentsDelegate::EnterFullscreenModeForTab(
    RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  is_fullscreen_ = true;
  auto* web_contents = WebContents::FromRenderFrameHost(requesting_frame);
  if (keyboard_lock_requested_)
    web_contents->GotResponseToKeyboardLockRequest(/*allowed=*/true);
}

void FakeKeyboardLockWebContentsDelegate::ExitFullscreenModeForTab(
    WebContents* web_contents) {
  is_fullscreen_ = false;
  if (keyboard_lock_requested_)
    web_contents->GotResponseToKeyboardLockRequest(/*allowed=*/false);
}

bool FakeKeyboardLockWebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) {
  return is_fullscreen_;
}

void FakeKeyboardLockWebContentsDelegate::RequestKeyboardLock(
    WebContents* web_contents,
    bool esc_key_locked) {
  keyboard_lock_requested_ = true;
  web_contents->GotResponseToKeyboardLockRequest(/*allowed=*/true);
}

void FakeKeyboardLockWebContentsDelegate::CancelKeyboardLockRequest(
    WebContents* web_contents) {
  keyboard_lock_requested_ = false;
}

}  // namespace

#if defined(USE_AURA)

void SetWindowFocusForKeyboardLockBrowserTests(bool is_focused) {
  g_window_has_focus = is_focused;
}

void InstallCreateHooksForKeyboardLockBrowserTests() {
  WebContentsViewAura::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewAura* {
        return new TestRenderWidgetHostView(host);
      });
}

#endif  // USE_AURA

class KeyboardLockBrowserTest : public ContentBrowserTest {
 public:
  KeyboardLockBrowserTest();
  ~KeyboardLockBrowserTest() override;

 protected:
  // ContentBrowserTest overrides.
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  // Helper methods for common tasks.
  bool KeyboardLockApiExists();
  void NavigateToTestURL(const GURL& gurl);
  void RequestKeyboardLock(const base::Location& from_here,
                           bool lock_all_keys = true);
  void CancelKeyboardLock(const base::Location& from_here);
  void EnterFullscreen(const base::Location& from_here);
  void ExitFullscreen(const base::Location& from_here);
  void FocusContent(const base::Location& from_here);
  void BlurContent(const base::Location& from_here);
  void VerifyKeyboardLockState(const base::Location& from_here);

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

  GURL https_fullscreen_frame() {
    return https_test_server()->GetURL(kFullscreenFramePath);
  }

  GURL https_cross_site_frame() {
    return https_test_server()->GetURL(kCrossSiteTopLevelDomain,
                                       kCrossSiteFramePath);
  }

  base::test::ScopedFeatureList* feature_list() {
    return &scoped_feature_list_;
  }

  WebContentsDelegate* web_contents_delegate() {
    return &web_contents_delegate_;
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_test_server_;
  FakeKeyboardLockWebContentsDelegate web_contents_delegate_;
};

KeyboardLockBrowserTest::KeyboardLockBrowserTest()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

KeyboardLockBrowserTest::~KeyboardLockBrowserTest() = default;

void KeyboardLockBrowserTest::SetUp() {
  // Assume we have focus to start with.
  SetWindowFocusForKeyboardLockBrowserTests(true);
  InstallCreateHooksForKeyboardLockBrowserTests();
  ContentBrowserTest::SetUp();
}

void KeyboardLockBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  mock_cert_verifier_.SetUpCommandLine(command_line);
}

void KeyboardLockBrowserTest::SetUpOnMainThread() {
  ContentBrowserTest::SetUpOnMainThread();
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

  web_contents()->SetDelegate(&web_contents_delegate_);

  // KeyboardLock requires a secure context (HTTPS).
  https_test_server()->AddDefaultHandlers(GetTestDataFilePath());
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(https_test_server());
  ASSERT_TRUE(https_test_server()->Start());
}

void KeyboardLockBrowserTest::SetUpInProcessBrowserTestFixture() {
  ContentBrowserTest::SetUpInProcessBrowserTestFixture();
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void KeyboardLockBrowserTest::TearDownInProcessBrowserTestFixture() {
  ContentBrowserTest::TearDownInProcessBrowserTestFixture();
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

bool KeyboardLockBrowserTest::KeyboardLockApiExists() {
  return EvalJs(web_contents(), kKeyboardLockMethodExistanceCheck)
      .ExtractBool();
}

void KeyboardLockBrowserTest::NavigateToTestURL(const GURL& gurl) {
  ASSERT_TRUE(NavigateToURL(shell(), gurl));

  ASSERT_TRUE(KeyboardLockApiExists());

  // Ensure the window has focus and is in windowed mode after the navigation.
  FocusContent(FROM_HERE);
  ExitFullscreen(FROM_HERE);
}

void KeyboardLockBrowserTest::RequestKeyboardLock(
    const base::Location& from_here,
    bool lock_all_keys /*=true*/) {
  // keyboard.lock() is an async call which requires a promise handling dance.
  bool result = EvalJs(web_contents()->GetPrimaryMainFrame(),
                       lock_all_keys ? kKeyboardLockMethodCallWithAllKeys
                                     : kKeyboardLockMethodCallWithSomeKeys)
                    .ExtractBool();

  ASSERT_TRUE(result) << "Location: " << from_here.ToString();

  ASSERT_EQ(result, web_contents()->GetKeyboardLockWidget() != nullptr)
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::CancelKeyboardLock(
    const base::Location& from_here) {
  // keyboard.unlock() is a synchronous call.
  ASSERT_TRUE(
      ExecJs(web_contents()->GetPrimaryMainFrame(), kKeyboardUnlockMethodCall));

  ASSERT_EQ(nullptr, web_contents()->GetKeyboardLockWidget())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::EnterFullscreen(const base::Location& from_here) {
  web_contents()->EnterFullscreenMode(web_contents()->GetPrimaryMainFrame(),
                                      {});

  ASSERT_TRUE(web_contents()->IsFullscreen())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::ExitFullscreen(const base::Location& from_here) {
  web_contents()->ExitFullscreenMode(/*should_resize=*/true);

  ASSERT_FALSE(web_contents()->IsFullscreen())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::FocusContent(const base::Location& from_here) {
  SetWindowFocusForKeyboardLockBrowserTests(true);
  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  host->GotFocus();
  host->SetActive(true);

  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->HasFocus())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::BlurContent(const base::Location& from_here) {
  SetWindowFocusForKeyboardLockBrowserTests(false);
  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  host->SetActive(false);
  host->LostFocus();

  ASSERT_FALSE(web_contents()->GetRenderWidgetHostView()->HasFocus())
      << "Location: " << from_here.ToString();

  VerifyKeyboardLockState(from_here);
}

void KeyboardLockBrowserTest::VerifyKeyboardLockState(
    const base::Location& from_here) {
  bool keyboard_lock_requested = !!web_contents()->GetKeyboardLockWidget();

  bool ux_conditions_satisfied =
      web_contents()->GetRenderWidgetHostView()->HasFocus() &&
      web_contents()->IsFullscreen();

  bool keyboard_lock_active =
      web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked();

  // Keyboard lock only active when requested and the UX is in the right state.
  ASSERT_EQ(keyboard_lock_active,
            ux_conditions_satisfied && keyboard_lock_requested)
      << "Location: " << from_here.ToString();
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, SingleLockCall) {
  NavigateToTestURL(https_fullscreen_frame());
  base::HistogramTester uma;
  RequestKeyboardLock(FROM_HERE);
  // Don't explicitly call CancelKeyboardLock().

  uma.ExpectTotalCount(kKeyboardLockMethodCalledHistogramName, 1);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kRequestAllKeys),
                        1);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, SingleLockCallForSomeKeys) {
  NavigateToTestURL(https_fullscreen_frame());
  base::HistogramTester uma;
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  // Don't explicitly call CancelKeyboardLock().

  uma.ExpectTotalCount(kKeyboardLockMethodCalledHistogramName, 1);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kRequestSomeKeys),
                        1);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, SingleLockWithCancelCall) {
  NavigateToTestURL(https_fullscreen_frame());
  base::HistogramTester uma;
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);

  uma.ExpectTotalCount(kKeyboardLockMethodCalledHistogramName, 2);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kRequestAllKeys),
                        1);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kCancelLock), 1);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, LockCalledBeforeFullscreen) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);
  RequestKeyboardLock(FROM_HERE);
  EnterFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, LockCalledAfterFullscreen) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);
  EnterFullscreen(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       LockAndCancelCyclingNoActivation) {
  NavigateToTestURL(https_fullscreen_frame());

  base::HistogramTester uma;
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);

  uma.ExpectTotalCount(kKeyboardLockMethodCalledHistogramName, 8);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kRequestAllKeys),
                        3);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kRequestSomeKeys),
                        1);
  uma.ExpectBucketCount(kKeyboardLockMethodCalledHistogramName,
                        static_cast<int>(KeyboardLockMethods::kCancelLock), 4);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       LockAndCancelCyclingInFullscreen) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  EnterFullscreen(FROM_HERE);

  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE, /*lock_all_keys=*/false);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, CancelInFullscreen) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  RequestKeyboardLock(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  ExitFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, EnterAndExitFullscreenCycling) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  RequestKeyboardLock(FROM_HERE);

  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, GainAndLoseFocusInWindowMode) {
  NavigateToTestURL(https_fullscreen_frame());

  RequestKeyboardLock(FROM_HERE);

  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, EnterFullscreenWithoutFocus) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  RequestKeyboardLock(FROM_HERE);

  BlurContent(FROM_HERE);
  EnterFullscreen(FROM_HERE);
  ExitFullscreen(FROM_HERE);

  EnterFullscreen(FROM_HERE);
  FocusContent(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       GainAndLoseFocusCyclingInFullscreen) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  RequestKeyboardLock(FROM_HERE);

  BlurContent(FROM_HERE);
  EnterFullscreen(FROM_HERE);

  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);
  FocusContent(FROM_HERE);
  BlurContent(FROM_HERE);

  ExitFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, CancelWithoutLock) {
  NavigateToTestURL(https_fullscreen_frame());
  CancelKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MultipleLockCalls) {
  NavigateToTestURL(https_fullscreen_frame());

  RequestKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, MultipleCancelCalls) {
  NavigateToTestURL(https_fullscreen_frame());

  RequestKeyboardLock(FROM_HERE);

  CancelKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
  CancelKeyboardLock(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, LockCallWithAllInvalidKeys) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  ASSERT_EQ(true,
            EvalJs(web_contents(), kKeyboardLockMethodCallWithAllInvalidKeys));

  // If no valid Keys are passed in, then keyboard lock will not be requested.
  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());

  EnterFullscreen(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, LockCallWithSomeInvalidKeys) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);

  ASSERT_EQ(true,
            EvalJs(web_contents(), kKeyboardLockMethodCallWithSomeInvalidKeys));

  // If some valid Keys are passed in, then keyboard lock will not be requested.
  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       ValidLockCallFollowedByInvalidLockCall) {
  NavigateToTestURL(https_fullscreen_frame());

  RequestKeyboardLock(FROM_HERE);
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());

  ASSERT_EQ(true,
            EvalJs(web_contents(), kKeyboardLockMethodCallWithSomeInvalidKeys));

  // An invalid call will cancel any previous lock request.
  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       KeyboardLockNotAllowedForSameOriginIFrame) {
  NavigateToTestURL(https_cross_site_frame());

  // The first child has the same origin as the top-level domain.
  RenderFrameHost* child_frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(),
                   /*index=*/0);
  ASSERT_TRUE(child_frame);

  ASSERT_EQ(true, EvalJs(child_frame, kKeyboardLockMethodExistanceCheck));

  ASSERT_EQ(false, EvalJs(child_frame, kKeyboardLockMethodCallWithAllKeys));

  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       KeyboardLockNotAllowedForCrossOriginIFrame) {
  NavigateToTestURL(https_cross_site_frame());

  // The second child has a different origin as the top-level domain.
  RenderFrameHost* child_frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(),
                   /*index=*/1);
  ASSERT_TRUE(child_frame);

  ASSERT_EQ(true, EvalJs(child_frame, kKeyboardLockMethodExistanceCheck));

  ASSERT_EQ(false, EvalJs(child_frame, kKeyboardLockMethodCallWithAllKeys));

  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       KeyboardUnlockedWhenNavigatingToSameUrl) {
  GURL url_for_test = https_fullscreen_frame();
  NavigateToTestURL(url_for_test);
  EnterFullscreen(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);

  // Navigate to the same URL which will reset the keyboard lock state.
  NavigateToTestURL(url_for_test);
  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());

  // Entering fullscreen on the new page should not engage keyboard lock.
  EnterFullscreen(FROM_HERE);
  ASSERT_FALSE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       KeyboardUnlockedWhenNavigatingAway) {
  GURL first_url_for_test = https_fullscreen_frame();
  NavigateToTestURL(first_url_for_test);
  EnterFullscreen(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);

  // Navigate to a new URL which will reset the keyboard lock state.
  GURL second_url_for_test = https_cross_site_frame();
  NavigateToTestURL(second_url_for_test);
  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());

  // Entering fullscreen on the new page should not engage keyboard lock.
  EnterFullscreen(FROM_HERE);
  ASSERT_FALSE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       KeyboardRemainsLockedWhenIframeNavigates) {
  NavigateToTestURL(https_cross_site_frame());
  EnterFullscreen(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);

  ASSERT_TRUE(NavigateIframeToURL(
      web_contents(), kChildIframeName_0,
      https_test_server()->GetURL(kCrossSiteTopLevelDomain, kHelloFramePath)));
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  ASSERT_TRUE(NavigateIframeToURL(
      web_contents(), kChildIframeName_1,
      https_test_server()->GetURL(kCrossSiteChildDomain1, kHelloFramePath)));
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  ASSERT_TRUE(NavigateIframeToURL(
      web_contents(), kChildIframeName_2,
      https_test_server()->GetURL(kCrossSiteChildDomain2, kHelloFramePath)));
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), kChildIframeName_0,
                          https_test_server()->GetURL(kCrossSiteChildDomain2,
                                                      kInputFieldFramePath)));
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), kChildIframeName_1,
                          https_test_server()->GetURL(kCrossSiteTopLevelDomain,
                                                      kInputFieldFramePath)));
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), kChildIframeName_2,
                          https_test_server()->GetURL(kCrossSiteChildDomain1,
                                                      kInputFieldFramePath)));
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       CrossOriginIFrameReceivesInputWhenFocused) {
  NavigateToTestURL(https_cross_site_frame());
  EnterFullscreen(FROM_HERE);
  RequestKeyboardLock(FROM_HERE);

  GURL iframe_url =
      https_test_server()->GetURL(kCrossSiteChildDomain1, kInputFieldFramePath);
  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), kChildIframeName_1, iframe_url));
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* child = ChildFrameAt(main_frame, 1);
  ASSERT_TRUE(child);

  ASSERT_EQ(main_frame, web_contents()->GetFocusedFrame());

  ASSERT_TRUE(ExecJs(child, kFocusInputFieldScript));
  ASSERT_EQ("input-focus", EvalJs(child, "window.focus(); focusInputField();"));
  ASSERT_EQ(child, web_contents()->GetFocusedFrame());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  SimulateKeyPress(web_contents(), child, "KeyB", "B");
  SimulateKeyPress(web_contents(), child, "KeyL", "BL");
  SimulateKeyPress(web_contents(), child, "KeyA", "BLA");
  SimulateKeyPress(web_contents(), child, "KeyR", "BLAR");
  SimulateKeyPress(web_contents(), child, "KeyG", "BLARG");
  SimulateKeyPress(web_contents(), child, "KeyH", "BLARGH");
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       LockRequestBeforeCrossOriginIFrameIsFullscreen) {
  // If the main frame trusts the child frame by granting it the allowfullscreen
  // permission, then we will allow keyboard lock to be activated when the child
  // frame activates fullscreen.
  NavigateToTestURL(https_cross_site_frame());
  RequestKeyboardLock(FROM_HERE);
  ASSERT_TRUE(web_contents()->GetKeyboardLockWidget());
  ASSERT_FALSE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());

  // The third child is cross-domain and has the allowfullscreen attribute set.
  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), kChildIframeName_2,
                          https_test_server()->GetURL(kCrossSiteChildDomain2,
                                                      kFullscreenFramePath)));
  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* child = ChildFrameAt(main_frame, 2);
  ASSERT_TRUE(child);

  ASSERT_TRUE(ExecJs(child, "activateFullscreen()"));

  ASSERT_EQ(main_frame->GetView()->GetRenderWidgetHost(),
            web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       LockRequestWhileCrossOriginIFrameIsFullscreen) {
  // If the main frame trusts the child frame by granting it the allowfullscreen
  // permission, then we will allow keyboard lock to be activated when the child
  // frame activates fullscreen.
  NavigateToTestURL(https_cross_site_frame());

  // The third child is cross-domain and has the allowfullscreen attribute set.
  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), kChildIframeName_2,
                          https_test_server()->GetURL(kCrossSiteChildDomain2,
                                                      kFullscreenFramePath)));
  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* child = ChildFrameAt(main_frame, 2);
  ASSERT_TRUE(child);

  ASSERT_TRUE(ExecJs(child, "activateFullscreen()"));

  RequestKeyboardLock(FROM_HERE);

  ASSERT_EQ(main_frame->GetView()->GetRenderWidgetHost(),
            web_contents()->GetKeyboardLockWidget());
  ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       LockRequestFailsFromInnerWebContents) {
  NavigateToTestURL(https_cross_site_frame());

  // The first child is a same-origin iframe.
  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child);

  WebContents* inner_contents = CreateAndAttachInnerContents(child);
  inner_contents->SetDelegate(web_contents_delegate());

  ASSERT_TRUE(
      NavigateToURLFromRenderer(inner_contents, https_fullscreen_frame()));

  ASSERT_EQ(true, EvalJs(inner_contents, kKeyboardLockMethodExistanceCheck));

  ASSERT_EQ(false, EvalJs(inner_contents, kKeyboardLockMethodCallWithAllKeys));

  // Verify neither inner nor outer WebContents have a pending lock request.
  WebContentsImpl* inner_contents_impl =
      static_cast<WebContentsImpl*>(inner_contents);
  ASSERT_FALSE(inner_contents_impl->GetKeyboardLockWidget());
  ASSERT_FALSE(
      inner_contents_impl->GetRenderWidgetHostView()->IsKeyboardLocked());
  ASSERT_FALSE(web_contents()->GetKeyboardLockWidget());
  ASSERT_FALSE(web_contents()->GetRenderWidgetHostView()->IsKeyboardLocked());
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest,
                       InnerContentsFullscreenBehavior) {
  // TODO(joedow): Added per code review feedback.  Need to define the behavior
  // for KeyboardLock when an attached InnerWebContents goes fullscreen.
  // Steps: 1. Request keyboard lock for all keys
  //        2. InnerWebContents request fullscreen
  //        3. Verify KeyboardLock behavior (should match iframe behavior)
}

IN_PROC_BROWSER_TEST_F(KeyboardLockBrowserTest, InnerContentsInputBehavior) {
  // TODO(joedow): Added per code review feedback.  Need to define the behavior
  // for KeyboardLock when an attached InnerWebContents goes fullscreen.
  // Steps: 1. Request keyboard lock for all keys
  //        2. Main frame goes fullscreen
  //        3. Inner web contents is focused
  //        4. Verify input behavior (should match iframe behavior)
}

}  // namespace content
