// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/actions_parser.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_controller.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_action.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_features.h"
#include "ui/latency/latency_info.h"

using blink::WebInputEvent;

namespace {

constexpr char kTouchActionDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width'/>"
    "<style>"
    "html, body {"
    "  margin: 0;"
    "}"
    ".box {"
    "  height: 100px;"
    "  width: 100px;"
    "  background-color: red;"
    "}"
    ".ta-none {"
    "  touch-action: none;"
    "  background-color: green;"
    "}"
    ".spacer {"
    "  height: 10000px;"
    "  width: 100px;"
    "  background-color: blue;"
    "}"
    "</style>"
    "<div class=box></div>"
    "<div class='box ta-none'></div>"
    "<div class=spacer></div>"
    "<script>"
    "  window.eventCounts = "
    "    {touchstart:0, touchmove:0, touchend: 0, touchcancel:0};"
    "  function countEvent(e) { eventCounts[e.type]++; }"
    "  for (var evt in eventCounts) { "
    "    document.addEventListener(evt, countEvent); "
    "  }"
    "  document.title='ready';"
    "</script>";

constexpr char kTouchActionURLWithOverlapArea[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width'/>"
    "<style>"
    "html, body {"
    "  margin: 0;"
    "}"
    ".box {"
    "  box-sizing: border-box;"
    "  height: 100px;"
    "  width: 100px;"
    "  border: 2px solid blue;"
    "  position: absolute;"
    "  will-change: transform;"
    "}"
    ".spacer {"
    "  height: 10000px;"
    "  width: 10000px;"
    "}"
    ".ta-auto {"
    "  top: 52px;"
    "  left: 52px;"
    "  touch-action: auto;"
    "}"
    ".ta-pany {"
    "  top: 102px;"
    "  left: 2px;"
    "  touch-action: pan-y;"
    "}"
    ".ta-panx {"
    "  top: 2px;"
    "  left: 102px;"
    "  touch-action: pan-x;"
    "}"
    "</style>"
    "<div class='box ta-auto'></div>"
    "<div class='box ta-panx'></div>"
    "<div class='box ta-pany'></div>"
    "<div class=spacer></div>"
    "<script>"
    "  document.title='ready';"
    "</script>";

void GiveItSomeTime(int t) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(t));
  run_loop.Run();
}

constexpr base::TimeDelta kNoJankTime = base::Milliseconds(0);
constexpr base::TimeDelta kShortJankTime = base::Milliseconds(100);
// 1200ms is larger than both desktop / mobile_touch_ack_timeout_delay in the
// PassthroughTouchEventQueue, which ensures timeout to be triggered.
constexpr base::TimeDelta kLongJankTime = base::Milliseconds(1200);
}  // namespace

namespace content {

class TouchActionBrowserTest : public ContentBrowserTest {
 public:
  TouchActionBrowserTest() = default;

  TouchActionBrowserTest(const TouchActionBrowserTest&) = delete;
  TouchActionBrowserTest& operator=(const TouchActionBrowserTest&) = delete;

  ~TouchActionBrowserTest() override = default;

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    run_loop_->Quit();
  }

 protected:
  void LoadURL(std::string_view touch_action_url) {
    const GURL data_url(std::move(touch_action_url));
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
  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(switches::kTouchEventFeatureDetection,
                           switches::kTouchEventFeatureDetectionEnabled);
  }

