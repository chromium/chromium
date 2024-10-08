// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/input/input_router_impl.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/touch_action_filter.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/input/touch_emulator_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/input/synthetic_smooth_drag_gesture.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/switches.h"
#include "ui/latency/latency_info.h"

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#endif

namespace content {

namespace {

// Test observer which waits for a visual properties update from a
// `RenderWidgetHost`.
class TestRenderWidgetHostObserver : public RenderWidgetHostObserver {
 public:
  explicit TestRenderWidgetHostObserver(RenderWidgetHost* widget_host)
      : widget_host_(widget_host) {
    widget_host_->AddObserver(this);
  }

  ~TestRenderWidgetHostObserver() override {
    widget_host_->RemoveObserver(this);
  }

  // RenderWidgetHostObserver:
  void RenderWidgetHostDidUpdateVisualProperties(
      RenderWidgetHost* widget_host) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  raw_ptr<RenderWidgetHost> widget_host_ = nullptr;
  base::RunLoop run_loop_;
};

}  // namespace

// For tests that just need a browser opened/navigated to a simple web page.
class RenderWidgetHostBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(NavigateToURL(
        shell(), GURL("data:text/html,<!doctype html>"
                      "<body style='background-color: magenta;'></body>")));
  }

  WebContents* web_contents() const { return shell()->web_contents(); }
  RenderWidgetHostViewBase* view() const {
    return static_cast<RenderWidgetHostViewBase*>(
        web_contents()->GetRenderWidgetHostView());
  }
  RenderWidgetHostImpl* host() const {
    return static_cast<RenderWidgetHostImpl*>(view()->GetRenderWidgetHost());
  }

  void WaitForVisualPropertiesAck() {
    while (host()->visual_properties_ack_pending_for_testing()) {
      TestRenderWidgetHostObserver(host()).Wait();
    }
  }
};

// This test enables --site-per-process flag.
class RenderWidgetHostSitePerProcessTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
    // Slow bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  input::TouchActionFilter* GetTouchActionFilterForWidget(
      RenderWidgetHostImpl* rwhi) {
    return &static_cast<input::InputRouterImpl*>(rwhi->input_router())
                ->touch_action_filter_;
  }
};

class TestInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  using EventTypeVector = std::vector<blink::WebInputEvent::Type>;

  ~TestInputEventObserver() override {}

  void OnInputEvent(const blink::WebInputEvent& event) override {
    dispatched_events_.push_back(event.GetType());
  }

  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent& event) override {
    if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
      acked_touch_event_type_ = event.GetType();
    }
  }

  EventTypeVector GetAndResetDispatchedEventTypes() {
    EventTypeVector new_event_types;
    std::swap(new_event_types, dispatched_events_);
    return new_event_types;
  }

  blink::WebInputEvent::Type acked_touch_event_type() const {
    return acked_touch_event_type_;
  }

 private:
  EventTypeVector dispatched_events_;
  blink::WebInputEvent::Type acked_touch_event_type_ =
      blink::WebInputEvent::Type::kUndefined;
};

class RenderWidgetHostTouchEmulatorBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostTouchEmulatorBrowserTest()
      : last_simulated_event_time_(ui::EventTimeForNow()),
        simulated_event_time_delta_(base::Milliseconds(100)) {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    EXPECT_TRUE(NavigateToURL(
        shell(), GURL("data:text/html,<!doctype html>"
                      "<body style='background-color: red;'></body>")));
  }

  base::TimeTicks GetNextSimulatedEventTime() {
    last_simulated_event_time_ += simulated_event_time_delta_;
    return last_simulated_event_time_;
  }

  void SimulateRoutedMouseEvent(blink::WebInputEvent::Type type,
                                int x,
                                int y,
                                int modifiers,
                                bool pressed) {
    blink::WebMouseEvent event =
        blink::SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed) {
      event.button = blink::WebMouseEvent::Button::kLeft;
    }
    event.SetTimeStamp(GetNextSimulatedEventTime());
    input::RenderWidgetHostInputEventRouter* router =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetInputEventRouter();
    ASSERT_TRUE(router);
    router->RouteMouseEvent(view(), &event, ui::LatencyInfo());
  }

  void WaitForAckWith(blink::WebInputEvent::Type type) {
    InputMsgWatcher watcher(host(), type);
    watcher.GetAckStateWaitIfNecessary();
  }

  RenderWidgetHostImpl* host() {
    return static_cast<RenderWidgetHostImpl*>(view()->GetRenderWidgetHost());
  }

  RenderWidgetHostViewBase* view() {
    return static_cast<RenderWidgetHostViewBase*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

 private:
  base::TimeTicks last_simulated_event_time_;
  const base::TimeDelta simulated_event_time_delta_;
};

// Synthetic mouse events not allowed on Android.
#if !BUILDFLAG(IS_ANDROID)
// This test makes sure that TouchEmulator doesn't emit a GestureScrollEnd
// without a valid unique_touch_event_id when it sees a GestureFlingStart
// terminating the underlying mouse scroll sequence. If the GestureScrollEnd is
// given a unique_touch_event_id of 0, then a crash will occur.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostTouchEmulatorBrowserTest,
                       TouchEmulatorPinchWithGestureFling) {
  auto* touch_emulator = host()->GetTouchEmulator(/*create_if_necessary=*/true);
  touch_emulator->Enable(input::TouchEmulator::Mode::kEmulatingTouchFromMouse,
                         ui::GestureProviderConfigType::GENERIC_MOBILE);
  touch_emulator->SetPinchGestureModeForTesting(true);

  TestInputEventObserver observer;
  host()->AddInputEventObserver(&observer);

  SyntheticSmoothDragGestureParams params;
  params.start_point = gfx::PointF(10.f, 110.f);
  params.gesture_source_type = content::mojom::GestureSourceType::kMouseInput;
  params.distances.push_back(gfx::Vector2d(0, -10));
  params.distances.push_back(gfx::Vector2d(0, -10));
  params.distances.push_back(gfx::Vector2d(0, -10));
  params.distances.push_back(gfx::Vector2d(0, -10));
  params.speed_in_pixels_s = 1200;

  // On slow bots (e.g. ChromeOS DBG) the synthetic gesture sequence events may
  // be delivered slowly/erratically-timed so that the velocity_tracker in the
  // TouchEmulator's GestureDetector may either (i) drop some scroll updates
  // from the velocity estimate, or (ii) create an unexpectedly low velocity
  // estimate. In either case, the minimum fling start velocity may not be
  // achieved, meaning the condition we're trying to test never occurs. To
  // avoid that, we'll keep trying until it happens. The failure mode for the
  // test is that it times out.
  do {
    std::unique_ptr<SyntheticSmoothDragGesture> gesture(
        new SyntheticSmoothDragGesture(params));

    InputEventAckWaiter scroll_end_ack_waiter(
        host(), blink::WebInputEvent::Type::kGestureScrollEnd);
    base::RunLoop run_loop;
    host()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(
            base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
              EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
              run_loop.Quit();
            })));
    run_loop.Run();
    scroll_end_ack_waiter.Wait();

    // Verify that a GestureFlingStart was suppressed by the TouchEmulator, and
    // that we generated a GestureScrollEnd and routed it without crashing.
    TestInputEventObserver::EventTypeVector dispatched_events =
        observer.GetAndResetDispatchedEventTypes();
    EXPECT_TRUE(base::Contains(dispatched_events,
                               blink::WebInputEvent::Type::kGestureScrollEnd));
  } while (!touch_emulator->suppress_next_fling_cancel_for_testing());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Todo(crbug.com/994353): The test is flaky(crash/timeout) on MSAN, TSAN, and
