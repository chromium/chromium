// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/synthetic_smooth_drag_gesture.h"
#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/latency/latency_info.h"

namespace content {

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
      WindowedNotificationObserver(
          NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
          Source<RenderWidgetHost>(host()))
          .Wait();
    }
  }
};

// This test enables --site-per-porcess flag.
class RenderWidgetHostSitePerProcessTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
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

  TouchActionFilter* GetTouchActionFilterForWidget(RenderWidgetHostImpl* rwhi) {
    return &static_cast<InputRouterImpl*>(rwhi->input_router())
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
    if (blink::WebInputEvent::IsTouchEventType(event.GetType()))
      acked_touch_event_type_ = event.GetType();
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
      : view_(nullptr),
        host_(nullptr),
        router_(nullptr),
        last_simulated_event_time_(ui::EventTimeForNow()),
        simulated_event_time_delta_(base::TimeDelta::FromMilliseconds(100)) {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    EXPECT_TRUE(NavigateToURL(
        shell(), GURL("data:text/html,<!doctype html>"
                      "<body style='background-color: red;'></body>")));

    view_ = static_cast<RenderWidgetHostViewBase*>(
        shell()->web_contents()->GetRenderWidgetHostView());
    host_ = static_cast<RenderWidgetHostImpl*>(view_->GetRenderWidgetHost());
    router_ = static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetInputEventRouter();
    ASSERT_TRUE(router_);
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
    if (pressed)
      event.button = blink::WebMouseEvent::Button::kLeft;
    event.SetTimeStamp(GetNextSimulatedEventTime());
    router_->RouteMouseEvent(view_, &event, ui::LatencyInfo());
  }

  void WaitForAckWith(blink::WebInputEvent::Type type) {
    InputMsgWatcher watcher(host(), type);
    watcher.GetAckStateWaitIfNecessary();
  }

  RenderWidgetHostImpl* host() { return host_; }
  RenderWidgetHostViewBase* view() { return view_; }

 private:
  RenderWidgetHostViewBase* view_;
  RenderWidgetHostImpl* host_;
  RenderWidgetHostInputEventRouter* router_;

  base::TimeTicks last_simulated_event_time_;
  const base::TimeDelta simulated_event_time_delta_;
};

// Synthetic mouse events not allowed on Android.
#if !defined(OS_ANDROID)
// This test makes sure that TouchEmulator doesn't emit a GestureScrollEnd
// without a valid unique_touch_event_id when it sees a GestureFlingStart
// terminating the underlying mouse scroll sequence. If the GestureScrollEnd is
// given a unique_touch_event_id of 0, then a crash will occur.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostTouchEmulatorBrowserTest,
                       TouchEmulatorPinchWithGestureFling) {
  auto* touch_emulator = host()->GetTouchEmulator();
  touch_emulator->Enable(TouchEmulator::Mode::kEmulatingTouchFromMouse,
                         ui::GestureProviderConfigType::GENERIC_MOBILE);
  touch_emulator->SetPinchGestureModeForTesting(true);

  TestInputEventObserver observer;
  host()->AddInputEventObserver(&observer);

  SyntheticSmoothDragGestureParams params;
  params.start_point = gfx::PointF(10.f, 110.f);
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
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
    auto it_gse = std::find(dispatched_events.begin(), dispatched_events.end(),
                            blink::WebInputEvent::Type::kGestureScrollEnd);
    EXPECT_NE(dispatched_events.end(), it_gse);
  } while (!touch_emulator->suppress_next_fling_cancel_for_testing());
}
#endif  // !defined(OS_ANDROID)

// Todo(crbug.com/994353): The test is flaky(crash/timeout) on MSAN, TSAN, and
// DEBUG builds.
#if (!defined(NDEBUG) || defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER))
#define MAYBE_TouchEmulator DISABLED_TouchEmulator
#else
#define MAYBE_TouchEmulator TouchEmulator
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostTouchEmulatorBrowserTest,
                       MAYBE_TouchEmulator) {
  host()->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
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
  host()->GetTouchEmulator()->Disable();
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
  host()->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
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
  host()->GetTouchEmulator()->Disable();
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

  void Wait() {
    if (loaded_)
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

 private:
  void DidFinishLoad(RenderFrameHost* rfh, const GURL& url) override {
    loaded_ |= (url == document_origin_);
    if (loaded_ && run_loop_)
      run_loop_->Quit();
  }

  bool loaded_ = false;
  const GURL document_origin_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DocumentLoadObserver);
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
                                                   ->GetFrameTree()
                                                   ->root()
                                                   ->child_at(0)
                                                   ->current_frame_host()
                                                   ->GetRenderWidgetHost());
  EXPECT_TRUE(filter->allowed_touch_action().has_value());
}