  // ContentBrowserTest:
  void PostRunTestOnMainThread() override {
    // Delete this before the WebContents is destroyed.
    frame_observer_.reset();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  void JankMainThread(base::TimeDelta delta) {
    std::string script = "var end = performance.now() + ";
    script.append(base::NumberToString(delta.InMilliseconds()));
    script.append("; while (performance.now() < end) ; ");
    EXPECT_TRUE(ExecJs(shell(), script));
  }

  double GetScrollTop() {
    return EvalJs(shell(), "document.scrollingElement.scrollTop")
        .ExtractDouble();
  }

  double GetScrollLeft() {
    return EvalJs(shell(), "document.scrollingElement.scrollLeft")
        .ExtractDouble();
  }

  bool URLLoaded() {
    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    const std::u16string title = watcher.WaitAndGetTitle();
    return title == ready_title;
  }

  // In this test, we first jank the main thread for 1200ms. Then we let the
  // first finger scroll along the x-direction, on a pan-y area, for 1 second.
  // While the first finger is still scrolling, we let the second finger
  // touching the same area and scroll along the same direction. We purposely
  // trigger touch ack timeout for the first finger touch. All we need to ensure
  // is that the second finger also scrolled.
  // TODO(bokan): This test isn't doing what's described. For one thing, the
  // JankMainThread function will block the caller as well as the main thread
  // so we're actually waiting 1.8s before starting the second scroll, by which
  // point the first scroll has finished. Additionally, we can only run one
  // synthetic gesture at a time so queueing two gestures will produce
  // back-to-back scrolls rather than one two fingered scroll.
  void DoTwoFingerTouchScroll(
      bool wait_until_scrolled,
      const gfx::Vector2d& expected_scroll_position_after_scroll) {
    SyntheticSmoothScrollGestureParams params1;
    params1.gesture_source_type =
        content::mojom::GestureSourceType::kTouchInput;
    params1.anchor = gfx::PointF(25, 125);
    params1.distances.push_back(gfx::Vector2dF(-5, 0));
    params1.prevent_fling = true;
    params1.speed_in_pixels_s = 5;

    SyntheticSmoothScrollGestureParams params2;
    params2.gesture_source_type =
        content::mojom::GestureSourceType::kTouchInput;
    params2.anchor = gfx::PointF(25, 125);
    params2.distances.push_back(gfx::Vector2dF(-50, 0));

    run_loop_ = std::make_unique<base::RunLoop>();

    GetWidgetHost()->QueueSyntheticGesture(
        std::make_unique<SyntheticSmoothScrollGesture>(params1),
        base::DoNothing());

    JankMainThread(kLongJankTime);
    GiveItSomeTime(800);

    GetWidgetHost()->QueueSyntheticGesture(
        std::make_unique<SyntheticSmoothScrollGesture>(params2),
        base::BindOnce(&TouchActionBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    run_loop_->Run();
    run_loop_.reset();

    CheckScrollOffset(wait_until_scrolled,
                      expected_scroll_position_after_scroll);
  }

  // Generate touch events for a synthetic scroll from |point| for |distance|.
  void DoTouchScrollAndCheckScrollHeight(
      const gfx::Point& point,
      const gfx::Vector2d& distance,
      bool wait_until_scrolled,
      int expected_scroll_height_after_scroll,
      const gfx::Vector2d& expected_scroll_position_after_scroll,
      const base::TimeDelta& jank_time) {
    EXPECT_EQ(expected_scroll_height_after_scroll,
              EvalJs(shell(), "document.documentElement.scrollHeight"));
    DoTouchScroll(point, distance, wait_until_scrolled,
                  expected_scroll_position_after_scroll, jank_time);
  }

  void DoTouchScroll(const gfx::Point& point,
                     const gfx::Vector2d& distance,
                     bool wait_until_scrolled,
                     const gfx::Vector2d& expected_scroll_position_after_scroll,
                     const base::TimeDelta& jank_time) {
    DCHECK(URLLoaded());
    EXPECT_EQ(0, GetScrollTop());

    EnsureInitializedForSyntheticGestures();

    float page_scale_factor =
        frame_observer_->LastRenderFrameMetadata().page_scale_factor;
    if (page_scale_factor == 0)
      page_scale_factor = 1.0f;
    gfx::PointF touch_point(point);
    if (page_scale_factor != 1.0f) {
      touch_point.set_x(touch_point.x() * page_scale_factor);
      touch_point.set_y(touch_point.y() * page_scale_factor);
    }
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
    params.anchor = touch_point;
    params.distances.push_back(-distance);
    // Set the speed to very high so that there is one GSU only.
    // It seems that when the speed is too high, it has a race with the timeout
    // test.
    if (jank_time != kLongJankTime) {
      params.speed_in_pixels_s = 1000000;
    }

    run_loop_ = std::make_unique<base::RunLoop>();

    GetWidgetHost()->QueueSyntheticGesture(
        std::make_unique<SyntheticSmoothScrollGesture>(params),
        base::BindOnce(&TouchActionBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    if (jank_time > base::Milliseconds(0))
      JankMainThread(jank_time);

    // Runs until we get the OnSyntheticGestureCompleted callback
    run_loop_->Run();
    run_loop_.reset();

    CheckScrollOffset(wait_until_scrolled,
                      expected_scroll_position_after_scroll);
  }

  void DoTwoFingerPan() {
    DCHECK(URLLoaded());

    const std::string pointer_actions_json = R"HTML(
        [{"source": "touch", "id": 0,
              "actions": [
                { "name": "pointerDown", "x": 10, "y": 125 },
                { "name": "pointerMove", "x": 10, "y": 155 },
                { "name": "pointerUp" }]},
             {"source": "touch", "id": 1,
              "actions": [
                { "name": "pointerDown", "x": 15, "y": 125 },
                { "name": "pointerMove", "x": 15, "y": 155 },
                { "name": "pointerUp"}]}]
        )HTML";

    ASSERT_OK_AND_ASSIGN(
        auto parsed_json,
        base::JSONReader::ReadAndReturnValueWithError(pointer_actions_json));
    ActionsParser actions_parser(std::move(parsed_json));

    ASSERT_TRUE(actions_parser.Parse());

    run_loop_ = std::make_unique<base::RunLoop>();

    GetWidgetHost()->QueueSyntheticGesture(
        std::make_unique<SyntheticPointerAction>(
            actions_parser.pointer_action_params()),
        base::BindOnce(&TouchActionBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    run_loop_->Run();
    run_loop_.reset();
  }

  // Generate touch events for a double tap and drag zoom gesture at
  // coordinates (50, 50).
  void DoDoubleTapDragZoom() {
    DCHECK(URLLoaded());

    const std::string pointer_actions_json = R"HTML(
        [{
          "source": "touch",
          "actions": [
            { "name": "pointerDown", "x": 50, "y": 50 },
            { "name": "pointerUp" },
            { "name": "pause", "duration": 50 },
            { "name": "pointerDown", "x": 50, "y": 50 },
            { "name": "pointerMove", "x": 50, "y": 150 },
            { "name": "pointerUp" }
          ]
        }]
        )HTML";

    UNSAFE_BUFFERS(ASSERT_OK_AND_ASSIGN(
        auto parsed_json,
        base::JSONReader::ReadAndReturnValueWithError(pointer_actions_json)));
    ActionsParser actions_parser(std::move(parsed_json));

    ASSERT_TRUE(actions_parser.Parse());

    run_loop_ = std::make_unique<base::RunLoop>();

    GetWidgetHost()->QueueSyntheticGesture(
        std::make_unique<SyntheticPointerAction>(
            actions_parser.pointer_action_params()),
        base::BindOnce(&TouchActionBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    run_loop_->Run();
    run_loop_.reset();
  }

  // Sends a no-op gesture to the page to ensure the SyntheticGestureController
  // is initialized. We need to do this if the first sent gesture happens when
  // the main thread is janked, otherwise the initialization won't happen
  // because of the blocked main thread.
  void EnsureInitializedForSyntheticGestures() {
    DCHECK(URLLoaded());

    base::RunLoop run_loop;

    GetWidgetHost()->EnsureReadyForSyntheticGestures(
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    // Runs until we get the OnSyntheticGestureCompleted callback
    run_loop.Run();
  }

  void CheckScrollOffset(
      bool wait_until_scrolled,
      const gfx::Vector2d& expected_scroll_position_after_scroll) {
    gfx::PointF default_scroll_offset;
    gfx::PointF root_scroll_offset =
        frame_observer_->LastRenderFrameMetadata().root_scroll_offset.value_or(
            default_scroll_offset);

    int scroll_top, scroll_left;
    if (!wait_until_scrolled) {
      scroll_top = root_scroll_offset.y();
      scroll_left = root_scroll_offset.x();
    } else {
      // GetScrollTop() and GetScrollLeft() goes through the main thread, here
      // we want to make sure that the compositor already scrolled before asking
      // the main thread.
      while (root_scroll_offset.y() <
                 expected_scroll_position_after_scroll.y() / 2 ||
             root_scroll_offset.x() <
                 expected_scroll_position_after_scroll.x() / 2) {
        frame_observer_->WaitForMetadataChange();
        root_scroll_offset =
            frame_observer_->LastRenderFrameMetadata()
                .root_scroll_offset.value_or(default_scroll_offset);
      }
      // Check the scroll offset
      scroll_top = GetScrollTop();
      scroll_left = GetScrollLeft();
    }

    // It seems that even if the compositor frame has scrolled half of the
    // expected scroll offset, the Blink side scroll offset may not yet be
    // updated, so here we expect it to at least have scrolled.
    // TODO(crbug.com/40601223): this can be resolved by fixing this bug.
    if (expected_scroll_position_after_scroll.y() > 0)
      EXPECT_GT(scroll_top, 0);
    if (expected_scroll_position_after_scroll.x() > 0)
      EXPECT_GT(scroll_left, 0);
  }

 private:
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_DefaultAuto DISABLED_DefaultAuto
#else
#define MAYBE_DefaultAuto DefaultAuto
#endif
//
// Verify the test infrastructure works - we can touch-scroll the page and get a
// touchcancel as expected.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_DefaultAuto) {
  LoadURL(kTouchActionDataURL);

  bool wait_until_scrolled = false;
  DoTouchScrollAndCheckScrollHeight(gfx::Point(50, 50), gfx::Vector2d(0, 45),
                                    wait_until_scrolled, 10200,
                                    gfx::Vector2d(0, 45), kNoJankTime);

  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.touchstart"));
  EXPECT_GE(EvalJs(shell(), "eventCounts.touchmove").ExtractInt(), 1);
  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.touchend"));
  EXPECT_EQ(0, EvalJs(shell(), "eventCounts.touchcancel"));
}

// Verify that touching a touch-action: none region disables scrolling and
// enables all touch events to be sent.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER)
#define MAYBE_TouchActionNone DISABLED_TouchActionNone
#else
#define MAYBE_TouchActionNone TouchActionNone
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_TouchActionNone) {
  LoadURL(kTouchActionDataURL);

  bool wait_until_scrolled = false;
  DoTouchScrollAndCheckScrollHeight(gfx::Point(50, 150), gfx::Vector2d(0, 45),
                                    wait_until_scrolled, 10200,
                                    gfx::Vector2d(0, 0), kNoJankTime);

  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.touchstart"));
  EXPECT_GE(EvalJs(shell(), "eventCounts.touchmove").ExtractInt(), 1);
  EXPECT_EQ(1, EvalJs(shell(), "eventCounts.touchend"));
  EXPECT_EQ(0, EvalJs(shell(), "eventCounts.touchcancel"));
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanYMainThreadJanky DISABLED_PanYMainThreadJanky
#else
#define MAYBE_PanYMainThreadJanky PanYMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_PanYMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  bool wait_until_scrolled = false;
  DoTouchScrollAndCheckScrollHeight(gfx::Point(25, 125), gfx::Vector2d(0, 45),
                                    wait_until_scrolled, 10000,
                                    gfx::Vector2d(0, 45), kShortJankTime);
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanXMainThreadJanky DISABLED_PanXMainThreadJanky
#else
#define MAYBE_PanXMainThreadJanky PanXMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_PanXMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  bool wait_until_scrolled = false;
  DoTouchScrollAndCheckScrollHeight(gfx::Point(125, 25), gfx::Vector2d(45, 0),
                                    wait_until_scrolled, 10000,
                                    gfx::Vector2d(45, 0), kShortJankTime);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PanXAtYAreaWithTimeout PanXAtYAreaWithTimeout
#else
#define MAYBE_PanXAtYAreaWithTimeout DISABLED_PanXAtYAreaWithTimeout
#endif
// When touch ack timeout is triggered, the panx gesture will be allowed even
// though we touch the pany area.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_PanXAtYAreaWithTimeout) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTouchScrollAndCheckScrollHeight(gfx::Point(25, 125), gfx::Vector2d(45, 0),
                                    true, 10000, gfx::Vector2d(45, 0),
                                    kLongJankTime);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TwoFingerPanXAtYAreaWithTimeout TwoFingerPanXAtYAreaWithTimeout
#else
#define MAYBE_TwoFingerPanXAtYAreaWithTimeout \
  DISABLED_TwoFingerPanXAtYAreaWithTimeout
#endif
// When touch ack timeout is triggered, the panx gesture will be allowed even
// though we touch the pany area.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest,
                       MAYBE_TwoFingerPanXAtYAreaWithTimeout) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTwoFingerTouchScroll(true, gfx::Vector2d(20, 0));
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanXYMainThreadJanky DISABLED_PanXYMainThreadJanky
#else
#define MAYBE_PanXYMainThreadJanky PanXYMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_PanXYMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  bool wait_until_scrolled = false;
  DoTouchScrollAndCheckScrollHeight(gfx::Point(75, 60), gfx::Vector2d(45, 45),
                                    wait_until_scrolled, 10000,
                                    gfx::Vector2d(45, 45), kShortJankTime);
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanXYAtXAreaMainThreadJanky DISABLED_PanXYAtXAreaMainThreadJanky
#else
#define MAYBE_PanXYAtXAreaMainThreadJanky PanXYAtXAreaMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest,
                       MAYBE_PanXYAtXAreaMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTouchScrollAndCheckScrollHeight(gfx::Point(125, 25), gfx::Vector2d(45, 20),
                                    true, 10000, gfx::Vector2d(45, 0),
                                    kShortJankTime);
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanXYAtYAreaMainThreadJanky DISABLED_PanXYAtYAreaMainThreadJanky
#else
#define MAYBE_PanXYAtYAreaMainThreadJanky PanXYAtYAreaMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest,
                       MAYBE_PanXYAtYAreaMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTouchScrollAndCheckScrollHeight(gfx::Point(25, 125), gfx::Vector2d(20, 45),
                                    true, 10000, gfx::Vector2d(0, 45),
                                    kShortJankTime);
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanXYAtAutoYOverlapAreaMainThreadJanky \
  DISABLED_PanXYAtAutoYOverlapAreaMainThreadJanky
#else
#define MAYBE_PanXYAtAutoYOverlapAreaMainThreadJanky \
  PanXYAtAutoYOverlapAreaMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest,
                       MAYBE_PanXYAtAutoYOverlapAreaMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTouchScrollAndCheckScrollHeight(gfx::Point(75, 125), gfx::Vector2d(20, 45),
                                    true, 10000, gfx::Vector2d(0, 45),
                                    kShortJankTime);
}

// TODO(crbug.com/40236573): Fix Mac failures.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||       \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_PanXYAtAutoXOverlapAreaMainThreadJanky \
  DISABLED_PanXYAtAutoXOverlapAreaMainThreadJanky
#else
#define MAYBE_PanXYAtAutoXOverlapAreaMainThreadJanky \
  PanXYAtAutoXOverlapAreaMainThreadJanky
#endif
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest,
                       MAYBE_PanXYAtAutoXOverlapAreaMainThreadJanky) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTouchScrollAndCheckScrollHeight(gfx::Point(125, 75), gfx::Vector2d(45, 20),
                                    true, 10000, gfx::Vector2d(45, 0),
                                    kShortJankTime);
}

// TODO(crbug.com/41422733): Make this test work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TwoFingerPanYDisallowed DISABLED_TwoFingerPanYDisallowed
#else
#define MAYBE_TwoFingerPanYDisallowed TwoFingerPanYDisallowed
#endif
// Test that two finger panning is treated as pinch zoom and is disallowed when
// touching the pan-y area.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, MAYBE_TwoFingerPanYDisallowed) {
  LoadURL(kTouchActionURLWithOverlapArea);

  DoTwoFingerPan();
  CheckScrollOffset(true, gfx::Vector2d(0, 0));
}

namespace {

const std::string kDoubleTapZoomDataURL = R"HTML(
    data:text/html,<!DOCTYPE html>
    <meta name='viewport' content='width=device-width'/>
    <style>
      html, body {
        margin: 0;
      }
      .spacer { height: 10000px; }
      .touchaction { width: 75px; height: 75px; touch-action: none; }
    </style>
    <div class="touchaction"></div>
    <div class=spacer></div>
    <script>
      document.title='ready';
    </script>)HTML";

}  // namespace

// Test that |touch-action: none| correctly blocks a double-tap and drag zoom
// gesture.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTest, BlockDoubleTapDragZoom) {
  LoadURL(kDoubleTapZoomDataURL.c_str());

