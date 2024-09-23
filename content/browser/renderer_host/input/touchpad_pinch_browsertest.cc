// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/features.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_touchpad_pinch_gesture.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

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
    "</script>";

void PerformTouchpadPinch(WebContents* web_contents,
                          gfx::PointF position,
                          float scale_factor) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());

  SyntheticPinchGestureParams params;
  params.gesture_source_type =
      content::mojom::GestureSourceType::kTouchpadInput;
  params.scale_factor = scale_factor;
  params.anchor = position;
  auto pinch_gesture = std::make_unique<SyntheticTouchpadPinchGesture>(params);

  base::RunLoop run_loop;
  widget_host->QueueSyntheticGesture(
      std::move(pinch_gesture),
      base::BindOnce(
          [](base::OnceClosure quit_closure, SyntheticGesture::Result result) {
            EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

class TouchpadPinchBrowserTest : public ContentBrowserTest {
 public:
  TouchpadPinchBrowserTest() = default;

  TouchpadPinchBrowserTest(const TouchpadPinchBrowserTest&) = delete;
  TouchpadPinchBrowserTest& operator=(const TouchpadPinchBrowserTest&) = delete;

  ~TouchpadPinchBrowserTest() override = default;

 protected:
  void LoadURL() {
    const GURL data_url(kTouchpadPinchDataURL);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));
    HitTestRegionObserver observer(GetRenderWidgetHost()->GetFrameSinkId());
    observer.WaitForHitTestData();
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetRenderWidgetHostView()
                                          ->GetRenderWidgetHost());
  }

  // After adding a blocking event listener, we need to wait for the compositor
  // thread to become aware of the listener. If it receives input events before
  // that, the compositor thread would handle them.
  void SynchronizeCompositorAndMainThreads() {
    MainThreadFrameObserver observer(GetRenderWidgetHost());
    observer.Wait();
  }

  void EnsureNoScaleChangeWhenCanceled(
      base::OnceCallback<void(WebContents*, gfx::PointF)> send_events);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Performing a touchpad pinch gesture should change the page scale.
IN_PROC_BROWSER_TEST_F(TouchpadPinchBrowserTest,
                       TouchpadPinchChangesPageScale) {
  LoadURL();

  content::TestPageScaleObserver scale_observer(shell()->web_contents());

  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::PointF pinch_position(contents_rect.width() / 2,
                                   contents_rect.height() / 2);
  PerformTouchpadPinch(shell()->web_contents(), pinch_position, 1.23);

  scale_observer.WaitForPageScaleUpdate();
}

// We should offer synthetic wheel events to the page when a touchpad pinch
// is performed.
IN_PROC_BROWSER_TEST_F(TouchpadPinchBrowserTest, WheelListenerAllowingPinch) {
  LoadURL();
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "setListener(false);"));
  SynchronizeCompositorAndMainThreads();

  content::TestPageScaleObserver scale_observer(shell()->web_contents());

  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::PointF pinch_position(contents_rect.width() / 2,
                                   contents_rect.height() / 2);
  PerformTouchpadPinch(shell()->web_contents(), pinch_position, 1.23);

  // Ensure that the page saw the synthetic wheel.
  ASSERT_EQ(false, EvalJs(shell()->web_contents(),
                          "handlerPromise.then(function(e) {"
                          "  return e.defaultPrevented;"
                          "});"));

  // Since the listener did not cancel the synthetic wheel, we should still
  // change the page scale.
  scale_observer.WaitForPageScaleUpdate();
}

// Ensures that the event(s) sent in |send_events| are cancelable by a
// wheel event listener and that doing so prevents any scale change.
void TouchpadPinchBrowserTest::EnsureNoScaleChangeWhenCanceled(
    base::OnceCallback<void(WebContents*, gfx::PointF)> send_events) {
  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::PointF pinch_position(contents_rect.width() / 2,
                                   contents_rect.height() / 2);

  const double starting_scale_factor =
      EvalJs(shell()->web_contents(), "window.visualViewport.scale;")
          .ExtractDouble();
  ASSERT_GT(starting_scale_factor, 0.f);

  ASSERT_TRUE(ExecJs(shell()->web_contents(), "setListener(true);"));
  SynchronizeCompositorAndMainThreads();

  std::move(send_events).Run(shell()->web_contents(), pinch_position);

  // Ensure the page handled a wheel event that it was able to cancel.
  ASSERT_EQ(true, EvalJs(shell()->web_contents(),
                         "handlerPromise.then(function(e) {"
                         "  return e.defaultPrevented;"
                         "});"));

  const double last_scale_factor =
      EvalJs(shell()->web_contents(), "window.visualViewport.scale;")
          .ExtractDouble();

  ASSERT_DOUBLE_EQ(starting_scale_factor, last_scale_factor);
}

// If the synthetic wheel event for a touchpad pinch is canceled, we should not
// change the page scale.
IN_PROC_BROWSER_TEST_F(TouchpadPinchBrowserTest, WheelListenerPreventingPinch) {
  LoadURL();

  EnsureNoScaleChangeWhenCanceled(
      base::BindOnce([](WebContents* web_contents, gfx::PointF position) {
        PerformTouchpadPinch(web_contents, position, 1.5);
      }));
}

// If the synthetic wheel event for a touchpad double tap is canceled, we
// should not change the page scale.
IN_PROC_BROWSER_TEST_F(TouchpadPinchBrowserTest,
                       WheelListenerPreventingDoubleTap) {
  LoadURL();

  blink::web_pref::WebPreferences prefs =
      shell()->web_contents()->GetOrCreateWebPreferences();
  prefs.double_tap_to_zoom_enabled = true;
  shell()->web_contents()->SetWebPreferences(prefs);

  EnsureNoScaleChangeWhenCanceled(
      base::BindOnce([](WebContents* web_contents, gfx::PointF position) {
        blink::WebGestureEvent double_tap_zoom(
            blink::WebInputEvent::Type::kGestureDoubleTap,
            blink::WebInputEvent::kNoModifiers,
            blink::WebInputEvent::GetStaticTimeStampForTests(),
            blink::WebGestureDevice::kTouchpad);
        double_tap_zoom.SetPositionInWidget(position);
        double_tap_zoom.SetPositionInScreen(position);
        double_tap_zoom.data.tap.tap_count = 1;
        double_tap_zoom.SetNeedsWheelEvent(true);

        SimulateGestureEvent(web_contents, double_tap_zoom, ui::LatencyInfo());
      }));
}

}  // namespace content
