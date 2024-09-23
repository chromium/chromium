// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_controller.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_driver.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

const std::string kEventListenerDataURL = R"HTML(
  <!DOCTYPE html>
  <meta name='viewport' content='width=device-width'/>
  <style>
  html, body {
    margin: 0;
  }
  .spacer { height: 10000px; }
  </style>
  <div class=spacer></div>
  <script type="text/javascript">
    window.eventCounts =
        {mousedown: 0, keydown: 0, touchstart: 0, click: 0, wheel: 0};
    window.eventTimeStamp =
        {mousedown: 0, keydown: 0, touchstart: 0, click: 0, wheel: 0};
    function recordEvent(e) {
        eventCounts[e.type]++;
        if (eventCounts[e.type] == 1)
            eventTimeStamp[e.type] = e.timeStamp;
    }
    for (var evt in eventCounts) {
        document.addEventListener(evt, recordEvent);
    }
    document.title='ready';
  </script>)HTML";

}  // namespace

namespace content {

class InputEventBrowserTest : public ContentBrowserTest {
 public:
  InputEventBrowserTest() = default;

  InputEventBrowserTest(const InputEventBrowserTest&) = delete;
  InputEventBrowserTest& operator=(const InputEventBrowserTest&) = delete;

  ~InputEventBrowserTest() override = default;

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void LoadURL(const std::string& page_data) {
    const GURL data_url("data:text/html," + page_data);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    frame_observer_ = std::make_unique<RenderFrameSubmissionObserver>(
        host->render_frame_metadata_provider());
    host->GetView()->SetSize(gfx::Size(400, 400));

    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    std::ignore = watcher.WaitAndGetTitle();

    // We need to wait until hit test data is available. We use our own
    // HitTestRegionObserver here because we have the RenderWidgetHostImpl
    // available.
    HitTestRegionObserver observer(host->GetFrameSinkId());
    observer.WaitForHitTestData();
  }

  // ContentBrowserTest:
  void PostRunTestOnMainThread() override {
    // Delete this before the WebContents is destroyed.
    frame_observer_.reset();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  bool URLLoaded() {
    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    const std::u16string title = watcher.WaitAndGetTitle();
    return title == ready_title;
  }

  void SimulateSyntheticMousePressAt(base::TimeTicks event_time) {
    DCHECK(URLLoaded());

    std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver =
        SyntheticPointerDriver::Create(
            content::mojom::GestureSourceType::kMouseInput);
    RenderWidgetHostImpl* render_widget_host = GetWidgetHost();
    auto* root_view = render_widget_host->GetView()->GetRootView();
    std::unique_ptr<SyntheticGestureTarget> synthetic_gesture_target;
    if (root_view)
      synthetic_gesture_target = root_view->CreateSyntheticGestureTarget();
    else
      synthetic_gesture_target =
          render_widget_host->GetView()->CreateSyntheticGestureTarget();

    synthetic_pointer_driver->Press(50, 50, 0,
                                    SyntheticPointerActionParams::Button::LEFT);
    synthetic_pointer_driver->DispatchEvent(synthetic_gesture_target.get(),
                                            event_time);

    synthetic_pointer_driver->Release(
        0, SyntheticPointerActionParams::Button::LEFT);
    synthetic_pointer_driver->DispatchEvent(synthetic_gesture_target.get(),
                                            event_time);
  }

  void SimulateSyntheticKeyDown(base::TimeTicks event_time) {
    DCHECK(URLLoaded());

    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, event_time);
    event.windows_key_code = ui::VKEY_DOWN;
    event.native_key_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ARROW_DOWN);
    event.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
    event.dom_key = ui::DomKey::ARROW_DOWN;
    GetWidgetHost()->ForwardKeyboardEvent(event);
  }

  void SimulateSyntheticTouchTapAt(base::TimeTicks event_time) {
    DCHECK(URLLoaded());

    std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver =
        SyntheticPointerDriver::Create(
            content::mojom::GestureSourceType::kTouchInput);
    RenderWidgetHostImpl* render_widget_host = GetWidgetHost();
    auto* root_view = render_widget_host->GetView()->GetRootView();
    std::unique_ptr<SyntheticGestureTarget> synthetic_gesture_target;
    if (root_view)
      synthetic_gesture_target = root_view->CreateSyntheticGestureTarget();
    else
      synthetic_gesture_target =
          render_widget_host->GetView()->CreateSyntheticGestureTarget();

    synthetic_pointer_driver->Press(50, 50, 0,
                                    SyntheticPointerActionParams::Button::LEFT);
    synthetic_pointer_driver->DispatchEvent(synthetic_gesture_target.get(),
                                            event_time);

    synthetic_pointer_driver->Release(
        0, SyntheticPointerActionParams::Button::LEFT);
    synthetic_pointer_driver->DispatchEvent(synthetic_gesture_target.get(),
                                            event_time);
  }

  void SimulateSyntheticWheelScroll(base::TimeTicks event_time) {
    DCHECK(URLLoaded());

    double x = 50;
    double y = 50;
    blink::WebMouseWheelEvent wheel_event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            x, y, x, y, 20, 20, 0,
            ui::ScrollGranularity::kScrollByPrecisePixel);
    wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
    wheel_event.SetTimeStamp(event_time);
    GetWidgetHost()->ForwardWheelEvent(wheel_event);
  }

 private:
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer_;
};