// DEBUG builds.
#if (!defined(NDEBUG) || defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER))
#define MAYBE_TouchEmulator DISABLED_TouchEmulator
#else
#define MAYBE_TouchEmulator TouchEmulator
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostTouchEmulatorBrowserTest,
                       MAYBE_TouchEmulator) {
  host()
      ->GetTouchEmulator(/*create_if_necessary=*/true)
      ->Enable(input::TouchEmulator::Mode::kEmulatingTouchFromMouse,
               ui::GestureProviderConfigType::GENERIC_MOBILE);

  TestInputEventObserver observer;
  host()->AddInputEventObserver(&observer);

  // Simulate a mouse move without any pressed buttons. This should not
  // generate any touch events.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 120, 0,
                           false);
  TestInputEventObserver::EventTypeVector dispatched_events =
      observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(0u, dispatched_events.size());

  // Mouse press becomes touch start which in turn becomes tap.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseDown, 10, 120, 0,
                           true);
  WaitForAckWith(blink::WebInputEvent::Type::kTouchStart);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown, dispatched_events[1]);

  // Mouse drag generates touch move, cancels tap and starts scroll.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 100, 0,
                           true);
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(4u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapCancel,
            dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            dispatched_events[2]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[3]);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());
  EXPECT_EQ(0u, observer.GetAndResetDispatchedEventTypes().size());

  // Mouse drag with shift becomes pinch.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 95,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchBegin,
            dispatched_events[1]);

  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 80,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchUpdate,
            dispatched_events[1]);

  // Mouse drag without shift becomes scroll again.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 70, 0,
                           true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(3u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchEnd, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[2]);

  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 60, 0,
                           true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[1]);

  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseUp, 10, 60, 0,
                           true);
  WaitForAckWith(blink::WebInputEvent::Type::kTouchEnd);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchEnd,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchEnd, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            dispatched_events[1]);

  // Mouse move does nothing.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 50, 0,
                           false);
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(0u, dispatched_events.size());

  // Another mouse down continues scroll.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseDown, 10, 50, 0,
                           true);
  WaitForAckWith(blink::WebInputEvent::Type::kTouchStart);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown, dispatched_events[1]);
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 30, 0,
                           true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(4u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapCancel,
            dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            dispatched_events[2]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[3]);
  EXPECT_EQ(0u, observer.GetAndResetDispatchedEventTypes().size());

  // Another pinch.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 20,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchBegin,
            dispatched_events[1]);
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 10,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchUpdate,
            dispatched_events[1]);

  // Turn off emulation during a pinch.
  host()->GetTouchEmulator(/*create_if_necessary=*/true)->Disable();
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchCancel,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(3u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchCancel, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGesturePinchEnd, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            dispatched_events[2]);

  // Mouse event should pass untouched.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 120,
                           blink::WebInputEvent::kShiftKey, true);
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(1u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kMouseMove, dispatched_events[0]);

  // Turn on emulation.
  host()
      ->GetTouchEmulator(/*create_if_necessary=*/true)
      ->Enable(input::TouchEmulator::Mode::kEmulatingTouchFromMouse,
               ui::GestureProviderConfigType::GENERIC_MOBILE);

  // Another touch.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseDown, 10, 120, 0,
                           true);
  WaitForAckWith(blink::WebInputEvent::Type::kTouchStart);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchStart, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapDown, dispatched_events[1]);

  // Scroll.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 100, 0,
                           true);
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(4u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureTapCancel,
            dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            dispatched_events[2]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[3]);
  EXPECT_EQ(0u, observer.GetAndResetDispatchedEventTypes().size());

  // Turn off emulation during a scroll.
  host()->GetTouchEmulator(/*create_if_necessary=*/true)->Disable();
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchCancel,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::Type::kTouchCancel, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            dispatched_events[1]);

  host()->RemoveInputEventObserver(&observer);
}

// Observes the WebContents until a frame finishes loading the contents of a
// given GURL.
class DocumentLoadObserver : WebContentsObserver {
 public:
  DocumentLoadObserver(WebContents* contents, const GURL& url)
      : WebContentsObserver(contents), document_origin_(url) {}

  DocumentLoadObserver(const DocumentLoadObserver&) = delete;
  DocumentLoadObserver& operator=(const DocumentLoadObserver&) = delete;