// The plumbing that this test is verifying is not utilized on Mac/Android,
// where popup menus don't create a popup RenderWidget, but rather they trigger
// a FrameHostMsg_ShowPopup to ask the browser to build and display the actual
// popup using native controls.
#if !defined(OS_MAC) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostSitePerProcessTest,
                       BrowserClosesSelectPopup) {
  // Navigate to a page with a <select> element.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = contents->GetFrameTree()->root();
  RenderFrameHostImpl* root_frame_host = root->current_frame_host();
  RenderProcessHost* process = root_frame_host->GetProcess();

  // Open the <select> menu by focusing it and sending a space key
  // at the focused node. This creates a popup widget.
  NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.text[0] = ' ';

  // A class to wait for ViewHostMsg_ShowWidget.
  class WaitForShowWidgetFilter : public ObserveMessageFilter {
   public:
    explicit WaitForShowWidgetFilter()
        : ObserveMessageFilter(ViewMsgStart, ViewHostMsg_ShowWidget::ID) {}

    bool OnMessageReceived(const IPC::Message& message) override {
      IPC_BEGIN_MESSAGE_MAP(WaitForShowWidgetFilter, message)
        IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
      IPC_END_MESSAGE_MAP()
      return ObserveMessageFilter::OnMessageReceived(message);
    }

    int routing_id() const { return routing_id_; }

   private:
    ~WaitForShowWidgetFilter() override = default;

    void OnShowWidget(int routing_id, const gfx::Rect& initial_rect) {
      routing_id_ = routing_id;
    }

    int routing_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(WaitForShowWidgetFilter);
  };

  for (int i = 0; i < 2; ++i) {
    bool browser_closes = i == 0;

    // This focuses and opens the select box, creating a popup RenderWidget. We
    // wait for the RenderWidgetHost to be shown.
    auto filter = base::MakeRefCounted<WaitForShowWidgetFilter>();
    process->AddFilter(filter.get());
    EXPECT_TRUE(ExecuteScript(root_frame_host, "focusSelectMenu();"));
    root_frame_host->GetRenderWidgetHost()->ForwardKeyboardEvent(event);
    filter->Wait();

    // The popup RenderWidget will get its own routing id.
    int popup_routing_id = filter->routing_id();
    EXPECT_TRUE(popup_routing_id);
    // Grab a pointer to the popup RenderWidget.
    RenderWidgetHost* popup_widget_host =
        RenderWidgetHost::FromID(process->GetID(), popup_routing_id);
    ASSERT_TRUE(popup_widget_host);
    ASSERT_NE(popup_widget_host, root_frame_host->GetRenderWidgetHost());

    // A class to wait for WidgetHostMsg_Close_ACK.
    auto close_filter = base::MakeRefCounted<ObserveMessageFilter>(
        WidgetMsgStart, WidgetHostMsg_Close_ACK::ID);
    process->AddFilter(close_filter.get());

    if (browser_closes) {
      // Close the popup RenderWidget from the browser side.
      auto* popup_widget_host_impl =
          static_cast<RenderWidgetHostImpl*>(popup_widget_host);
      popup_widget_host_impl->ShutdownAndDestroyWidget(true);
    } else {
      // Close the popup RenderWidget from the renderer side by removing focus.
      EXPECT_TRUE(
          ExecuteScript(root_frame_host, "document.activeElement.blur()"));
    }
    // In either case, wait until closing the popup RenderWidget is complete to
    // know it worked by waiting for the WidgetHostMsg_Close_ACK.
    close_filter->Wait();

    // Ensure the renderer didn't explode :).
    {
      base::string16 title_when_done[] = {base::UTF8ToUTF16("done 0"),
                                          base::UTF8ToUTF16("done 1")};
      TitleWatcher title_watcher(shell()->web_contents(), title_when_done[i]);
      EXPECT_TRUE(ExecuteScript(root_frame_host,
                                JsReplace("document.title='done $1'", i)));
      EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_done[i]);
    }
  }
}
#endif