  ASSERT_EQ(1, EvalJs(shell(), "window.visualViewport.scale"));

  DoDoubleTapDragZoom();

  EXPECT_EQ(1, EvalJs(shell(), "window.visualViewport.scale"));
}

namespace {

constexpr char kContentEditableDataURL[] = R"HTML(
    data:text/html,<!DOCTYPE html>
    <meta name='viewport' content='width=device-width'/>
    <style>
    html, body {
      margin: 0;
    }
    </style>
    <div id='container' contenteditable style='height: 200px'>
      11111111111111111111111111111111
    </div>
    <div class=spacer style='height: 10000px'></div>
    <script>
      let container = document.getElementById('container');
      container.focus();
      let textNode = container.childNodes[0];
      window.getSelection().setBaseAndExtent(textNode, 32, textNode, 32);
      document.title='ready';
    </script>)HTML";

constexpr char kContentEditableHorizontalScrollableDataURL[] = R"HTML(
    data:text/html,<!DOCTYPE html>
    <meta name='viewport' content='width=device-width'/>
    <style>
    html, body {
      margin: 0;
    }
    %23scroller {
      height: 220px;
      width: 100px;
      white-space: nowrap;
      overflow-x: scroll;
    }
    %23container {
      height: 200px;
      width: 500px;
      display: inline-block;
    }
    </style>
    <div id="scroller">
      <div id='container' contenteditable>
        11111111111111111111111111111111
      </div>
    </div>
    <div class=spacer style='height: 10000px'></div>
    <script>
      let container = document.getElementById('container');
      container.focus();
      let textNode = container.childNodes[0];
      window.getSelection().setBaseAndExtent(textNode, 32, textNode, 32);
      document.title='ready';
    </script>)HTML";