  void Wait() {
    if (loaded_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void DidFinishLoad(RenderFrameHost* rfh, const GURL& url) override {
    loaded_ |= (url == document_origin_);
    if (loaded_ && run_loop_) {
      run_loop_->Quit();
    }
  }

  bool loaded_ = false;
  const GURL document_origin_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// This test verifies that when a cross-process child frame loads, the initial
// updates for touch event handlers are sent from the renderer.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostSitePerProcessTest,
                       OnHasTouchEventHandlers) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL::Replacements replacement;
  replacement.SetHostStr("b.com");
  replacement.SetQueryStr("b()");
  GURL target_child_url = main_url.ReplaceComponents(replacement);
  DocumentLoadObserver child_frame_observer(shell()->web_contents(),
                                            target_child_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  child_frame_observer.Wait();
  auto* filter = GetTouchActionFilterForWidget(web_contents()
                                                   ->GetPrimaryFrameTree()
                                                   .root()
                                                   ->child_at(0)
                                                   ->current_frame_host()
                                                   ->GetRenderWidgetHost());
  EXPECT_TRUE(filter->allowed_touch_action().has_value());
}

// The plumbing that this test is verifying is not utilized on Mac/Android,
// where popup menus don't create a popup RenderWidget, but rather they trigger
// a FrameHostMsg_ShowPopup to ask the browser to build and display the actual
// popup using native controls.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)

namespace {

// Helper to use inside a loop instead of using RunLoop::RunUntilIdle() to avoid
// the loop being a busy loop that prevents renderer from doing its job. Use
// only when there is no better way to synchronize.
void GiveItSomeTime(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(RenderWidgetHostSitePerProcessTest,
                       BrowserClosesSelectPopup) {
  // Navigate to a page with a <select> element.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* root_frame_host = root->current_frame_host();
  RenderProcessHost* process = root_frame_host->GetProcess();

  // Open the <select> menu by focusing it and sending a space key
  // at the focused node. This creates a popup widget.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.text[0] = ' ';

  for (int i = 0; i < 2; ++i) {
    bool browser_closes = i == 0;

    // This focuses and opens the select box, creating a popup RenderWidget. We
    // wait for the RenderWidgetHost to be shown.
    auto filter =
        std::make_unique<ShowPopupWidgetWaiter>(contents, root_frame_host);
    EXPECT_TRUE(ExecJs(root_frame_host, "focusSelectMenu();"));
    root_frame_host->GetRenderWidgetHost()->ForwardKeyboardEvent(event);
    filter->Wait();

    // The popup RenderWidget will get its own routing id.
    int popup_routing_id = filter->last_routing_id();
    EXPECT_TRUE(popup_routing_id);
    // Grab a pointer to the popup RenderWidget.
    RenderWidgetHost* popup_widget_host =
        RenderWidgetHost::FromID(process->GetID(), popup_routing_id);
    ASSERT_TRUE(popup_widget_host);
    ASSERT_NE(popup_widget_host, root_frame_host->GetRenderWidgetHost());

    auto* popup_widget_host_impl =
        static_cast<RenderWidgetHostImpl*>(popup_widget_host);
    if (browser_closes) {
      // Close the popup RenderWidget from the browser side.
      popup_widget_host_impl->ShutdownAndDestroyWidget(true);
    } else {
      base::WeakPtr<RenderWidgetHostImpl> popup_weak_ptr =
          popup_widget_host_impl->GetWeakPtr();

      // Close the popup RenderWidget from the renderer side by removing focus.
      EXPECT_TRUE(ExecJs(root_frame_host, "document.activeElement.blur()"));

      // Ensure that the RenderWidgetHostImpl gets destroyed, which implies the
      // close step has also been sent to the renderer process.
      while (popup_weak_ptr) {
        GiveItSomeTime(TestTimeouts::tiny_timeout());
      }
    }
    // Ensure the renderer didn't explode :).
    {
      const auto title_when_done =
          std::to_array<std::u16string>({u"done 0", u"done 1"});
      TitleWatcher title_watcher(shell()->web_contents(), title_when_done[i]);
      EXPECT_TRUE(
          ExecJs(root_frame_host, JsReplace("document.title='done $1'", i)));
      EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_done[i]);
    }
  }
}

#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace {

// Intercept PopupWidgetHost::ShowPopup to override the initial bounds
class ShowPopupInterceptor
    : public blink::mojom::PopupWidgetHostInterceptorForTesting {
 public:
  ShowPopupInterceptor(WebContentsImpl* web_contents,
                       RenderFrameHostImpl* frame_host,
                       const gfx::Rect& overriden_bounds)
      : create_new_popup_widget_interceptor_(
            frame_host,
            base::BindOnce(&ShowPopupInterceptor::DidCreatePopupWidget,
                           base::Unretained(this))),
        overriden_bounds_(overriden_bounds) {}

  ShowPopupInterceptor(const ShowPopupInterceptor&) = delete;
  ShowPopupInterceptor& operator=(const ShowPopupInterceptor&) = delete;

  ~ShowPopupInterceptor() override {
    if (auto* rwhi = RenderWidgetHostImpl::FromID(process_id_, routing_id_)) {
      std::ignore =
          rwhi->popup_widget_host_receiver_for_testing().SwapImplForTesting(
              rwhi);
    }
  }

  void Wait() { run_loop_.Run(); }

  // blink::mojom::PopupWidgetHostInterceptorForTesting:
  blink::mojom::PopupWidgetHost* GetForwardingInterface() override {
    DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
    return RenderWidgetHostImpl::FromID(process_id_, routing_id_);
  }

  void ShowPopup(const gfx::Rect& initial_rect,
                 const gfx::Rect& initial_anchor_rect,
                 ShowPopupCallback callback) override {
    GetForwardingInterface()->ShowPopup(overriden_bounds_, initial_anchor_rect,
                                        std::move(callback));
    run_loop_.Quit();
  }

  void DidCreatePopupWidget(RenderWidgetHostImpl* render_widget_host) {
    process_id_ = render_widget_host->GetProcess()->GetID();
    routing_id_ = render_widget_host->GetRoutingID();
    std::ignore = render_widget_host->popup_widget_host_receiver_for_testing()
                      .SwapImplForTesting(this);
  }

  int last_routing_id() const { return routing_id_; }

 private:
  CreateNewPopupWidgetInterceptor create_new_popup_widget_interceptor_;
  base::RunLoop run_loop_;
  gfx::Rect overriden_bounds_;
  int32_t routing_id_ = MSG_ROUTING_NONE;
  int32_t process_id_ = 0;
};

#if BUILDFLAG(IS_MAC)

// Intercepts calls to LocalFrameHost::ShowPopupMenu method(), to override
// initial bounds and hook the `PopupMenuClient`
class ShowPopupMenuInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting,
      public blink::mojom::PopupMenuClient {
 public:
  explicit ShowPopupMenuInterceptor(RenderFrameHostImpl* render_frame_host,
                                    const gfx::Rect& overriden_bounds)
      : overriden_bounds_(overriden_bounds),
        swapped_impl_(
            render_frame_host->local_frame_host_receiver_for_testing(),
            this) {}

  ~ShowPopupMenuInterceptor() override = default;

  LocalFrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void Wait() { run_loop_.Run(); }

  void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override {
    CHECK(GetForwardingInterface());
    GetForwardingInterface()->ShowPopupMenu(
        receiver_.BindNewPipeAndPassRemote(), overriden_bounds_, item_height,
        font_size, selected_item, std::move(menu_items), right_aligned,
        allow_multiple_selection);
  }

  void DidAcceptIndices(const std::vector<int32_t>& indices) override {
    receiver_.reset();
  }

  void DidCancel() override {
    is_cancelled_ = true;
    receiver_.reset();
    run_loop_.Quit();
  }

  bool is_cancelled() const { return is_cancelled_; }

 private:
  base::RunLoop run_loop_;
  bool is_cancelled_{false};
  gfx::Rect overriden_bounds_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalFrameHost>
      swapped_impl_;
  mojo::Receiver<blink::mojom::PopupMenuClient> receiver_{this};
};
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

IN_PROC_BROWSER_TEST_F(RenderWidgetHostSitePerProcessTest,
                       BrowserClosesPopupIntersectsPermissionPrompt) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* contents = static_cast<WebContentsImpl*>(web_contents());
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* root_frame_host = root->current_frame_host();

  // TODO(crbug.com/40750695): Crash when we attempt to use a mock prompt here.
  // After the ticket is fixed, remove the shortcut of getting bounds and use
  // the `MockPermissionPromptFactory` instead.
  // Create a popup widget and wait for the RenderWidgetHost to be shown.
  gfx::Rect permission_exclusion_area_bounds(100, 100, 100, 100);
  static_cast<PermissionControllerImpl*>(
      root_frame_host->GetBrowserContext()->GetPermissionController())
      ->set_exclusion_area_bounds_for_tests(permission_exclusion_area_bounds);
#if BUILDFLAG(IS_MAC)
  ShowPopupMenuInterceptor show_popup_menu_interceptor(
      root_frame_host, permission_exclusion_area_bounds -
                           contents->GetContainerBounds().OffsetFromOrigin());
#else
  ShowPopupInterceptor show_popup_interceptor(contents, root_frame_host,
                                              permission_exclusion_area_bounds);
#endif  // BUILDFLAG(IS_MAC)

  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.text[0] = ' ';
  EXPECT_TRUE(ExecJs(root_frame_host, "focusSelectMenu();"));
  root_frame_host->GetRenderWidgetHost()->ForwardKeyboardEvent(event);

#if BUILDFLAG(IS_MAC)
  show_popup_menu_interceptor.Wait();
  ASSERT_TRUE(show_popup_menu_interceptor.is_cancelled());
#else
  show_popup_interceptor.Wait();
  ASSERT_FALSE(
      RenderWidgetHost::FromID(root_frame_host->GetProcess()->GetID(),
                               show_popup_interceptor.last_routing_id()));
#endif  // BUILDFLAG(IS_MAC)
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that `window.screen` dimensions match the display, not the viewport,
// while the frame is fullscreen. See crbug.com/1367416
IN_PROC_BROWSER_TEST_F(RenderWidgetHostBrowserTest, FullscreenSize) {
  // Check initial dimensions before entering fullscreen.
  ASSERT_FALSE(shell()->IsFullscreenForTabOrPending(web_contents()));
  ASSERT_FALSE(web_contents()->IsFullscreen());
  WaitForVisualPropertiesAck();
  EXPECT_EQ(host()->GetScreenInfo().rect.size().ToString(),
            EvalJs(web_contents(), "`${screen.width}x${screen.height}`"));

  // Enter fullscreen; Content Shell does not resize the viewport to fill the
  // screen in fullscreen on some platforms.
  constexpr char kEnterFullscreenScript[] = R"JS(
    document.documentElement.requestFullscreen().then(() => {
        return !!document.fullscreenElement;
    });
  )JS";
  ASSERT_TRUE(EvalJs(web_contents(), kEnterFullscreenScript).ExtractBool());

  // `window.screen` dimensions match the display size.
  EXPECT_EQ(host()->GetScreenInfo().rect.size().ToString(),
            EvalJs(web_contents(), "`${screen.width}x${screen.height}`"));

  // Check dimensions again after exiting fullscreen.
  constexpr char kExitFullscreenScript[] = R"JS(
    document.exitFullscreen().then(() => {
        return !document.fullscreenElement;
    });
  )JS";
  ASSERT_TRUE(EvalJs(web_contents(), kExitFullscreenScript).ExtractBool());
  ASSERT_FALSE(web_contents()->IsFullscreen());
  EXPECT_EQ(host()->GetScreenInfo().rect.size().ToString(),
            EvalJs(web_contents(), "`${screen.width}x${screen.height}`"));
}

class RenderWidgetHostFoldableCSSTest : public RenderWidgetHostBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// Tests that when a video element goes fullscreen and uses the default
// fullscreen UA stylesheet (in blink/core/css/fullscreen.css) the viewport
// segments MQs and env variables are correctly working.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostFoldableCSSTest,
                       ViewportSegmentsWorksInUAFullscreenCSS) {
  const char kTestPageURL[] =
      R"HTML(data:text/html,<!DOCTYPE html>
      <video id='video'></video>)HTML";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kTestPageURL)));
  // Check initial state before entering fullscreen.
  ASSERT_FALSE(shell()->IsFullscreenForTabOrPending(web_contents()));
  ASSERT_FALSE(web_contents()->IsFullscreen());
  constexpr char kEnterFullscreenScript[] = R"JS(
    document.querySelector('video').requestFullscreen().then(() => {
        return !!document.fullscreenElement;
    });
  )JS";
  // Initial state. This will ensure that no display feature/viewport segments
  // are coming from the platform.
  view()->SetDisplayFeatureForTesting(nullptr);
  host()->SynchronizeVisualProperties();
  ASSERT_TRUE(EvalJs(web_contents(), kEnterFullscreenScript).ExtractBool());

  // Changing the display feature/viewport segments without leaving fullscreen
  // should update the video element.
  const gfx::Size root_view_size = view()->GetVisibleViewportSize();
  const int kDisplayFeatureLength = 10;
  int offset = root_view_size.width() / 2 - kDisplayFeatureLength / 2;
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical, offset,
      /* mask_length */ kDisplayFeatureLength};
  view()->SetDisplayFeatureForTesting(&emulated_display_feature);
  host()->SynchronizeVisualProperties();
  WaitForVisualPropertiesAck();
  EXPECT_EQ(base::NumberToString(offset) + "px",
            EvalJs(shell(), "getComputedStyle(video).width").ExtractString());
  EXPECT_EQ(
      root_view_size.height(),
      EvalJs(shell(), "parseInt(getComputedStyle(video).height)").ExtractInt());

  emulated_display_feature.orientation =
      DisplayFeature::Orientation::kHorizontal;
  offset = root_view_size.height() / 2 - kDisplayFeatureLength / 2;
  emulated_display_feature.offset = offset;
  view()->SetDisplayFeatureForTesting(&emulated_display_feature);
  host()->SynchronizeVisualProperties();
  WaitForVisualPropertiesAck();
  EXPECT_EQ(base::NumberToString(offset) + "px",
            EvalJs(shell(), "getComputedStyle(video).height").ExtractString());
  EXPECT_EQ(
      root_view_size.width(),
      EvalJs(shell(), "parseInt(getComputedStyle(video).width)").ExtractInt());

  // No display feature/viewport segments are set, the video should go
  // fullscreen.
  view()->SetDisplayFeatureForTesting(nullptr);
  host()->SynchronizeVisualProperties();
  WaitForVisualPropertiesAck();
  EXPECT_EQ(
      root_view_size.height(),
      EvalJs(shell(), "parseInt(getComputedStyle(video).height)").ExtractInt());
  EXPECT_EQ(
      root_view_size.width(),
      EvalJs(shell(), "parseInt(getComputedStyle(video).width)").ExtractInt());

  constexpr char kExitFullscreenScript[] = R"JS(
    document.exitFullscreen().then(() => {
        return !document.fullscreenElement;
    });
  )JS";
  ASSERT_TRUE(EvalJs(web_contents(), kExitFullscreenScript).ExtractBool());
  ASSERT_FALSE(web_contents()->IsFullscreen());

  // Change the viewport segments/display feature before entering fullscreen.
  view()->SetDisplayFeatureForTesting(&emulated_display_feature);
  ASSERT_TRUE(EvalJs(web_contents(), kEnterFullscreenScript).ExtractBool());
  host()->SynchronizeVisualProperties();
  WaitForVisualPropertiesAck();
  EXPECT_EQ(base::NumberToString(offset) + "px",
            EvalJs(shell(), "getComputedStyle(video).height").ExtractString());
  EXPECT_EQ(
      root_view_size.width(),
      EvalJs(shell(), "parseInt(getComputedStyle(video).width)").ExtractInt());
}