// Tests that the renderer receives the blink::ScreenInfo size overrides
// while the page is in fullscreen mode. This is a regression test for
// https://crbug.com/1060795.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostBrowserTest,
                       PropagatesFullscreenSizeOverrides) {
  class FullscreenWaiter : public WebContentsObserver {
   public:
    explicit FullscreenWaiter(WebContents* wc) : WebContentsObserver(wc) {}

    void Wait(bool enter) {
      if (web_contents()->IsFullscreen() != enter) {
        run_loop_.Run();
      }
      EXPECT_EQ(enter, web_contents()->IsFullscreen());
    }

   private:
    void DidToggleFullscreenModeForTab(bool entered,
                                       bool will_resize) override {
      run_loop_.Quit();
    }

    base::RunLoop run_loop_;
  };

  // Sanity-check: Ensure the Shell and WebContents both agree the browser is
  // not currently in fullscreen.
  ASSERT_FALSE(shell()->IsFullscreenForTabOrPending(web_contents()));
  ASSERT_FALSE(web_contents()->IsFullscreen());

  // While not fullscreened, expect the screen size to not be overridden.
  blink::ScreenInfo screen_info;
  host()->GetScreenInfo(&screen_info);
  WaitForVisualPropertiesAck();
  EXPECT_EQ(screen_info.rect.size().ToString(),
            EvalJs(web_contents(), "`${screen.width}x${screen.height}`"));

  // Enter fullscreen mode. The Content Shell does not resize the view to fill
  // the entire screen, and so the page will see the view's size as the screen
  // size. This confirms the ScreenInfo override logic is working.
  ASSERT_TRUE(ExecJs(web_contents(), "document.body.requestFullscreen();"));
  FullscreenWaiter(web_contents()).Wait(true);
  WaitForVisualPropertiesAck();
  EXPECT_EQ(view()->GetRequestedRendererSize().ToString(),
            EvalJs(web_contents(), "`${screen.width}x${screen.height}`"));

  // Exit fullscreen mode, and then the page should see the screen size again.
  ASSERT_TRUE(ExecJs(web_contents(), "document.exitFullscreen();"));
  FullscreenWaiter(web_contents()).Wait(false);
  host()->GetScreenInfo(&screen_info);
  WaitForVisualPropertiesAck();
  EXPECT_EQ(screen_info.rect.size().ToString(),
            EvalJs(web_contents(), "`${screen.width}x${screen.height}`"));
}

class RenderWidgetHostFoldableCSSTest : public RenderWidgetHostBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// Tests that the renderer receives the root widget's window segments and
// correctly exposes those via CSS.
// TODO(crbug.com/1098549) Convert this to a WPT once emulation is available
// via WebDriver.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostFoldableCSSTest,
                       FoldablesCSSWithOverrides) {
  const char kTestPageURL[] =
      R"HTML(data:text/html,<!DOCTYPE html>
      <style>
        div {
          margin: env(fold-top, 1px) env(fold-right, 1px)
                  env(fold-bottom, 1px) env(fold-left, 1px);
          width: env(fold-width, 1px);
          height: env(fold-height, 1px);
        }
        @media (screen-spanning: none) {
          div { opacity: 0.1; }
        }
        @media (screen-spanning: single-fold-vertical) {
          div { opacity: 0.2; }
        }
        @media (screen-spanning: single-fold-horizontal) {
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
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical,
      /* offset */ root_view_size.width() / 2 - kDisplayFeatureLength / 2,
      /* mask_length */ kDisplayFeatureLength};
  view()->SetDisplayFeatureForTesting(emulated_display_feature);
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
  emulated_display_feature.offset =
      root_view_size.height() / 2 - kDisplayFeatureLength / 2,
  view()->SetDisplayFeatureForTesting(emulated_display_feature);
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

  view()->SetDisplayFeatureForTesting(base::nullopt);
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
        @media (screen-spanning: single-fold-vertical) {
          div { margin-left: env(fold-left, 10px); }
        }
      </style>
      <div id='target'></div>)HTML";

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kTestPageURL)));

  const gfx::Size root_view_size = view()->GetVisibleViewportSize();
  const int kDisplayFeatureLength = 10;
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical,
      /* offset */ root_view_size.width() / 2 - kDisplayFeatureLength / 2,
      /* mask_length */ kDisplayFeatureLength};
  view()->SetDisplayFeatureForTesting(emulated_display_feature);
  host()->SynchronizeVisualProperties();

  EXPECT_EQ(
      base::NumberToString(emulated_display_feature.offset) + "px",
      EvalJs(shell(), "getComputedStyle(target).marginLeft").ExtractString());

  // Ensure that the environment variables have the correct values in the new
  // document that is created on reloading the page.
  WindowedNotificationObserver load_stop_observer(
      NOTIFICATION_LOAD_STOP, NotificationService::AllSources());
  shell()->Reload();
  load_stop_observer.Wait();

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
IN_PROC_BROWSER_TEST_F(RenderWidgetHostDelegatedInkMetadataTest,
                       FlagGetsSetFromRenderFrameMetadata) {
  ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
      let presenter = navigator.ink.requestPresenter('delegated-ink-trail');
      let style = { color: 'green', diameter: 21 };
      window.addEventListener('pointermove' , evt => {
        presenter.then( function(v) {
          v.updateInkTrailStartPoint(evt, style);
        });
      });
      )"));
  SimulateRoutedMouseEvent(blink::WebInputEvent::Type::kMouseMove, 10, 10, 0,
                           false);
  RunUntilInputProcessed(host());

  {
    const cc::RenderFrameMetadata& last_metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    EXPECT_TRUE(last_metadata.has_delegated_ink_metadata);
  }

  // Confirm that the flag is set back to false when the JS API isn't called.
  RunUntilInputProcessed(host());
  {
    const cc::RenderFrameMetadata& last_metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    EXPECT_FALSE(last_metadata.has_delegated_ink_metadata);
  }
}

}  // namespace content