constexpr char kContentEditableNonPassiveHandlerDataURL[] = R"HTML(
    data:text/html,<!DOCTYPE html>
    <meta name='viewport' content='width=device-width'/>
    <style>
    html, body {
      margin: 0;
    }
    </style>
    <div id='container' contenteditable style='height: 200px'>
      11111111111111111111111111111111
    </div>
    <div class=spacer style='height: 10000px'></div>
    <script>
      let container = document.getElementById('container');
      container.focus();
      let textNode = container.childNodes[0];
      window.getSelection().setBaseAndExtent(textNode, 32, textNode, 32);
      container.addEventListener("touchstart", function(event) {
        event.preventDefault();
      }, {passive: false});
      document.title='ready';
    </script>)HTML";

constexpr char kInputTagCursorControl[] = R"HTML(
    data:text/html,<!DOCTYPE html>
    <meta name='viewport' content='width=device-width'/>
    <style>
    html, body {
      margin: 0;
    }
    input {
      height: 20px;
      padding: 0px;
      margin: 0px;
      border: 0px;
    }
    </style>
    <input type="text" id="container" value="11111111111111111111111111111111"
      size=%d>
    <div class=spacer style='height: 10000px'></div>
    <script>
      let container = document.getElementById('container');
      container.focus();
      container.setSelectionRange(32, 32);
      document.title='ready';
    </script>)HTML";

}  // namespace