// Tests that the renderer receives the root widget's viewport segments and
// correctly exposes those via CSS.
// TODO(crbug.com/40137084) Convert this to a WPT once emulation is available
// via WebDriver.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostFoldableCSSTest,
                       FoldablesCSSWithOverrides) {
  const char kTestPageURL[] =
      R"HTML(data:text/html,<!DOCTYPE html>
      <style>
      /* The following styles set the margin top/left/bottom/right to the
         values where the display feature between segments is, and the width and
         height of the div to the width and height of the display feature */
        @media (horizontal-viewport-segments: 2) {
          div {
            margin: env(viewport-segment-top 0 0, 10px)
                    env(viewport-segment-left 1 0, 10px)
                    env(viewport-segment-bottom 0 0, 10px)
                    env(viewport-segment-right 0 0, 10px);
            width: calc(env(viewport-segment-left 1 0, 10px) -
                        env(viewport-segment-right 0 0, 0px));
            height: env(viewport-segment-height 0 0, 10px);
          }
        }

        @media (vertical-viewport-segments: 2) {
          div {
            margin: env(viewport-segment-bottom 0 0, 11px)
                    env(viewport-segment-right 0 1, 11px)
                    env(viewport-segment-top 0 1, 11px)
                    env(viewport-segment-left 0 0, 11px);
            width: env(viewport-segment-width 0 0, 11px);
            height: calc(env(viewport-segment-top 0 1, 11px) -
                         env(viewport-segment-bottom 0 0, 0px));
          }
        }
        @media (horizontal-viewport-segments: 1) and
               (vertical-viewport-segments: 1) {
          div { opacity: 0.1; margin: 1px; width: 1px; height: 1px; }
        }
        @media (horizontal-viewport-segments: 2) and
               (vertical-viewport-segments: 1) {
          div { opacity: 0.2; }
        }
        @media (horizontal-viewport-segments: 1) and
               (vertical-viewport-segments: 2) {
          div { opacity: 0.3; }
        }
      </style>
      <div id='target'></div>)HTML";

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kTestPageURL)));

  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginTop").ExtractString());
  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginRight").ExtractString());
  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginBottom").ExtractString());
  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());
  EXPECT_EQ("1px",
            EvalJs(shell(), "getComputedStyle(target).width").ExtractString());
  EXPECT_EQ("1px",
            EvalJs(shell(), "getComputedStyle(target).height").ExtractString());

  EXPECT_EQ(
      "0.1",
      EvalJs(shell(), "getComputedStyle(target).opacity").ExtractString());

  const gfx::Size root_view_size = view()->GetVisibleViewportSize();
  const int kDisplayFeatureLength = 10;
  int offset = root_view_size.width() / 2 - kDisplayFeatureLength / 2;
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical, offset,
      /* mask_length */ kDisplayFeatureLength};
  view()->SetDisplayFeatureForTesting(&emulated_display_feature);
  host()->SynchronizeVisualProperties();

  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "getComputedStyle(target).marginTop").ExtractString());
  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset +
                           emulated_display_feature.mask_length) +
          "px",
      EvalJs(shell(), "getComputedStyle(target).marginRight").ExtractString());
  EXPECT_EQ(
      base::NumberToString(root_view_size.height()) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginBottom").ExtractString());
  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());
  EXPECT_EQ(base::NumberToString(emulated_display_feature.mask_length) + "px",
            EvalJs(shell(), "getComputedStyle(target).width").ExtractString());
  EXPECT_EQ(base::NumberToString(root_view_size.height()) + "px",
            EvalJs(shell(), "getComputedStyle(target).height").ExtractString());

  EXPECT_EQ(
      "0.2",
      EvalJs(shell(), "getComputedStyle(target).opacity").ExtractString());

  emulated_display_feature.orientation =
      DisplayFeature::Orientation::kHorizontal;
  offset = root_view_size.height() / 2 - kDisplayFeatureLength / 2;
  emulated_display_feature.offset = offset;

  view()->SetDisplayFeatureForTesting(&emulated_display_feature);
  host()->SynchronizeVisualProperties();

  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginTop").ExtractString());
  EXPECT_EQ(
      base::NumberToString(root_view_size.width()) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginRight").ExtractString());
  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset +
                           emulated_display_feature.mask_length) +
          "px",
      EvalJs(shell(), "getComputedStyle(target).marginBottom").ExtractString());
  EXPECT_EQ(
      "0px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());
  EXPECT_EQ(base::NumberToString(root_view_size.width()) + "px",
            EvalJs(shell(), "getComputedStyle(target).width").ExtractString());
  EXPECT_EQ(base::NumberToString(emulated_display_feature.mask_length) + "px",
            EvalJs(shell(), "getComputedStyle(target).height").ExtractString());

  EXPECT_EQ(
      "0.3",
      EvalJs(shell(), "getComputedStyle(target).opacity").ExtractString());

  view()->SetDisplayFeatureForTesting(nullptr);
  host()->SynchronizeVisualProperties();

  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginTop").ExtractString());
  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginRight").ExtractString());
  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginBottom").ExtractString());
  EXPECT_EQ(
      "1px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());
  EXPECT_EQ("1px",
            EvalJs(shell(), "getComputedStyle(target).width").ExtractString());
  EXPECT_EQ("1px",
            EvalJs(shell(), "getComputedStyle(target).height").ExtractString());

  EXPECT_EQ(
      "0.1",
      EvalJs(shell(), "getComputedStyle(target).opacity").ExtractString());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostFoldableCSSTest,
                       FoldablesCSSWithReload) {
  const char kTestPageURL[] =
      R"HTML(data:text/html,<!DOCTYPE html>
      <style>
        @media (horizontal-viewport-segments: 2) and
               (vertical-viewport-segments: 1) {
          div { margin-left: env(viewport-segment-right 0 0, 10px); }
        }
      </style>
      <div id='target'></div>)HTML";

  LoadStopObserver load_stop_observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kTestPageURL)));
  load_stop_observer.Wait();

  const gfx::Size root_view_size = view()->GetVisibleViewportSize();
  const int kDisplayFeatureLength = 10;
  const int offset = root_view_size.width() / 2 - kDisplayFeatureLength / 2;
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical, offset,
      /* mask_length */ kDisplayFeatureLength};
  {
    view()->SetDisplayFeatureForTesting(&emulated_display_feature);
    host()->SynchronizeVisualProperties();
  }

  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());

  // Ensure that the environment variables have the correct values in the new
  // document that is created on reloading the page.
  LoadStopObserver load_stop_observer2(shell()->web_contents());
  TestNavigationManager navigation_manager(shell()->web_contents(),
                                           GURL(kTestPageURL));
  shell()->Reload();
  EXPECT_TRUE(navigation_manager.WaitForResponse());
  if (ShouldCreateNewHostForAllFrames()) {
    // When RenderDocument is enabled, a new RenderWidgetHost will be created
    // after the reload, so we need to call SynchronizeVisualProperties() again.
    RenderWidgetHostImpl* target_rwh = static_cast<RenderWidgetHostImpl*>(
        navigation_manager.GetNavigationHandle()
            ->GetRenderFrameHost()
            ->GetRenderWidgetHost());
    target_rwh->GetView()->SetDisplayFeatureForTesting(
        &emulated_display_feature);
    target_rwh->SynchronizeVisualProperties();
  }
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  load_stop_observer2.Wait();

  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());
}

