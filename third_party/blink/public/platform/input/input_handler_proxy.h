// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_INPUT_HANDLER_PROXY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_INPUT_HANDLER_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "cc/input/input_handler.h"
#include "cc/input/snap_fling_controller.h"
#include "cc/paint/element_id.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/platform/input/synchronous_input_handler_proxy.h"
#include "third_party/blink/public/platform/web_common.h"

namespace base {
class TickClock;
}

namespace ui {
class LatencyInfo;
}

namespace blink {
class WebInputEventAttribution;
class WebMouseWheelEvent;
class WebTouchEvent;
class ElasticOverscrollController;
}  // namespace blink

namespace blink {

namespace test {
class InputHandlerProxyTest;
class InputHandlerProxyEventQueueTest;
class InputHandlerProxyMomentumScrollJankTest;
class InputHandlerProxyForceHandlingOnMainThread;
class TestInputHandlerProxy;
class UnifiedScrollingInputHandlerProxyTest;
}  // namespace test

class CompositorThreadEventQueue;
class EventWithCallback;
class InputHandlerProxyClient;
class ScrollPredictor;
class SynchronousInputHandler;
class SynchronousInputHandlerProxy;
class MomentumScrollJankTracker;

// This class is a proxy between the blink web input events for a WebWidget and
// the compositor's input handling logic. InputHandlerProxy instances live
// entirely on the compositor thread. Each InputHandler instance handles input
// events intended for a specific WebWidget.
class BLINK_PLATFORM_EXPORT InputHandlerProxy
    : public cc::InputHandlerClient,
      public SynchronousInputHandlerProxy,
      public cc::SnapFlingClient {
 public:
  InputHandlerProxy(cc::InputHandler& input_handler,
                    InputHandlerProxyClient* client,
                    bool force_input_to_main_thread);
  ~InputHandlerProxy() override;

  using WebScopedInputEvent = std::unique_ptr<blink::WebInputEvent>;

  ElasticOverscrollController* elastic_overscroll_controller() {
    return elastic_overscroll_controller_.get();
  }

  // TODO(dtapuska): Eventually move this to mojo.
  struct DidOverscrollParams {
    gfx::Vector2dF accumulated_overscroll;
    gfx::Vector2dF latest_overscroll_delta;
    gfx::Vector2dF current_fling_velocity;
    gfx::PointF causal_event_viewport_point;
    cc::OverscrollBehavior overscroll_behavior;
  };

  // Result codes returned to the client indicating the status of handling the
  // event on the compositor. Used to determine further event handling behavior
  // (i.e. should the event be forwarded to the main thread, ACK'ed to the
  // browser, etc.).
  enum EventDisposition {
    // The event was handled on the compositor and should not be forwarded to
    // the main thread.
    DID_HANDLE,

    // The compositor could not handle the event but the event may still be
    // valid for handling so it should be forwarded to the main thread.
    DID_NOT_HANDLE,

    // Set only from a touchstart that occurred while a fling was in progress.
    // Indicates that the rest of the touch stream should be sent non-blocking
    // to ensure the scroll remains smooth. Since it's non-blocking, the event
    // will be ACK'ed to the browser before being dispatched to the main
    // thread.
    // TODO(bokan): It's not clear that we need a separate status for this
    // case, why can't we just use the DID_HANDLE_NON_BLOCKING below?
    DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING,

    // Set to indicate that the event needs to be sent to the main thread (e.g.
    // because the touch event hits a touch-event handler) but the compositor
    // has determined it shouldn't be cancellable (e.g. the event handler is
    // passive). Because it isn't cancellable, the event (and future events)
    // will be sent non-blocking and be acked to the browser before being
    // dispatchehd to the main thread.
    // TODO(bokan): The semantics of DID/DID_NOT HANDLE are whether the main
    // thread needs to know about the event. In this case, we expect the event
    // to be forwarded to the main thread so this should be DID_NOT_HANDLE.
    DID_HANDLE_NON_BLOCKING,

    // The compositor didn't handle the event but has determined the main
    // thread doesn't care about the event either (e.g. it's a touch event and
    // the hit point doesn't have a touch handler). In this case, we should ACK
    // the event immediately. Both this and DID_HANDLE will avoid forwarding
    // the event to the main thread and ACK immediately; the difference is that
    // DROP_EVENT tells the client the event wasn't consumed. For example, the
    // browser may choose to use this to avoid forwarding touch events if there
    // isn't a consumer for them (and send only the scroll events).
    DROP_EVENT,

    // The compositor did handle the scroll event (so it wouldn't forward the
    // event to the main thread.) but it didn't consume the scroll so it should
    // pass it to the next consumer (either overscrolling or bubbling the event
    // to the next renderer).
    DID_HANDLE_SHOULD_BUBBLE,

    // Used only in scroll unification; the compositor couldn't determine the
    // scroll node to handle the event and requires a second try with an
    // ElementId provided by a hit test in Blink.
    REQUIRES_MAIN_THREAD_HIT_TEST,
  };
  using EventDispositionCallback = base::OnceCallback<void(
      EventDisposition,
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      std::unique_ptr<DidOverscrollParams>,
      const blink::WebInputEventAttribution&)>;
  void HandleInputEventWithLatencyInfo(
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      EventDispositionCallback callback);

  // In scroll unification, a scroll begin event may initially return unhandled
  // due to requiring the main thread to perform a hit test. In that case, the
  // client will perform the hit test by calling into Blink. When it has a
  // result, it can try handling the event again by calling back through this
  // method.
  void ContinueScrollBeginAfterMainThreadHitTest(
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      EventDispositionCallback callback,
      cc::ElementIdType hit_tests_result);

  void InjectScrollbarGestureScroll(
      const blink::WebInputEvent::Type type,
      const gfx::PointF& position_in_widget,
      const cc::InputHandlerPointerResult& pointer_result,
      const ui::LatencyInfo& latency_info,
      const base::TimeTicks now);

  // Attempts to perform attribution of the given WebInputEvent to a target
  // frame. Intended for simple impl-side hit testing.
  blink::WebInputEventAttribution PerformEventAttribution(
      const blink::WebInputEvent& event);

  // cc::InputHandlerClient implementation.
  void WillShutdown() override;
  void Animate(base::TimeTicks time) override;
  void ReconcileElasticOverscrollAndRootScroll() override;
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override;
  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override;
  void DeliverInputForHighLatencyMode() override;

  // SynchronousInputHandlerProxy implementation.
  void SetSynchronousInputHandler(
      SynchronousInputHandler* synchronous_input_handler) override;
  void SynchronouslySetRootScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void SynchronouslyZoomBy(float magnify_delta,
                           const gfx::Point& anchor) override;

  // SnapFlingClient implementation.
  bool GetSnapFlingInfoAndSetAnimatingSnapTarget(
      const gfx::Vector2dF& natural_displacement,
      gfx::Vector2dF* initial_offset,
      gfx::Vector2dF* target_offset) const override;
  gfx::Vector2dF ScrollByForSnapFling(const gfx::Vector2dF& delta) override;
  void ScrollEndForSnapFling(bool did_finish) override;
  void RequestAnimationForSnapFling() override;

  bool gesture_scroll_on_impl_thread_for_testing() const {
    return handling_gesture_on_impl_thread_;
  }

  blink::WebGestureDevice currently_active_gesture_device() const {
    return currently_active_gesture_device_.value();
  }

 protected:
  void RecordMainThreadScrollingReasons(blink::WebGestureDevice device,
                                        uint32_t reasons);
  void RecordScrollingThreadStatus(blink::WebGestureDevice device,
                                   uint32_t reasons);

 private:
  friend class test::TestInputHandlerProxy;
  friend class test::InputHandlerProxyTest;
  friend class test::UnifiedScrollingInputHandlerProxyTest;
  friend class test::InputHandlerProxyEventQueueTest;
  friend class test::InputHandlerProxyMomentumScrollJankTest;
  friend class test::InputHandlerProxyForceHandlingOnMainThread;

  void DispatchSingleInputEvent(std::unique_ptr<EventWithCallback>,
                                const base::TimeTicks);
  void DispatchQueuedInputEvents();

  // Helper functions for handling more complicated input events.
  EventDisposition HandleMouseWheel(const blink::WebMouseWheelEvent& event);
  EventDisposition HandleGestureScrollBegin(
      const blink::WebGestureEvent& event);
  EventDisposition HandleGestureScrollUpdate(
      const blink::WebGestureEvent& event,
      const blink::WebInputEventAttribution& original_attribution);
  EventDisposition HandleGestureScrollEnd(const blink::WebGestureEvent& event);
  EventDisposition HandleTouchStart(
      EventWithCallback* event_with_callback,
      const ui::LatencyInfo& original_latency_info);
  EventDisposition HandleTouchMove(
      EventWithCallback* event_with_callback,
      const ui::LatencyInfo& original_latency_info);
  EventDisposition HandleTouchEnd(EventWithCallback* event_with_callback,
                                  const ui::LatencyInfo& original_latency_info);

  const cc::InputHandlerPointerResult HandlePointerDown(
      const gfx::PointF& position,
      const ui::LatencyInfo&,
      bool has_modifier,
      base::TimeTicks timestamp,
      EventWithCallback* event_with_callback);
  const cc::InputHandlerPointerResult HandlePointerMove(
      const gfx::PointF& position,
      const ui::LatencyInfo&,
      base::TimeTicks timestamp,
      EventWithCallback* event_with_callback);
  const cc::InputHandlerPointerResult HandlePointerUp(
      const gfx::PointF& position,
      const ui::LatencyInfo&,
      base::TimeTicks timestamp,
      EventWithCallback* event_with_callback);

  void InputHandlerScrollEnd();

  // Request a frame of animation from the InputHandler or
  // SynchronousInputHandler. They can provide that by calling Animate().
  void RequestAnimation();

  // Used to send overscroll messages to the browser. It bundles the overscroll
  // params with with event ack.
  void HandleOverscroll(const gfx::PointF& causal_event_viewport_point,
                        const cc::InputHandlerScrollResult& scroll_result);

  // Update the elastic overscroll controller with |gesture_event|.
  void HandleScrollElasticityOverscroll(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  // Overrides the internal clock for testing.
  // This doesn't take the ownership of the clock. |tick_clock| must outlive the
  // InputHandlerProxy instance.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // |is_touching_scrolling_layer| indicates if one of the points that has
  // been touched hits a currently scrolling layer. |allowed_touch_action| is
  // the touch_action we are sure will be allowed for the given touch event.
  EventDisposition HitTestTouchEvent(const blink::WebTouchEvent& touch_event,
                                     bool* is_touching_scrolling_layer,
                                     cc::TouchAction* allowed_touch_action);

  EventDisposition RouteToTypeSpecificHandler(
      EventWithCallback* event_with_callback,
      const ui::LatencyInfo& original_latency_info,
      const blink::WebInputEventAttribution& original_attribution);

  void set_event_attribution_enabled(bool enabled) {
    event_attribution_enabled_ = enabled;
  }

  InputHandlerProxyClient* client_;

  // The input handler object is owned by the compositor delegate. The input
  // handler must call WillShutdown() on this class before it is deleted at
  // which point this pointer will be cleared.
  cc::InputHandler* input_handler_;

  SynchronousInputHandler* synchronous_input_handler_;

  // This should be true when a pinch is in progress. The sequence of events is
  // as follows: GSB GPB GSU GPU ... GPE GSE.
  bool handling_gesture_on_impl_thread_;

  bool gesture_pinch_in_progress_ = false;
  bool in_inertial_scrolling_ = false;
  bool scroll_sequence_ignored_;

  // Used to animate rubber-band/bounce over-scroll effect.
  std::unique_ptr<ElasticOverscrollController> elastic_overscroll_controller_;

  // The merged result of the last touch event with previous touch events
  // within a single touch sequence. This value will get returned for
  // subsequent TouchMove events to allow passive events not to block
  // scrolling.
  base::Optional<EventDisposition> touch_result_;

  // The result of the last mouse wheel event in a wheel phase sequence. This
  // value is used to determine whether the next wheel scroll is blocked on the
  // Main thread or not.
  base::Optional<EventDisposition> mouse_wheel_result_;

  // Used to record overscroll notifications while an event is being
  // dispatched.  If the event causes overscroll, the overscroll metadata is
  // bundled in the event ack, saving an IPC.
  std::unique_ptr<DidOverscrollParams> current_overscroll_params_;

  std::unique_ptr<CompositorThreadEventQueue> compositor_event_queue_;

  // Set only when the compositor input handler is handling a gesture. Tells
  // which source device is currently performing a gesture based scroll.
  base::Optional<blink::WebGestureDevice> currently_active_gesture_device_;

  // Tracks whether the first scroll update gesture event has been seen after a
  // scroll begin. This is set/reset when scroll gestures are processed in
  // HandleInputEventWithLatencyInfo and shouldn't be used outside the scope
  // of that method.
  bool has_seen_first_gesture_scroll_update_after_begin_;

  // Whether the last injected scroll gesture was a GestureScrollBegin. Used to
  // determine which GestureScrollUpdate is the first in a gesture sequence for
  // latency classification. This is separate from
  // |is_first_gesture_scroll_update_| and is used to determine which type of
  // latency component should be added for injected GestureScrollUpdates.
  bool last_injected_gesture_was_begin_;

  const base::TickClock* tick_clock_;

  std::unique_ptr<cc::SnapFlingController> snap_fling_controller_;

  std::unique_ptr<ScrollPredictor> scroll_predictor_;

  // This flag can be used to force all input to be forwarded to Blink. It's
  // used in LayoutTests to preserve existing behavior for non-threaded layout
  // tests and to allow testing both Blink and CC input handling paths.
  bool force_input_to_main_thread_;

  // These flags are set for the SkipTouchEventFilter experiment. The
  // experiment either skips filtering discrete (touch start/end) events to the
  // main thread, or all events (touch start/end/move).
  bool skip_touch_filter_discrete_ = false;
  bool skip_touch_filter_all_ = false;

  // This bit is set when the input handler proxy has requested that the client
  // perform a hit test for a scroll begin on the main thread. During that
  // time, scroll updates need to be queued. The reply from the main thread
  // will come by calling ContinueScrollBeginAfterMainThreadHitTest where the
  // queue will be flushed and this bit cleared. Used only in scroll
  // unification.
  bool hit_testing_scroll_begin_on_main_thread_ = false;

  // This bit can be used to disable event attribution in cases where the
  // hit test information is unnecessary (e.g. tests).
  bool event_attribution_enabled_ = true;

  // Helpers for the momentum scroll jank UMAs.
  std::unique_ptr<MomentumScrollJankTracker> momentum_scroll_jank_tracker_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_INPUT_HANDLER_PROXY_H_