#if BUILDFLAG(IS_ANDROID)
// Android does not support synthetic mouse events.
// TODO(lanwei): support dispatching WebMouseEvent in
// SyntheticGestureTargetAndroid.
#define MAYBE_MouseDownEventTimeStamp DISABLED_MouseDownEventTimeStamp
#else
#define MAYBE_MouseDownEventTimeStamp MouseDownEventTimeStamp
#endif
IN_PROC_BROWSER_TEST_F(InputEventBrowserTest, MAYBE_MouseDownEventTimeStamp) {
  LoadURL(kEventListenerDataURL);

  MainThreadFrameObserver frame_observer(GetWidgetHost());
  base::TimeTicks event_time = base::TimeTicks::Now();
  int64_t event_time_ms = event_time.since_origin().InMilliseconds();
  SimulateSyntheticMousePressAt(event_time);
  while (EvalJs(shell(), "eventCounts.mousedown") == 0) {
    frame_observer.Wait();
  }

  int64_t monotonic_time =
      EvalJs(shell(),
             "internals.zeroBasedDocumentTimeToMonotonicTime(eventTimeStamp."
             "mousedown)")
          .ExtractDouble();
  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.mousedown"));
  EXPECT_NEAR(event_time_ms, monotonic_time, 1);
}

IN_PROC_BROWSER_TEST_F(InputEventBrowserTest, KeyDownEventTimeStamp) {
  LoadURL(kEventListenerDataURL);

  MainThreadFrameObserver frame_observer(GetWidgetHost());

  base::TimeTicks event_time = base::TimeTicks::Now();
  int64_t event_time_ms = event_time.since_origin().InMilliseconds();
  SimulateSyntheticKeyDown(event_time);

  while (EvalJs(shell(), "eventCounts.keydown") == 0) {
    frame_observer.Wait();
  }

  int64_t monotonic_time =
      EvalJs(shell(),
             "internals.zeroBasedDocumentTimeToMonotonicTime(eventTimeStamp."
             "keydown)")
          .ExtractDouble();
  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.keydown"));
  EXPECT_NEAR(event_time_ms, monotonic_time, 1);
}

// TODO(crbug.com/41489011): Flaky on LaCros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TouchStartEventTimeStamp DISABLED_TouchStartEventTimeStamp
#else
#define MAYBE_TouchStartEventTimeStamp TouchStartEventTimeStamp
#endif
IN_PROC_BROWSER_TEST_F(InputEventBrowserTest, MAYBE_TouchStartEventTimeStamp) {
  LoadURL(kEventListenerDataURL);

  MainThreadFrameObserver frame_observer(GetWidgetHost());

  base::TimeTicks event_time = base::TimeTicks::Now();
  int64_t event_time_ms = event_time.since_origin().InMilliseconds();
  SimulateSyntheticTouchTapAt(event_time);

  while (EvalJs(shell(), "eventCounts.touchstart") == 0) {
    frame_observer.Wait();
  }

  int64_t monotonic_time =
      EvalJs(shell(),
             "internals.zeroBasedDocumentTimeToMonotonicTime(eventTimeStamp."
             "touchstart)")
          .ExtractDouble();
  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.touchstart"));
  EXPECT_NEAR(event_time_ms, monotonic_time, 1);
}

IN_PROC_BROWSER_TEST_F(InputEventBrowserTest, ClickEventTimeStamp) {
  LoadURL(kEventListenerDataURL);

  MainThreadFrameObserver frame_observer(GetWidgetHost());

  base::TimeTicks event_time = base::TimeTicks::Now();
  int64_t event_time_ms = event_time.since_origin().InMilliseconds();
  SimulateSyntheticTouchTapAt(event_time);

  while (EvalJs(shell(), "eventCounts.click") == 0) {
    frame_observer.Wait();
  }

  int64_t monotonic_time =
      EvalJs(shell(),
             "internals.zeroBasedDocumentTimeToMonotonicTime(eventTimeStamp."
             "click)")
          .ExtractDouble();
  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.click"));
  EXPECT_NEAR(event_time_ms, monotonic_time, 1);
}

IN_PROC_BROWSER_TEST_F(InputEventBrowserTest, WheelEventTimeStamp) {
  LoadURL(kEventListenerDataURL);

  MainThreadFrameObserver frame_observer(GetWidgetHost());

  base::TimeTicks event_time = base::TimeTicks::Now();
  int64_t event_time_ms = event_time.since_origin().InMilliseconds();
  SimulateSyntheticWheelScroll(event_time);

  while (EvalJs(shell(), "eventCounts.wheel") == 0) {
    frame_observer.Wait();
  }

  int64_t monotonic_time =
      EvalJs(shell(),
             "internals.zeroBasedDocumentTimeToMonotonicTime(eventTimeStamp."
             "wheel)")
          .ExtractDouble();
  EXPECT_GE(EvalJs(shell(), "eventCounts.wheel").ExtractInt(), 1);
  EXPECT_NEAR(event_time_ms, monotonic_time, 1);
}

}  // namespace content