class RenderWidgetHostDelegatedInkMetadataTest
    : public RenderWidgetHostTouchEmulatorBrowserTest {
 public:
  RenderWidgetHostDelegatedInkMetadataTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "DelegatedInkTrails");
  }
};

// Confirm that using the |updateInkTrailStartPoint| JS API results in the
// |request_points_for_delegated_ink_| flag being set on the RWHVB.
// TODO(crbug.com/40852704). Flaky on Linux.
// TODO(crbug.com/40929902): Failing on ChromesOS MSan.
#if BUILDFLAG(IS_LINUX) || (BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER))
#define MAYBE_FlagGetsSetFromRenderFrameMetadata \
  DISABLED_FlagGetsSetFromRenderFrameMetadata
#else
#define MAYBE_FlagGetsSetFromRenderFrameMetadata \
  FlagGetsSetFromRenderFrameMetadata
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostDelegatedInkMetadataTest,
                       MAYBE_FlagGetsSetFromRenderFrameMetadata) {
  ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
      let presenter = null;
      navigator.ink.requestPresenter().then(e => { presenter = e; });
      let style = { color: 'green', diameter: 21 };

      window.addEventListener('pointermove' , evt => {
        presenter.updateInkTrailStartPoint(evt, style);
      });
      )"));
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 10, 0,
                           false);
  RunUntilInputProcessed(host());

  {
    const cc::RenderFrameMetadata& last_metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    EXPECT_TRUE(last_metadata.delegated_ink_metadata.has_value());
    EXPECT_TRUE(
        last_metadata.delegated_ink_metadata.value().delegated_ink_is_hovering);
  }

  // Confirm that the state of hover changing on the next produced delegated ink
  // metadata results in a new RenderFrameMetadata being sent, with
  // |delegated_ink_hovering| false.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 20, 20,
                           blink::WebInputEvent::kLeftButtonDown, false);
  RunUntilInputProcessed(host());

  {
    const cc::RenderFrameMetadata& last_metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    EXPECT_TRUE(last_metadata.delegated_ink_metadata.has_value());
    EXPECT_FALSE(
        last_metadata.delegated_ink_metadata.value().delegated_ink_is_hovering);
  }

  // Confirm that the flag is set back to false when the JS API isn't called.
  RunUntilInputProcessed(host());
  const cc::RenderFrameMetadata& last_metadata =
      host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
  EXPECT_FALSE(last_metadata.delegated_ink_metadata.has_value());

  // Finally, confirm that a change in hovering state (pointerdown to pointerup
  // here) without a call to updateInkTrailStartPoint doesn't cause a new
  // RenderFrameMetadata to be sent.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 20, 20, 0,
                           false);
  RunUntilInputProcessed(host());
  EXPECT_EQ(
      last_metadata,
      host()->render_frame_metadata_provider()->LastRenderFrameMetadata());
}

