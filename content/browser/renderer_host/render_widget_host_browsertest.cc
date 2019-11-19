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
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/latency/latency_info.h"

namespace content {

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

  void OnInputEventAck(InputEventAckSource source,
                       InputEventAckState state,
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
        SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
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
        host(), blink::WebInputEvent::kGestureScrollEnd);
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
                            blink::WebInputEvent::kGestureScrollEnd);
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
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 10, 0, false);
  TestInputEventObserver::EventTypeVector dispatched_events =
      observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(0u, dispatched_events.size());

  // Mouse press becomes touch start which in turn becomes tap.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseDown, 10, 10, 0, true);
  WaitForAckWith(blink::WebInputEvent::kTouchStart);
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown, dispatched_events[1]);

  // Mouse drag generates touch move, cancels tap and starts scroll.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 30, 0, true);
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(4u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapCancel, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin, dispatched_events[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate, dispatched_events[3]);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());
  EXPECT_EQ(0u, observer.GetAndResetDispatchedEventTypes().size());

  // Mouse drag with shift becomes pinch.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 35,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin, dispatched_events[1]);

  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 50,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchUpdate, dispatched_events[1]);

  // Mouse drag without shift becomes scroll again.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 60, 0, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(3u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate, dispatched_events[2]);

  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 70, 0, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate, dispatched_events[1]);

  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseUp, 10, 70, 0, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchEnd, observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchEnd, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd, dispatched_events[1]);

  // Mouse move does nothing.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 80, 0, false);
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(0u, dispatched_events.size());

  // Another mouse down continues scroll.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseDown, 10, 80, 0, true);
  WaitForAckWith(blink::WebInputEvent::kTouchStart);
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown, dispatched_events[1]);
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 100, 0, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(4u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapCancel, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin, dispatched_events[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate, dispatched_events[3]);
  EXPECT_EQ(0u, observer.GetAndResetDispatchedEventTypes().size());

  // Another pinch.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 110,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin, dispatched_events[1]);
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 120,
                           blink::WebInputEvent::kShiftKey, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  EXPECT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchUpdate, dispatched_events[1]);

  // Turn off emulation during a pinch.
  host()->GetTouchEmulator()->Disable();
  EXPECT_EQ(blink::WebInputEvent::kTouchCancel,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(3u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchCancel, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd, dispatched_events[2]);

  // Mouse event should pass untouched.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 10,
                           blink::WebInputEvent::kShiftKey, true);
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(1u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kMouseMove, dispatched_events[0]);

  // Turn on emulation.
  host()->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
      ui::GestureProviderConfigType::GENERIC_MOBILE);

  // Another touch.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseDown, 10, 10, 0, true);
  WaitForAckWith(blink::WebInputEvent::kTouchStart);
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown, dispatched_events[1]);

  // Scroll.
  SimulateRoutedMouseEvent(blink::WebInputEvent::kMouseMove, 10, 30, 0, true);
  EXPECT_EQ(blink::WebInputEvent::kTouchMove,
            observer.acked_touch_event_type());
  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(4u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchMove, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapCancel, dispatched_events[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin, dispatched_events[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate, dispatched_events[3]);
  EXPECT_EQ(0u, observer.GetAndResetDispatchedEventTypes().size());

  // Turn off emulation during a scroll.
  host()->GetTouchEmulator()->Disable();
  EXPECT_EQ(blink::WebInputEvent::kTouchCancel,
            observer.acked_touch_event_type());

  dispatched_events = observer.GetAndResetDispatchedEventTypes();
  ASSERT_EQ(2u, dispatched_events.size());
  EXPECT_EQ(blink::WebInputEvent::kTouchCancel, dispatched_events[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd, dispatched_events[1]);

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
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
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
      blink::WebKeyboardEvent::kChar, blink::WebInputEvent::kNoModifiers,
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

}  // namespace content