class TouchActionBrowserTestEnableCursorControl
    : public TouchActionBrowserTest {
 public:
  TouchActionBrowserTestEnableCursorControl() {
    feature_list_.InitWithFeatures({::features::kSwipeToMoveCursor}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Perform a horizontal swipe over an editable element from right to left.
// Ensure the swipe is interpreted as a cursor control movement, rather than a
// scroll, and changes the selection.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTestEnableCursorControl,
                       BasicCursorControl) {
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  LoadURL(kContentEditableDataURL);

  EXPECT_EQ(32, EvalJs(shell(), "window.getSelection().anchorOffset"));
  EXPECT_EQ(32, EvalJs(shell(), "window.getSelection().focusOffset"));

  DoTouchScroll(gfx::Point(85, 5), gfx::Vector2d(40, 0),
                /* wait_until_scrolled*/ false, gfx::Vector2d(0, 0),
                kNoJankTime);

  const int anchor_offset =
      EvalJs(shell(), "window.getSelection().anchorOffset").ExtractInt();

  EXPECT_EQ(anchor_offset,
            EvalJs(shell(), "window.getSelection().focusOffset"));
  EXPECT_GT(32, anchor_offset);
}

// Perform a horizontal swipe over an editable element from right to left (the
// element shift to left), the element is inside of a horizontal scroller.
// Ensure the swipe is interpreted as a normal scroll, selection should not be
// changed and scroll should happen.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTestEnableCursorControl,
                       NoCursorControlForHorizontalScrollable) {
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  LoadURL(kContentEditableHorizontalScrollableDataURL);

  EXPECT_EQ(32, EvalJs(shell(), "window.getSelection().anchorOffset"));
  EXPECT_EQ(32, EvalJs(shell(), "window.getSelection().focusOffset"));

  DoTouchScroll(gfx::Point(85, 5), gfx::Vector2d(40, 0),
                /* wait_until_scrolled*/ false, gfx::Vector2d(0, 0),
                kNoJankTime);

  const int anchor_offset =
      EvalJs(shell(), "window.getSelection().anchorOffset").ExtractInt();

  EXPECT_EQ(anchor_offset,
            EvalJs(shell(), "window.getSelection().focusOffset"));
  EXPECT_EQ(32, anchor_offset);
  EXPECT_LT(0.f,
            EvalJs(shell(), "document.getElementById('scroller').scrollLeft")
                .ExtractDouble());
}

// Perform a horizontal swipe over an editable element from right to left
// Ensure the swipe is not triggering cursor control.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTestEnableCursorControl,
                       NoCursorControlForNonPassiveLisenter) {
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  LoadURL(kContentEditableNonPassiveHandlerDataURL);

  EXPECT_EQ(32, EvalJs(shell(), "window.getSelection().anchorOffset"));
  EXPECT_EQ(32, EvalJs(shell(), "window.getSelection().focusOffset"));

  DoTouchScroll(gfx::Point(85, 5), gfx::Vector2d(40, 0),
                /* wait_until_scrolled*/ false, gfx::Vector2d(0, 0),
                kNoJankTime);

  const int anchor_offset =
      EvalJs(shell(), "window.getSelection().anchorOffset").ExtractInt();

  EXPECT_EQ(anchor_offset,
            EvalJs(shell(), "window.getSelection().focusOffset"));
  EXPECT_EQ(32, anchor_offset);
}