// If the DelegatedInkTrailPresenter creates a metadata that has the same
// timestamp as the previous one, it does not set the metadata.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostDelegatedInkMetadataTest,
                       DuplicateMetadata) {
  ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
      let presenter = null;
      navigator.ink.requestPresenter().then(e => { presenter = e; });
      let style = { color: 'green', diameter: 21 };
      let first_move_event = null;

      window.addEventListener('pointermove' , evt => {
        if (first_move_event == null) {
          first_move_event = evt;
        }
        presenter.updateInkTrailStartPoint(first_move_event, style);
      });
      )"));
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 10, 0,
                           false);
  RunUntilInputProcessed(host());

  {
    const cc::RenderFrameMetadata& last_metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    EXPECT_TRUE(last_metadata.delegated_ink_metadata.has_value());
    EXPECT_TRUE(
        last_metadata.delegated_ink_metadata.value().delegated_ink_is_hovering);
  }

  // Confirm metadata has no value when updateInkTrailStartPoint is called
  // with the same event.
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 20, 20,
                           blink::WebInputEvent::kLeftButtonDown, false);
  RunUntilInputProcessed(host());

  {
    const cc::RenderFrameMetadata& last_metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    EXPECT_FALSE(last_metadata.delegated_ink_metadata.has_value());
  }
}

