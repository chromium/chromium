// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "ui/events/base_event_utils.h"

namespace content {

namespace {

using WebUIImplBrowserTest = ContentBrowserTest;

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "messageRequiringGesture",
        base::BindRepeating(&TestWebUIMessageHandler::OnMessageRequiringGesture,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "notifyFinish",
        base::BindRepeating(&TestWebUIMessageHandler::OnNotifyFinish,
                            base::Unretained(this)));
  }

  void set_finish_closure(base::RepeatingClosure closure) {
    finish_closure_ = std::move(closure);
  }

  int message_requiring_gesture_count() const {
    return message_requiring_gesture_count_;
  }

 private:
  void OnMessageRequiringGesture(const base::ListValue* args) {
    ++message_requiring_gesture_count_;
  }

  void OnNotifyFinish(const base::ListValue* args) {
    if (finish_closure_)
      finish_closure_.Run();
  }

  int message_requiring_gesture_count_ = 0;
  base::RepeatingClosure finish_closure_;
};

class WebUIRequiringGestureBrowserTest : public ContentBrowserTest {
 public:
  WebUIRequiringGestureBrowserTest() {
    clock_.SetNowTicks(base::TimeTicks::Now());
    ui::SetEventTickClockForTesting(&clock_);
  }

  ~WebUIRequiringGestureBrowserTest() override {
    ui::SetEventTickClockForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(NavigateToURL(web_contents(), GetWebUIURL(kChromeUIGpuHost)));
    test_handler_ = new TestWebUIMessageHandler();
    web_contents()->GetWebUI()->AddMessageHandler(
        base::WrapUnique(test_handler_));
  }

 protected:
  void SendMessageAndWaitForFinish() {
    main_rfh()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("chrome.send('messageRequiringGesture');"
                           "chrome.send('notifyFinish');"),
        base::NullCallback());
    base::RunLoop run_loop;
    test_handler()->set_finish_closure(run_loop.QuitClosure());
    run_loop.Run();
  }

  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

  WebContents* web_contents() { return shell()->web_contents(); }
  RenderFrameHost* main_rfh() { return web_contents()->GetMainFrame(); }

  TestWebUIMessageHandler* test_handler() { return test_handler_; }

 private:
  base::SimpleTestTickClock clock_;

  // Owned by the WebUI associated with the WebContents.
  TestWebUIMessageHandler* test_handler_ = nullptr;
};

}  // namespace

// Tests that navigating between WebUIs of different types results in
// SiteInstance swap when running in process-per-tab process model.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnDifferenteWebUITypes) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  WebContents* web_contents = shell()->web_contents();

  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());

  // Navigate to a different WebUI type and ensure that the SiteInstance
  // has changed and the new process also has WebUI bindings.
  const GURL web_ui_url2(GetWebUIURL(kChromeUIGpuHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url2));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url2));
  EXPECT_NE(orig_site_instance, web_contents->GetSiteInstance());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, SameDocumentNavigationsAndReload) {
  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GetWebUIURL(kChromeUIHistogramHost)));

  WebUIMessageHandler* test_handler = new TestWebUIMessageHandler;
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));
  test_handler->AllowJavascriptForTesting();

  // Push onto window.history. Back should now be an in-page navigation.
  ASSERT_TRUE(ExecuteScript(web_contents,
                            "window.history.pushState({}, '', 'foo.html')"));
  shell()->GoBackOrForward(-1);
  WaitForLoadStop(web_contents);

  // Test handler should still have JavaScript allowed after in-page navigation.
  EXPECT_TRUE(test_handler->IsJavascriptAllowed());

  shell()->Reload();
  WaitForLoadStop(web_contents);

  // Verify that after a reload, the test handler has been disallowed.
  EXPECT_FALSE(test_handler->IsJavascriptAllowed());
}

// A WebUI message that should require a user gesture is ignored if there is no
// recent input event.
IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoredIfNoGesture) {
  SendMessageAndWaitForFinish();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoresRendererOnlyGesture) {
  // Note: this doesn't use SendMessageAndWaitForFinish() since this test needs
  // to use a test-only helper to instantiate a scoped user gesture in the
  // renderer.
  main_rfh()->ExecuteJavaScriptWithUserGestureForTests(
      base::ASCIIToUTF16("chrome.send('messageRequiringGesture');"
                         "chrome.send('notifyFinish');"));
  base::RunLoop run_loop;
  test_handler()->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoresNonInteractiveEvents) {
  // Mouse enter / mouse move / mouse leave should not be considered input
  // events that interact with the page.
  content::SimulateMouseEvent(web_contents(), blink::WebInputEvent::kMouseEnter,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents(), blink::WebInputEvent::kMouseMove,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents(), blink::WebInputEvent::kMouseLeave,
                              gfx::Point(50, 50));
  // Nor should mouse wheel.
  content::SimulateMouseWheelEvent(web_contents(), gfx::Point(50, 50),
                                   gfx::Vector2d(0, 100),
                                   blink::WebMouseWheelEvent::kPhaseBegan);
  SendMessageAndWaitForFinish();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureAllowedWithInteractiveEvent) {
  // Simulate a click at Now.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Now+0 should be allowed.
  SendMessageAndWaitForFinish();
  EXPECT_EQ(1, test_handler()->message_requiring_gesture_count());

  // Now+5 seconds should be allowed.
  AdvanceClock(base::TimeDelta::FromSeconds(5));
  SendMessageAndWaitForFinish();
  EXPECT_EQ(2, test_handler()->message_requiring_gesture_count());

  // Anything after that should be disallowed though.
  AdvanceClock(base::TimeDelta::FromMicroseconds(1));
  SendMessageAndWaitForFinish();
  EXPECT_EQ(2, test_handler()->message_requiring_gesture_count());
}

}  // namespace content