// Perform a horizontal swipe over an input element from right to left.
// Ensure the swipe is interpreted as a cursor control movement, rather than a
// scroll, and changes the selection.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTestEnableCursorControl,
                       CursorControlOnInput) {
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  // input size larger than the text size, not horizontally scrollable.
  LoadURL(base::StringPrintf(kInputTagCursorControl, 40).c_str());

  EXPECT_EQ(32, EvalJs(shell(), "container.selectionStart"));
  EXPECT_EQ(32, EvalJs(shell(), "container.selectionEnd"));

  DoTouchScroll(gfx::Point(85, 5), gfx::Vector2d(40, 0),
                /* wait_until_scrolled*/ false, gfx::Vector2d(0, 0),
                kNoJankTime);

  const int selection_start =
      EvalJs(shell(), "container.selectionStart").ExtractInt();

  EXPECT_EQ(selection_start, EvalJs(shell(), "container.selectionEnd"));
  EXPECT_GT(32, selection_start);
}

// Perform a horizontal swipe over an horizontal scrollable input element from
// right to left. Ensure the swipe is doing scrolling other than cursor control.
IN_PROC_BROWSER_TEST_F(TouchActionBrowserTestEnableCursorControl,
                       NoCursorControlOnHorizontalScrollableInput) {
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  // Make the input size smaller than the text size, so it horizontally
  // scrollable.
  LoadURL(base::StringPrintf(kInputTagCursorControl, 20).c_str());

  EXPECT_EQ(32, EvalJs(shell(), "container.selectionStart"));
  EXPECT_EQ(32, EvalJs(shell(), "container.selectionEnd"));

  DoTouchScroll(gfx::Point(85, 5), gfx::Vector2d(40, 0),
                /* wait_until_scrolled*/ false, gfx::Vector2d(0, 0),
                kNoJankTime);

  const int selection_start =
      EvalJs(shell(), "container.selectionStart").ExtractInt();

  EXPECT_EQ(selection_start, EvalJs(shell(), "container.selectionEnd"));
  EXPECT_EQ(32, selection_start);
}

}  // namespace content
