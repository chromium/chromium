// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"

namespace {

const char kTouchpadPinchDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<style>"
    "html,body {"
    " height: 100%;"
    "}"
    "</style>"
    "<p>Hello.</p>"
    "<script>"
    "  var resolveHandlerPromise = null;"
    "  var handlerPromise = new Promise(function(resolve) {"
    "    resolveHandlerPromise = resolve;"
    "  });"
    "  function preventPinchListener(e) {"
    "    e.preventDefault();"
    "    resolveHandlerPromise(e);"
    "  }"
    "  function allowPinchListener(e) {"
    "    resolveHandlerPromise(e);"
    "  }"
    "  function setListener(prevent) {"
    "    document.body.addEventListener("
    "        'wheel',"
    "        (prevent ? preventPinchListener : allowPinchListener),"
    "        {passive: false});"
    "  }"
    "  function reset() {"
    "    document.body.removeEventListener("
    "        'wheel', preventPinchListener, {passive: false});"
    "    document.body.removeEventListener("
    "        'wheel', allowPinchListener, {passive: false});"
    "    handlerPromise = new Promise(function(resolve) {"
    "      resolveHandlerPromise = resolve;"
    "    });"
    "  }"
    "</script>";

}  // namespace

namespace content {

class TouchpadPinchBrowserTest : public ContentBrowserTest,
                                 public testing::WithParamInterface<bool> {
 public:
  TouchpadPinchBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kTouchpadAsyncPinchEvents);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kTouchpadAsyncPinchEvents);
    }
  }
  ~TouchpadPinchBrowserTest() override = default;

 protected:
  void LoadURL() {
    const GURL data_url(kTouchpadPinchDataURL);
    NavigateToURL(shell(), data_url);
    SynchronizeThreads();
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetRenderWidgetHostView()
                                          ->GetRenderWidgetHost());
  }

  void SynchronizeThreads() {
    MainThreadFrameObserver observer(GetRenderWidgetHost());
    observer.Wait();
  }

  void EnsureNoScaleChangeWhenCanceled(
      base::OnceCallback<void(WebContents*, gfx::Point)> send_events);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(TouchpadPinchBrowserTest);
};

INSTANTIATE_TEST_CASE_P(, TouchpadPinchBrowserTest, testing::Bool());

// Performing a touchpad pinch gesture should change the page scale.
IN_PROC_BROWSER_TEST_P(TouchpadPinchBrowserTest,
                       TouchpadPinchChangesPageScale) {
  LoadURL();

  content::TestPageScaleObserver scale_observer(shell()->web_contents());

  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.width() / 2,
                                  contents_rect.height() / 2);
  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position, 1.23,
                               blink::kWebGestureDeviceTouchpad);

  scale_observer.WaitForPageScaleUpdate();
}

// We should offer synthetic wheel events to the page when a touchpad pinch
// is performed.
IN_PROC_BROWSER_TEST_P(TouchpadPinchBrowserTest, WheelListenerAllowingPinch) {
  LoadURL();
  ASSERT_TRUE(
      content::ExecuteScript(shell()->web_contents(), "setListener(false);"));
  SynchronizeThreads();

  content::TestPageScaleObserver scale_observer(shell()->web_contents());

  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.width() / 2,
                                  contents_rect.height() / 2);
  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position, 1.23,
                               blink::kWebGestureDeviceTouchpad);

  // Ensure that the page saw the synthetic wheel.
  bool default_prevented = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "handlerPromise.then(function(e) {"
      "  window.domAutomationController.send(e.defaultPrevented);"
      "});",
      &default_prevented));
  EXPECT_FALSE(default_prevented);

  // Since the listener did not cancel the synthetic wheel, we should still
  // change the page scale.
  scale_observer.WaitForPageScaleUpdate();
}

// Ensures that the event(s) sent in |send_events| are cancelable by a
// wheel event listener and that doing so prevents any scale change.
void TouchpadPinchBrowserTest::EnsureNoScaleChangeWhenCanceled(
    base::OnceCallback<void(WebContents*, gfx::Point)> send_events) {
  // Perform an initial pinch so we can figure out the page scale we're
  // starting with for the test proper.
  content::TestPageScaleObserver starting_scale_observer(
      shell()->web_contents());
  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.width() / 2,
                                  contents_rect.height() / 2);
  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position, 1.23,
                               blink::kWebGestureDeviceTouchpad);
  const float starting_scale_factor =
      starting_scale_observer.WaitForPageScaleUpdate();
  ASSERT_GT(starting_scale_factor, 0.f);

  ASSERT_TRUE(
      content::ExecuteScript(shell()->web_contents(), "setListener(true);"));
  SynchronizeThreads();

  std::move(send_events).Run(shell()->web_contents(), pinch_position);

  // Ensure the page handled a wheel event that it was able to cancel.
  bool default_prevented = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "handlerPromise.then(function(e) {"
      "  window.domAutomationController.send(e.defaultPrevented);"
      "});",
      &default_prevented));
  EXPECT_TRUE(default_prevented);

  // We'll check that the previous event(s) did not cause a scale change by
  // performing another pinch that does change the scale.
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "reset(); "
                                     "setListener(false);"));
  SynchronizeThreads();

  content::TestPageScaleObserver scale_observer(shell()->web_contents());
  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position, 2.0,
                               blink::kWebGestureDeviceTouchpad);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "handlerPromise.then(function(e) {"
      "  window.domAutomationController.send(e.defaultPrevented);"
      "});",
      &default_prevented));
  EXPECT_FALSE(default_prevented);

  const float last_scale_factor = scale_observer.WaitForPageScaleUpdate();
  EXPECT_FLOAT_EQ(starting_scale_factor * 2.0, last_scale_factor);
}

// If the synthetic wheel event for a touchpad pinch is canceled, we should not
// change the page scale.
IN_PROC_BROWSER_TEST_P(TouchpadPinchBrowserTest, WheelListenerPreventingPinch) {
  LoadURL();

  EnsureNoScaleChangeWhenCanceled(
      base::BindOnce([](WebContents* web_contents, gfx::Point position) {
        SimulateGesturePinchSequence(web_contents, position, 1.5,
                                     blink::kWebGestureDeviceTouchpad);
      }));
}

// If the synthetic wheel event for a touchpad double tap is canceled, we
// should not change the page scale.
IN_PROC_BROWSER_TEST_P(TouchpadPinchBrowserTest,
                       WheelListenerPreventingDoubleTap) {
  LoadURL();

  WebPreferences prefs =
      shell()->web_contents()->GetRenderViewHost()->GetWebkitPreferences();
  prefs.double_tap_to_zoom_enabled = true;
  shell()->web_contents()->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  EnsureNoScaleChangeWhenCanceled(
      base::BindOnce([](WebContents* web_contents, gfx::Point position) {
        blink::WebGestureEvent double_tap_zoom(
            blink::WebInputEvent::kGestureDoubleTap,
            blink::WebInputEvent::kNoModifiers,
            blink::WebInputEvent::GetStaticTimeStampForTests(),
            blink::kWebGestureDeviceTouchpad);
        double_tap_zoom.SetPositionInWidget(gfx::PointF(position));
        double_tap_zoom.SetPositionInScreen(gfx::PointF(position));
        double_tap_zoom.data.tap.tap_count = 1;
        double_tap_zoom.SetNeedsWheelEvent(true);

        SimulateGestureEvent(web_contents, double_tap_zoom,
                             ui::LatencyInfo(ui::SourceEventType::WHEEL));
      }));
}

}  // namespace content