namespace {

class LocalSurfaceIdChangedObserver
    : public RenderFrameMetadataProvider::Observer {
 public:
  explicit LocalSurfaceIdChangedObserver(
      bool expect_newer_id,
      const viz::LocalSurfaceId& local_surface_id,
      RenderFrameMetadataProviderImpl* provider)
      : expect_newer_id_(expect_newer_id),
        current_id_(local_surface_id),
        provider_(provider) {
    provider_->AddObserver(this);
  }
  ~LocalSurfaceIdChangedObserver() override { provider_->RemoveObserver(this); }

  // `RenderFrameMetadataProvider::Observer`:
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {
    if (!metadata.local_surface_id.has_value()) {
      return;
    }
    if (expect_newer_id_ &&
        !metadata.local_surface_id->IsNewerThan(current_id_)) {
      // Only record the first newer id.
      return;
    }
    if (!expect_newer_id_) {
      // Fail immediately instead of timing out.
      ASSERT_FALSE(metadata.local_surface_id->IsNewerThan(current_id_));
      if (metadata.local_surface_id != current_id_) {
        // Only record the first id that's the same.
        return;
      }
    }
    observed_id_ = metadata.local_surface_id.value();
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override {}
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  [[nodiscard]] bool WaitForExpectedLocalSurfaceIdUpdate(
      const viz::LocalSurfaceId& expected_id) {
    // If `OnRenderFrameMetadataChangedBeforeActivation()` is called before
    // `WaitForExpectedLocalSurfaceIdUpdate()`.
    if (observed_id_.is_valid()) {
      return observed_id_ == expected_id;
    }
    CHECK(!run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    return observed_id_ == expected_id;
  }

  const viz::LocalSurfaceId& observed_id() const { return observed_id_; }

 private:
  const bool expect_newer_id_;
  const viz::LocalSurfaceId current_id_;
  const raw_ptr<RenderFrameMetadataProviderImpl> provider_;
  std::unique_ptr<base::RunLoop> run_loop_;
  viz::LocalSurfaceId observed_id_ = viz::LocalSurfaceId{};
};

}  // namespace

class RenderWidgetHostSameDocNavUpdatesLocalSurfaceIdTest
    : public RenderWidgetHostBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  RenderWidgetHostSameDocNavUpdatesLocalSurfaceIdTest() {
    bool increment_local_surface_id = GetParam();
    if (increment_local_surface_id) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{blink::features::
                kIncrementLocalSurfaceIdForMainframeSameDocNavigation,
            {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {},
          /*disabled_features=*/{
              blink::features::
                  kIncrementLocalSurfaceIdForMainframeSameDocNavigation});
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    RenderWidgetHostBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForcePrefersNoReducedMotion);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Assert that with `IncrementLocalSurfaceIdForMainframeSameDocNavigation`
// enabled, the `LocalSurfaceId` will be updated for same-doc navigations.
IN_PROC_BROWSER_TEST_P(RenderWidgetHostSameDocNavUpdatesLocalSurfaceIdTest,
                       SameDocNavigationUpdatesLocalSurfaceId) {
  bool increment_local_surface_id = GetParam();
  ASSERT_EQ(increment_local_surface_id,
            base::FeatureList::IsEnabled(
                blink::features::
                    kIncrementLocalSurfaceIdForMainframeSameDocNavigation));
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/session_history/fragment.html")));
  // Changes the background color when navigate to "fragment.html#a".
  ASSERT_TRUE(ExecJs(web_contents(), R"(
    window.addEventListener("hashchange", (event) => {
      document.body.style.background = 'red';
    })
  )"));
  // Get the current LocalSurfaceId of the mainframe.
  const viz::LocalSurfaceId& id_before_nav = view()->GetLocalSurfaceId();
  LocalSurfaceIdChangedObserver obs(
      increment_local_surface_id, id_before_nav,
      view()->host()->render_frame_metadata_provider());
  viz::LocalSurfaceId expected;
  if (increment_local_surface_id) {
    // Expect the child component of the LocalSurfaceId is incremented by one,
    // as the result of the same-doc navigation to #a.
    expected = viz::LocalSurfaceId(id_before_nav.parent_sequence_number(),
                                   id_before_nav.child_sequence_number() + 1,
                                   id_before_nav.embed_token());
  } else {
    expected = id_before_nav;
  }
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/session_history/fragment.html#a")));
  // Forces a frame submission from the renderer.
  WaitForCopyableViewInWebContents(shell()->web_contents());
  ASSERT_TRUE(obs.WaitForExpectedLocalSurfaceIdUpdate(expected))
      << "Expected " << expected << " but observed " << obs.observed_id();
}

INSTANTIATE_TEST_SUITE_P(All,
                         RenderWidgetHostSameDocNavUpdatesLocalSurfaceIdTest,
                         ::testing::Bool());

namespace {

enum class TestConfig { kSameDoc = 0, kBFCacheEnabled, kBFCacheDisabled };

static std::string DescribeTest(
    const ::testing::TestParamInfo<TestConfig>& info) {
  switch (info.param) {
    case TestConfig::kSameDoc:
      return "SameDoc";
    case TestConfig::kBFCacheEnabled:
      return "CrossDoc_BFCacheEnabled";
    case TestConfig::kBFCacheDisabled:
      return "CrossDoc_BFCacheDisabled";
  }
}

class ItemSequenceNumberObserver : public RenderFrameMetadataProvider::Observer,
                                   public WebContentsObserver {
 public:
  ItemSequenceNumberObserver(WebContents* web_contents,
                             int64_t expected_sequence_number)
      : WebContentsObserver(web_contents),
        expected_sequence_number_(expected_sequence_number) {}
  ~ItemSequenceNumberObserver() override {
    if (provider_) {
      provider_->RemoveObserver(this);
    }
  }

  // `RenderFrameMetadataProvider::Observer`:
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override {
    ASSERT_TRUE(provider_);
    if (expected_sequence_number_ ==
        provider_->LastRenderFrameMetadata()
            .primary_main_frame_item_sequence_number) {
      observed_expected_number_ = true;
    } else {
      return;
    }
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  // `WebContentsObserver`:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsSameDocument());
    auto* request = static_cast<NavigationRequest*>(navigation_handle);
    RenderFrameHostImpl* rfhi = request->GetRenderFrameHost();
    ASSERT_TRUE(rfhi);
    provider_ = rfhi->GetView()->host()->render_frame_metadata_provider();
    provider_->AddObserver(this);
  }

  // For same-doc navigations, we don't get `WCO::ReadyToCommitNavigation`.
  // Since this is testing code, we just set the provider directly (instead of
  // registering a new `CommitDeferringCondition`).
  void SetProviderForSameDocNavigations(
      RenderFrameMetadataProviderImpl* provider) {
    ASSERT_FALSE(provider_);
    provider_ = provider;
    provider_->AddObserver(this);
  }

  [[nodiscard]] bool WaitForExpectedItemSequenceNumber() {
    if (!provider_) {
      return false;
    }
    // If `OnRenderFrameMetadataChangedAfterActivation()` is called before
    // `WaitForExpectedItemSequenceNumber()`.
    if (observed_expected_number_) {
      return true;
    }
    CHECK(!run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    return observed_expected_number_;
  }

 private:
  const int64_t expected_sequence_number_;

  raw_ptr<RenderFrameMetadataProviderImpl> provider_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool observed_expected_number_ = false;
};

class ItemSequenceNumberTracker : public RenderFrameMetadataProvider::Observer {
 public:
  explicit ItemSequenceNumberTracker(RenderFrameMetadataProviderImpl* provider)
      : provider_(provider),
        last_sequence_number_(provider_->LastRenderFrameMetadata()
                                  .primary_main_frame_item_sequence_number) {
    provider_->AddObserver(this);
  }
  ~ItemSequenceNumberTracker() override { provider_->RemoveObserver(this); }

  // `RenderFrameMetadataProvider::Observer`:
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override {
    last_sequence_number_ = provider_->LastRenderFrameMetadata()
                                .primary_main_frame_item_sequence_number;
    if (run_loop_ && last_sequence_number_ == expected_sequence_number_) {
      run_loop_->Quit();
    }
  }
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  void WaitForExpectedItemSequenceNumber(int64_t expected_sequence_number) {
    expected_sequence_number_ = expected_sequence_number;
    if (expected_sequence_number_ == last_sequence_number_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  raw_ptr<RenderFrameMetadataProviderImpl> provider_;

  int64_t expected_sequence_number_;
  int64_t last_sequence_number_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

class RenderWidgetHostItemSequenceNumberInRenderFrameMetadataTest
    : public RenderWidgetHostBrowserTest,
      public ::testing::WithParamInterface<TestConfig> {
 public:
  RenderWidgetHostItemSequenceNumberInRenderFrameMetadataTest() = default;
  ~RenderWidgetHostItemSequenceNumberInRenderFrameMetadataTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    auto test_config = GetParam();
    switch (test_config) {
      case TestConfig::kSameDoc: {
        first_url_ = "/changing_color.html#red";
        second_url_ = "/changing_color.html#green";
        break;
      }
      case TestConfig::kBFCacheEnabled: {
        first_url_ = "/empty.html";
        second_url_ = "/title1.html";
        scoped_feature_list_.InitWithFeaturesAndParameters(
            GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
            GetDefaultDisabledBackForwardCacheFeaturesForTesting());
        break;
      }
      case TestConfig::kBFCacheDisabled: {
        first_url_ = "/empty.html";
        second_url_ = "/title1.html";
        command_line->AppendSwitch(switches::kDisableBackForwardCache);
        break;
      }
    }
    RenderWidgetHostBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL FirstURL() { return embedded_test_server()->GetURL(first_url_); }

  GURL SecondURL() { return embedded_test_server()->GetURL(second_url_); }

 private:
  std::string first_url_;
  std::string second_url_;
  std::vector<std::string> hosts_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(
    RenderWidgetHostItemSequenceNumberInRenderFrameMetadataTest,
    ItemSequenceNumberExpected) {
  ASSERT_TRUE(NavigateToURL(shell(), FirstURL()));
  ASSERT_TRUE(NavigateToURL(shell(), SecondURL()));

  auto* controller = static_cast<NavigationControllerImpl*>(
      &(web_contents()->GetController()));
  ASSERT_EQ(controller->GetEntryCount(), 2);
  int64_t expected_sequence_number =
      controller->GetEntryAtIndex(0)
          ->GetFrameEntry(controller->frame_tree().root())
          ->item_sequence_number();

  ItemSequenceNumberObserver obs(web_contents(), expected_sequence_number);
  if (GetParam() == TestConfig::kSameDoc) {
    obs.SetProviderForSameDocNavigations(
        view()->host()->render_frame_metadata_provider());
  }

  TestNavigationObserver nav_observer(web_contents(), 1);
  ASSERT_TRUE(web_contents()->GetController().CanGoBack());
  web_contents()->GetController().GoBack();

  nav_observer.WaitForNavigationFinished();
  ASSERT_TRUE(obs.WaitForExpectedItemSequenceNumber());
}

IN_PROC_BROWSER_TEST_P(
    RenderWidgetHostItemSequenceNumberInRenderFrameMetadataTest,
    ItemSequenceNumberExpectedNoContentChange) {
  if (GetParam() != TestConfig::kSameDoc) {
    return;
  }

  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/session_history/fragment.html")));

  auto* controller = static_cast<NavigationControllerImpl*>(
      &(web_contents()->GetController()));
  ItemSequenceNumberTracker tracker(
      view()->host()->render_frame_metadata_provider());

  tracker.WaitForExpectedItemSequenceNumber(
      controller->GetEntryAtIndex(0)
          ->GetFrameEntry(controller->frame_tree().root())
          ->item_sequence_number());

  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/session_history/fragment.html#a")));
  tracker.WaitForExpectedItemSequenceNumber(
      controller->GetEntryAtIndex(1)
          ->GetFrameEntry(controller->frame_tree().root())
          ->item_sequence_number());

  ASSERT_TRUE(web_contents()->GetController().CanGoBack());
  web_contents()->GetController().GoBack();
  tracker.WaitForExpectedItemSequenceNumber(
      controller->GetEntryAtIndex(0)
          ->GetFrameEntry(controller->frame_tree().root())
          ->item_sequence_number());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RenderWidgetHostItemSequenceNumberInRenderFrameMetadataTest,
#if BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug/345304287): Temporary disable the same-doc variant while
    // investigate the cause for flakiness.
    ::testing::ValuesIn({TestConfig::kBFCacheEnabled,
                         TestConfig::kBFCacheDisabled}),
#else
    ::testing::ValuesIn({TestConfig::kSameDoc, TestConfig::kBFCacheEnabled,
                         TestConfig::kBFCacheDisabled}),
#endif
    &DescribeTest);

}  // namespace content
