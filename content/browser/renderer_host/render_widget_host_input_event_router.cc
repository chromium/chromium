// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

#include <algorithm>
#include <deque>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/viz/common/features.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/frame_messages.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"

namespace {

// Transforms WebTouchEvent touch positions from the root view coordinate
// space to the target view coordinate space.
void TransformEventTouchPositions(blink::WebTouchEvent* event,
                                  const gfx::Transform& transform) {
  for (unsigned i = 0; i < event->touches_length; ++i) {
    gfx::PointF point(event->touches[i].PositionInWidget());
    transform.TransformPoint(&point);
    event->touches[i].SetPositionInWidget(point);
  }
}

blink::WebGestureEvent DummyGestureScrollUpdate(
    base::TimeTicks time_stamp,
    blink::WebGestureDevice source_device =
        blink::kWebGestureDeviceUninitialized) {
  return blink::WebGestureEvent(blink::WebInputEvent::kGestureScrollUpdate,
                                blink::WebInputEvent::kNoModifiers, time_stamp,
                                source_device);
}

gfx::PointF ComputePointInRootInPixels(
    const gfx::PointF& point,
    content::RenderWidgetHostViewBase* root_view,
    float device_scale_factor) {
  gfx::PointF point_in_root = point;
  root_view->TransformPointToRootSurface(&point_in_root);
  return gfx::ConvertPointToPixel(device_scale_factor, point_in_root);
}

bool IsMouseButtonDown(const blink::WebMouseEvent& event) {
  constexpr int mouse_button_modifiers =
      blink::WebInputEvent::kLeftButtonDown |
      blink::WebInputEvent::kMiddleButtonDown |
      blink::WebInputEvent::kRightButtonDown |
      blink::WebInputEvent::kBackButtonDown |
      blink::WebInputEvent::kForwardButtonDown;

  return event.GetModifiers() & mouse_button_modifiers;
}

}  // anonymous namespace

namespace content {

// Helper method also used from hit_test_debug_key_event_observer.cc
viz::HitTestQuery* GetHitTestQuery(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    const viz::FrameSinkId& frame_sink_id) {
  if (!frame_sink_id.is_valid())
    return nullptr;
  const auto& display_hit_test_query_map =
      host_frame_sink_manager->display_hit_test_query();
  const auto iter = display_hit_test_query_map.find(frame_sink_id);
  if (iter == display_hit_test_query_map.end())
    return nullptr;
  return iter->second.get();
}

// A class to implement a queue for tracking outbound TouchEvents, and making
// sure that their acks are returned to the appropriate root view in order.
// This is important to ensure proper operation of the GestureProvider.
// Some challenges include:
// * differentiating between native and emulated TouchEvents, as the latter ack
//   to the TouchEmulator's GestureProvider,
// * making sure all events from destroyed renderers are acked properly, and
//   without delaying acks from other renderers, and
// * making sure events are only acked if the root_view (at the time of the
//   out-bound event) is still valid.
// Some of this logic, e.g. the last item above, is shared with
// RenderWidgetHostViewBase.
class TouchEventAckQueue {
 public:
  enum class TouchEventAckStatus { TouchEventNotAcked, TouchEventAcked };
  enum class TouchEventSource { SystemTouchEvent, EmulatedTouchEvent };
  struct AckData {
    uint32_t touch_event_id;
    RenderWidgetHostViewBase* target_view;
    RenderWidgetHostViewBase* root_view;
    TouchEventSource touch_event_source;
    TouchEventAckStatus touch_event_ack_status;
    InputEventAckState ack_result;
  };

  TouchEventAckQueue() {}

  void Add(uint32_t touch_event_id,
           RenderWidgetHostViewBase* target_view,
           RenderWidgetHostViewBase* root_view,
           TouchEventSource touch_event_source,
           TouchEventAckStatus touch_event_ack_status,
           InputEventAckState ack_result);

  void Add(uint32_t touch_event_id,
           RenderWidgetHostViewBase* target_view,
           RenderWidgetHostViewBase* root_view,
           TouchEventSource touch_event_source);

  void MarkAcked(uint32_t touch_event_id,
                 InputEventAckState ack_result,
                 RenderWidgetHostViewBase* target_view);

  void UpdateQueueAfterTargetDestroyed(RenderWidgetHostViewBase* target_view);

 private:
  void ProcessAckedTouchEvents();
  void ReportTouchEventAckQueueUmaStats();

  std::deque<AckData> ack_queue_;
};

void TouchEventAckQueue::Add(uint32_t touch_event_id,
                             RenderWidgetHostViewBase* target_view,
                             RenderWidgetHostViewBase* root_view,
                             TouchEventSource touch_event_source,
                             TouchEventAckStatus touch_event_ack_status,
                             InputEventAckState ack_result) {
  AckData data = {
      touch_event_id, target_view, root_view, touch_event_source,
      touch_event_ack_status, ack_result};
  ack_queue_.push_back(data);
  if (touch_event_ack_status == TouchEventAckStatus::TouchEventAcked)
    ProcessAckedTouchEvents();
  ReportTouchEventAckQueueUmaStats();
}

void TouchEventAckQueue::Add(uint32_t touch_event_id,
                             RenderWidgetHostViewBase* target_view,
                             RenderWidgetHostViewBase* root_view,
                             TouchEventSource touch_event_source) {
  Add(touch_event_id, target_view, root_view, touch_event_source,
      TouchEventAckStatus::TouchEventNotAcked, INPUT_EVENT_ACK_STATE_UNKNOWN);
}

void TouchEventAckQueue::MarkAcked(uint32_t touch_event_id,
                                   InputEventAckState ack_result,
                                   RenderWidgetHostViewBase* target_view) {
  auto it = find_if(ack_queue_.begin(), ack_queue_.end(),
                    [touch_event_id](AckData data) {
                      return data.touch_event_id == touch_event_id;
                    });
  if (it == ack_queue_.end())
    return;
  DCHECK(it->touch_event_ack_status != TouchEventAckStatus::TouchEventAcked);
  DCHECK(target_view && target_view == it->target_view);
  it->touch_event_ack_status = TouchEventAckStatus::TouchEventAcked;
  it->ack_result = ack_result;
  ProcessAckedTouchEvents();
}

void TouchEventAckQueue::ProcessAckedTouchEvents() {
  if (ack_queue_.empty())
    return;

  // TODO(wjmaclean): modify the following loop to actually forward the acks
  // to the root_view. Must verify that the root_view at the time the event
  // was registered still exists.
  while (!ack_queue_.empty() && ack_queue_.front().touch_event_ack_status ==
                                    TouchEventAckStatus::TouchEventAcked) {
    // TODO(wjmaclean): We will eventually ack touch events to the root_view
    // here. Each ack will require confirmation that the touch event's root
    // view (at the time of event dispatch) is still valid, otherwise we just
    // discard the ack.
    ack_queue_.pop_front();
  }
}

void TouchEventAckQueue::ReportTouchEventAckQueueUmaStats() {
  size_t count = ack_queue_.size();
  UMA_HISTOGRAM_COUNTS_10000("Event.FrameEventRouting.TouchEventAckQueueSize",
                             count);
  // TODO(wjmaclean): is it worth also recording how many different renderers
  // are waiting on touch event acks at the time of reporting?
}

void TouchEventAckQueue::UpdateQueueAfterTargetDestroyed(
    RenderWidgetHostViewBase* target_view) {
  // If a queue entry's root view is being destroyed, just delete it.
  ack_queue_.erase(remove_if(ack_queue_.begin(), ack_queue_.end(),
                             [target_view](AckData data) {
                               return data.root_view == target_view;
                             }),
                   ack_queue_.end());

  // Otherwise, mark its status accordingly.
  for_each(ack_queue_.begin(), ack_queue_.end(), [target_view](AckData data) {
    if (data.target_view == target_view) {
      data.touch_event_ack_status = TouchEventAckStatus::TouchEventAcked;
      data.ack_result = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    }
  });

  ProcessAckedTouchEvents();
}

void RenderWidgetHostInputEventRouter::OnRenderWidgetHostViewBaseDestroyed(
    RenderWidgetHostViewBase* view) {
  // RenderWidgetHostViewBase::RemoveObserver() should only ever be called
  // in this function, except during the shutdown of this class. This prevents
  // removal of an observed view that is being tracked as an event target
  // without cleaning up dangling pointers to it.
  view->RemoveObserver(this);

  // Remove this view from the owner_map.
  for (auto entry : owner_map_) {
    if (entry.second == view) {
      owner_map_.erase(entry.first);
      // There will only be one instance of a particular view in the map.
      break;
    }
  }

  if (touch_emulator_)
    touch_emulator_->OnViewDestroyed(view);

  if (view == touch_target_.target) {
    touch_target_.target = nullptr;
    active_touches_ = 0;
  }
  touch_event_ack_queue_->UpdateQueueAfterTargetDestroyed(view);

  if (view == wheel_target_.target)
    wheel_target_.target = nullptr;

  // If the target that's being destroyed is in the gesture target map, we
  // replace it with nullptr so that we maintain the 1:1 correspondence between
  // map entries and the touch sequences that underly them.
  for (auto it : touchscreen_gesture_target_map_) {
    if (it.second.target == view)
      it.second.target = nullptr;
  }

  if (view == mouse_capture_target_.target)
    mouse_capture_target_.target = nullptr;

  if (view == touchscreen_gesture_target_.target)
    touchscreen_gesture_target_.target = nullptr;

  if (view == touchpad_gesture_target_.target)
    touchpad_gesture_target_.target = nullptr;

  if (view == bubbling_gesture_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target = nullptr;
    first_bubbling_scroll_target_.target = nullptr;
  } else if (view == first_bubbling_scroll_target_.target) {
    first_bubbling_scroll_target_.target = nullptr;
  }

  if (view == last_mouse_move_target_) {
    // When a child iframe is destroyed, consider its parent to be to be the
    // most recent target, if possible. In some cases the parent might already
    // have been destroyed, in which case the last target is cleared.
    if (view != last_mouse_move_root_view_) {
      DCHECK(last_mouse_move_target_->IsRenderWidgetHostViewChildFrame());
      last_mouse_move_target_ =
          static_cast<RenderWidgetHostViewChildFrame*>(last_mouse_move_target_)
              ->GetParentView();
    } else {
      last_mouse_move_target_ = nullptr;
    }

    if (!last_mouse_move_target_ || view == last_mouse_move_root_view_)
      last_mouse_move_root_view_ = nullptr;
  }

  if (view == last_fling_start_target_)
    last_fling_start_target_ = nullptr;

  if (view == last_fling_start_bubbled_target_)
    last_fling_start_bubbled_target_ = nullptr;

  event_targeter_->ViewWillBeDestroyed(view);
}

void RenderWidgetHostInputEventRouter::ClearAllObserverRegistrations() {
  // Since we're shutting down, it's safe to call RenderWidgetHostViewBase::
  // RemoveObserver() directly here.
  for (auto entry : owner_map_)
    entry.second->RemoveObserver(this);
  owner_map_.clear();
}

RenderWidgetHostInputEventRouter::HittestDelegate::HittestDelegate(
    const std::unordered_map<viz::SurfaceId, HittestData, viz::SurfaceIdHash>&
        hittest_data)
    : hittest_data_(hittest_data) {}

bool RenderWidgetHostInputEventRouter::HittestDelegate::RejectHitTarget(
    const viz::SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  auto it = hittest_data_.find(surface_quad->surface_range.end());
  if (it != hittest_data_.end() && it->second.ignored_for_hittest)
    return true;
  return false;
}

bool RenderWidgetHostInputEventRouter::HittestDelegate::AcceptHitTarget(
    const viz::SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  auto it = hittest_data_.find(surface_quad->surface_range.end());
  if (it != hittest_data_.end() && !it->second.ignored_for_hittest)
    return true;
  return false;
}

RenderWidgetHostInputEventRouter::RenderWidgetHostInputEventRouter()
    : touchscreen_gesture_target_in_map_(false),
      last_mouse_move_target_(nullptr),
      last_mouse_move_root_view_(nullptr),
      last_emulated_event_root_view_(nullptr),
      last_device_scale_factor_(1.f),
      active_touches_(0),
      in_touchscreen_gesture_pinch_(false),
      gesture_pinch_did_send_scroll_begin_(false),
      event_targeter_(std::make_unique<RenderWidgetTargeter>(this)),
      use_viz_hit_test_(features::IsVizHitTestingEnabled()),
      touch_event_ack_queue_(new TouchEventAckQueue),
      weak_ptr_factory_(this) {}

RenderWidgetHostInputEventRouter::~RenderWidgetHostInputEventRouter() {
  // We may be destroyed before some of the owners in the map, so we must
  // remove ourself from their observer lists.
  ClearAllObserverRegistrations();
}

RenderWidgetTargetResult RenderWidgetHostInputEventRouter::FindMouseEventTarget(
    RenderWidgetHostViewBase* root_view,
    const blink::WebMouseEvent& event) const {
  RenderWidgetHostViewBase* target = nullptr;
  bool needs_transform_point = true;
  bool latched_target = true;
  bool should_verify_result = false;
  if (root_view->IsMouseLocked()) {
    target = root_view->host()->delegate()->GetMouseLockWidget()->GetView();
  }

  // Ignore mouse_capture_target_ if there are no mouse buttons currently down
  // because this is only for the purpose of dragging.
  if (!target && mouse_capture_target_.target &&
      (event.GetType() == blink::WebInputEvent::kMouseUp ||
       IsMouseButtonDown(event))) {
    target = mouse_capture_target_.target;
  }

  gfx::PointF transformed_point;
  if (!target) {
    latched_target = false;
    auto result = FindViewAtLocation(
        root_view, event.PositionInWidget(), event.PositionInScreen(),
        viz::EventSource::MOUSE, &transformed_point);
    // Due to performance concerns we do not verify mouse move events.
    should_verify_result = (event.GetType() == blink::WebInputEvent::kMouseMove)
                               ? false
                               : result.should_verify_result;
    if (result.should_query_view) {
      DCHECK(!should_verify_result);
      return {result.view, true, transformed_point, latched_target,
              should_verify_result};
    }
    target = result.view;
    // |transformed_point| is already transformed.
    needs_transform_point = false;
  }

  if (needs_transform_point) {
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), target, &transformed_point,
            viz::EventSource::MOUSE)) {
      return {nullptr, false, base::nullopt, latched_target, false};
    }
  }
  return {target, false, transformed_point, latched_target,
          should_verify_result};
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindMouseWheelEventTarget(
    RenderWidgetHostViewBase* root_view,
    const blink::WebMouseWheelEvent& event) const {
  RenderWidgetHostViewBase* target = nullptr;
  gfx::PointF transformed_point;
  if (root_view->IsMouseLocked()) {
    target = root_view->host()->delegate()->GetMouseLockWidget()->GetView();
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), target, &transformed_point,
            viz::EventSource::MOUSE)) {
      return {nullptr, false, base::nullopt, true, false};
    }
    return {target, false, transformed_point, true, false};
  }

  if (event.phase == blink::WebMouseWheelEvent::kPhaseBegan) {
    auto result = FindViewAtLocation(
        root_view, event.PositionInWidget(), event.PositionInScreen(),
        viz::EventSource::MOUSE, &transformed_point);
    return {result.view, result.should_query_view, transformed_point, false,
            result.should_verify_result};
  }
  // For non-begin events, the target found for the previous phaseBegan is
  // used.
  return {nullptr, false, base::nullopt, true, false};
}

RenderWidgetTargetResult RenderWidgetHostInputEventRouter::FindViewAtLocation(
    RenderWidgetHostViewBase* root_view,
    const gfx::PointF& point,
    const gfx::PointF& point_in_screen,
    viz::EventSource source,
    gfx::PointF* transformed_point) const {
  // Short circuit if owner_map has only one RenderWidgetHostView, no need for
  // hit testing.
  if (owner_map_.size() <= 1) {
    *transformed_point = point;
    return {root_view, false, *transformed_point, false, false};
  }

  viz::FrameSinkId frame_sink_id;
  bool query_renderer = false;
  bool should_verify_result = false;
  if (use_viz_hit_test_) {
    viz::HitTestQuery* query = GetHitTestQuery(GetHostFrameSinkManager(),
                                               root_view->GetRootFrameSinkId());
    if (!query)
      return {root_view, false, base::nullopt, false, false};
    // |point_in_screen| is in the coordinate space of of the screen, but the
    // display HitTestQuery does a hit test in the coordinate space of the root
    // window. The following translation should account for that discrepancy.
    // TODO(riajiang): Get rid of |point_in_screen| since it's not used.
    float device_scale_factor = root_view->GetDeviceScaleFactor();
    DCHECK_GT(device_scale_factor, 0.0f);
    gfx::PointF point_in_root_in_pixels =
        ComputePointInRootInPixels(point, root_view, device_scale_factor);
    viz::Target target =
        query->FindTargetForLocation(source, point_in_root_in_pixels);
    frame_sink_id = target.frame_sink_id;
    if (frame_sink_id.is_valid()) {
      *transformed_point = gfx::ConvertPointToDIP(device_scale_factor,
                                                  target.location_in_target);
    } else {
      *transformed_point = point;
    }
    // To ensure the correctness of viz hit testing with cc generated data, we
    // verify hit test results when:
    // a) We use cc generated data to do synchronous hit testing and
    // b) We use HitTestQuery to find the target (instead of reusing previous
    // targets when hit testing latched events) and
    // c) We are not hit testing MouseMove events which is too frequent to
    // verify it without impacting performance.
    // The code that implements c) locates in |FindMouseEventTarget|.
    if (target.flags & viz::HitTestRegionFlags::kHitTestAsk)
      query_renderer = true;
    else if (features::IsVizHitTestingSurfaceLayerEnabled())
      should_verify_result = true;
  } else {
    // The hittest delegate is used to reject hittesting quads based on extra
    // hittesting data send by the renderer.
    HittestDelegate delegate(hittest_data_);

    // The conversion of point to transform_point is done over the course of the
    // hit testing, and reflect transformations that would normally be applied
    // in the renderer process if the event was being routed between frames
    // within a single process with only one RenderWidgetHost.
    frame_sink_id = root_view->FrameSinkIdAtPoint(
        &delegate, point, transformed_point, &query_renderer);
  }

  auto* view = FindViewFromFrameSinkId(frame_sink_id);
  // Send the event to |root_view| if |view| is not in |root_view|'s sub-tree
  // anymore.
  if (!view || (RenderWidgetHostViewGuest::GetRootView(view) != root_view)) {
    view = root_view;
    *transformed_point = point;
  }

  return {view, query_renderer, *transformed_point, false,
          should_verify_result};
}

void RenderWidgetHostInputEventRouter::RouteMouseEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebMouseEvent* event,
    const ui::LatencyInfo& latency) {
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::DispatchMouseEvent(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebMouseEvent& mouse_event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location) {
  // TODO(wjmaclean): Should we be sending a no-consumer ack to the root_view
  // if there is no target?
  if (!target)
    return;

  // Implicitly release any capture when a MouseUp arrives, so that if any
  // events arrive before the renderer can explicitly release capture, we can
  // target those correctly. This also releases if there are no mouse buttons
  // down, which is to protect against problems that can occur on some
  // platforms where MouseUps are not received when the mouse cursor is off the
  // browser window.
  // Also, this is strictly necessary for touch emulation.
  if (mouse_event.GetType() == blink::WebInputEvent::kMouseUp ||
      !IsMouseButtonDown(mouse_event))
    mouse_capture_target_.target = nullptr;

  // When touch emulation is active, mouse events have to act like touch
  // events, which requires that there be implicit capture between MouseDown
  // and MouseUp.
  if (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
      touch_emulator_ && touch_emulator_->enabled()) {
    mouse_capture_target_.target = target;
  }

  DCHECK(target_location.has_value());
  blink::WebMouseEvent event = mouse_event;
  event.SetPositionInWidget(target_location->x(), target_location->y());

  // SendMouseEnterOrLeaveEvents is called with the original event
  // coordinates, which are transformed independently for each view that will
  // receive an event. Also, since the view under the mouse has changed,
  // notify the CursorManager that it might need to change the cursor.
  if ((event.GetType() == blink::WebInputEvent::kMouseLeave ||
       event.GetType() == blink::WebInputEvent::kMouseMove) &&
      target != last_mouse_move_target_ && !root_view->IsMouseLocked()) {
    SendMouseEnterOrLeaveEvents(mouse_event, target, root_view);
    if (root_view->GetCursorManager())
      root_view->GetCursorManager()->UpdateViewUnderCursor(target);
  }

  target->ProcessMouseEvent(event, latency);
}

void RenderWidgetHostInputEventRouter::RouteMouseWheelEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebMouseWheelEvent* event,
    const ui::LatencyInfo& latency) {
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::DispatchMouseWheelEvent(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebMouseWheelEvent& mouse_wheel_event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location) {
  base::Optional<gfx::PointF> point_in_target = target_location;
  if (!root_view->IsMouseLocked()) {
    if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseBegan) {
      wheel_target_.target = target;
      if (target_location.has_value()) {
        wheel_target_.delta =
            target_location.value() - mouse_wheel_event.PositionInWidget();
      }
    } else {
      if (wheel_target_.target) {
        DCHECK(!target && !target_location.has_value());
        target = wheel_target_.target;
        point_in_target.emplace(mouse_wheel_event.PositionInWidget() +
                                wheel_target_.delta);
      } else if ((mouse_wheel_event.phase ==
                      blink::WebMouseWheelEvent::kPhaseEnded ||
                  mouse_wheel_event.momentum_phase ==
                      blink::WebMouseWheelEvent::kPhaseEnded) &&
                 bubbling_gesture_scroll_target_.target) {
        // Send a GSE to the bubbling target and cancel scroll bubbling since
        // the wheel target view is destroyed and the wheel end event won't get
        // processed.
        blink::WebGestureEvent fake_scroll_update =
            DummyGestureScrollUpdate(mouse_wheel_event.TimeStamp(),
                                     bubbling_gesture_scroll_source_device_);
        SendGestureScrollEnd(bubbling_gesture_scroll_target_.target,
                             fake_scroll_update);
        bubbling_gesture_scroll_target_.target = nullptr;
        first_bubbling_scroll_target_.target = nullptr;
      }
    }
  }

  if (!target) {
    root_view->WheelEventAck(mouse_wheel_event,
                             INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }
  // If target_location doesn't have a value, it can be for two reasons:
  // 1. |target| is null, in which case we would have early returned from the
  // check above.
  // 2. The event we are receiving is not a phaseBegan, in which case we should
  // have got a valid |point_in_target| from wheel_target_.delta above.
  DCHECK(point_in_target.has_value());

  blink::WebMouseWheelEvent event = mouse_wheel_event;
  event.SetPositionInWidget(point_in_target->x(), point_in_target->y());
  target->ProcessMouseWheelEvent(event, latency);

  if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseEnded ||
      mouse_wheel_event.momentum_phase ==
          blink::WebMouseWheelEvent::kPhaseEnded) {
    wheel_target_.target = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::RouteGestureEvent(
    RenderWidgetHostViewBase* root_view,
    const blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  if (event->IsTargetViewport()) {
    root_view->ProcessGestureEvent(*event, latency);
    return;
  }

  switch (event->SourceDevice()) {
    case blink::kWebGestureDeviceUninitialized:
    case blink::kWebGestureDeviceCount:
      NOTREACHED() << "Uninitialized device type is not allowed";
      break;
    case blink::kWebGestureDeviceSyntheticAutoscroll:
      NOTREACHED() << "Only target_viewport synthetic autoscrolls are "
                      "currently supported";
      break;
    case blink::kWebGestureDeviceTouchpad:
      RouteTouchpadGestureEvent(root_view, event, latency);
      break;
    case blink::kWebGestureDeviceTouchscreen:
      RouteTouchscreenGestureEvent(root_view, event, latency);
      break;
  };
}

namespace {

unsigned CountChangedTouchPoints(const blink::WebTouchEvent& event) {
  unsigned changed_count = 0;

  blink::WebTouchPoint::State required_state =
      blink::WebTouchPoint::kStateUndefined;
  switch (event.GetType()) {
    case blink::WebInputEvent::kTouchStart:
      required_state = blink::WebTouchPoint::kStatePressed;
      break;
    case blink::WebInputEvent::kTouchEnd:
      required_state = blink::WebTouchPoint::kStateReleased;
      break;
    case blink::WebInputEvent::kTouchCancel:
      required_state = blink::WebTouchPoint::kStateCancelled;
      break;
    default:
      // We'll only ever call this method for TouchStart, TouchEnd
      // and TounchCancel events, so mark the rest as not-reached.
      NOTREACHED();
  }
  for (unsigned i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state == required_state)
      ++changed_count;
  }

  DCHECK(event.GetType() == blink::WebInputEvent::kTouchCancel ||
         changed_count == 1);
  return changed_count;
}

}  // namespace

// Any time a touch start event is handled/consumed/default prevented it is
// removed from the gesture map, because it will never create a gesture.
void RenderWidgetHostInputEventRouter::OnHandledTouchStartOrFirstTouchMove(
    uint32_t unique_touch_event_id) {
  // unique_touch_event_id of 0 implies a gesture not created by a touch.
  DCHECK_NE(unique_touch_event_id, 0U);
  touchscreen_gesture_target_map_.erase(unique_touch_event_id);
}

RenderWidgetTargetResult RenderWidgetHostInputEventRouter::FindTouchEventTarget(
    RenderWidgetHostViewBase* root_view,
    const blink::WebTouchEvent& event) {
  // Tests may call this without an initial TouchStart, so check event type
  // explicitly here.
  if (active_touches_ || event.GetType() != blink::WebInputEvent::kTouchStart)
    return {nullptr, false, base::nullopt, true, false};

  active_touches_ += CountChangedTouchPoints(event);
  gfx::PointF original_point = gfx::PointF(event.touches[0].PositionInWidget());
  gfx::PointF original_point_in_screen(event.touches[0].PositionInScreen());
  gfx::PointF transformed_point;

  return FindViewAtLocation(root_view, original_point, original_point_in_screen,
                            viz::EventSource::TOUCH, &transformed_point);
}

void RenderWidgetHostInputEventRouter::DispatchTouchEvent(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebTouchEvent& touch_event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location,
    bool is_emulated_touchevent) {
  DCHECK(blink::WebInputEvent::IsTouchEventType(touch_event.GetType()) &&
         touch_event.GetType() != blink::WebInputEvent::kTouchScrollStarted);

  bool is_sequence_start = !touch_target_.target && target;
  if (is_sequence_start) {
    touch_target_.target = target;
    // For now we only compute the transform at TouchStart, but in a follow-on
    // CL this will be computed for all events in order to account for css
    // animations, pinches, etc.
    if (!root_view->GetTransformToViewCoordSpace(touch_target_.target,
                                                 &touch_target_.transform)) {
      // Fall-back to just using the delta if we are unable to get the full
      // transform.
      touch_target_.transform.MakeIdentity();
      if (target_location.has_value()) {
        touch_target_.transform.Translate(
            target_location.value() -
            touch_event.touches[0].PositionInWidget());
      }
    }

    DCHECK(touchscreen_gesture_target_map_.find(
               touch_event.unique_touch_event_id) ==
           touchscreen_gesture_target_map_.end());
    touchscreen_gesture_target_map_[touch_event.unique_touch_event_id] =
        touch_target_;
  } else if (touch_event.GetType() == blink::WebInputEvent::kTouchStart) {
    active_touches_ += CountChangedTouchPoints(touch_event);
  }

  // Test active_touches_ before decrementing, since its value can be
  // reset to 0 in OnRenderWidgetHostViewBaseDestroyed, and this can
  // happen between the TouchStart and a subsequent TouchMove/End/Cancel.
  if ((touch_event.GetType() == blink::WebInputEvent::kTouchEnd ||
       touch_event.GetType() == blink::WebInputEvent::kTouchCancel) &&
      active_touches_) {
    active_touches_ -= CountChangedTouchPoints(touch_event);
  }
  DCHECK_GE(active_touches_, 0);

  // Debugging for crbug.com/814674.
  if (touch_target_.target && !IsViewInMap(touch_target_.target)) {
    NOTREACHED() << "Touch events should not be routed to a destroyed target "
                    "View.";
    touch_target_.target = nullptr;
    base::debug::DumpWithoutCrashing();
  }

  TouchEventAckQueue::TouchEventSource event_source =
      is_emulated_touchevent
          ? TouchEventAckQueue::TouchEventSource::EmulatedTouchEvent
          : TouchEventAckQueue::TouchEventSource::SystemTouchEvent;
  if (!touch_target_.target) {
    touch_event_ack_queue_->Add(
        touch_event.unique_touch_event_id, nullptr, root_view, event_source,
        TouchEventAckQueue::TouchEventAckStatus::TouchEventAcked,
        INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    TouchEventWithLatencyInfo touch_with_latency(touch_event, latency);
    root_view->ProcessAckedTouchEvent(touch_with_latency,
                                      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }

  if (is_sequence_start) {
    if (touch_target_.target == bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(
          bubbling_gesture_scroll_target_.target,
          DummyGestureScrollUpdate(touch_event.TimeStamp(),
                                   bubbling_gesture_scroll_source_device_));
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  touch_event_ack_queue_->Add(touch_event.unique_touch_event_id,
                              touch_target_.target, root_view, event_source);

  blink::WebTouchEvent event(touch_event);
  TransformEventTouchPositions(&event, touch_target_.transform);
  touch_target_.target->ProcessTouchEvent(event, latency);

  if (!active_touches_)
    touch_target_.target = nullptr;
}

void RenderWidgetHostInputEventRouter::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& event,
    InputEventAckState ack_result,
    RenderWidgetHostViewBase* view) {
  touch_event_ack_queue_->MarkAcked(event.event.unique_touch_event_id,
                                    ack_result, view);
  // TODO(wjmaclean): Eventually we will keep track of which outgoing touch
  // events are emulated and which aren't, so the decision to hand off to the
  // touch emulator won't just rely on the existence of the touch emulator.
  if (touch_emulator_ &&
      touch_emulator_->HandleTouchEventAck(event.event, ack_result)) {
    return;
  }

  if (!view)
    return;

  auto* root_view = view->GetRootView();
  if (!root_view)
    return;

  root_view->ProcessAckedTouchEvent(event, ack_result);
}

void RenderWidgetHostInputEventRouter::RouteTouchEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebTouchEvent* event,
    const ui::LatencyInfo& latency) {
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::SendMouseEnterOrLeaveEvents(
    const blink::WebMouseEvent& event,
    RenderWidgetHostViewBase* target,
    RenderWidgetHostViewBase* root_view) {
  // This method treats RenderWidgetHostViews as a tree, where the mouse
  // cursor is potentially leaving one node and entering another somewhere
  // else in the tree. Since iframes are graphically self-contained (i.e. an
  // iframe can't have a descendant that renders outside of its rect
  // boundaries), all affected RenderWidgetHostViews are ancestors of either
  // the node being exited or the node being entered.
  // Approach:
  // 1. Find lowest common ancestor (LCA) of the last view and current target
  //    view.
  // 2. The last view, and its ancestors up to but not including the LCA,
  //    receive a MouseLeave.
  // 3. The LCA itself, unless it is the new target, receives a MouseOut
  //    because the cursor has passed between elements within its bounds.
  // 4. The new target view's ancestors, up to but not including the LCA,
  //    receive a MouseEnter.
  // Ordering does not matter since these are handled asynchronously relative
  // to each other.

  // If the mouse has moved onto a different root view (typically meaning it
  // has crossed over a popup or context menu boundary), then we invalidate
  // last_mouse_move_target_ because we have no reference for its coordinate
  // space.
  if (root_view != last_mouse_move_root_view_)
    last_mouse_move_target_ = nullptr;

  // Finding the LCA uses a standard approach. We build vectors of the
  // ancestors of each node up to the root, and then remove common ancestors.
  std::vector<RenderWidgetHostViewBase*> entered_views;
  std::vector<RenderWidgetHostViewBase*> exited_views;
  RenderWidgetHostViewBase* cur_view = target;
  entered_views.push_back(cur_view);
  // Non-root RWHVs are guaranteed to be RenderWidgetHostViewChildFrames,
  // as long as they are the only embeddable RWHVs.
  while (cur_view->IsRenderWidgetHostViewChildFrame()) {
    cur_view =
        static_cast<RenderWidgetHostViewChildFrame*>(cur_view)->GetParentView();
    // cur_view can possibly be nullptr for guestviews that are not currently
    // connected to the webcontents tree.
    if (!cur_view) {
      last_mouse_move_target_ = target;
      last_mouse_move_root_view_ = root_view;
      return;
    }
    entered_views.push_back(cur_view);
  }

  // On Windows, it appears to be possible that render widget targeting could
  // produce a target that is outside of the specified root. For now, we'll
  // just give up in such a case. See https://crbug.com/851958.
  if (cur_view != root_view)
    return;

  cur_view = last_mouse_move_target_;
  if (cur_view) {
    exited_views.push_back(cur_view);
    while (cur_view->IsRenderWidgetHostViewChildFrame()) {
      cur_view = static_cast<RenderWidgetHostViewChildFrame*>(cur_view)
                     ->GetParentView();
      if (!cur_view) {
        last_mouse_move_target_ = target;
        last_mouse_move_root_view_ = root_view;
        return;
      }
      exited_views.push_back(cur_view);
    }
    DCHECK_EQ(cur_view, root_view);
  }

  // This removes common ancestors from the root downward.
  RenderWidgetHostViewBase* common_ancestor = nullptr;
  while (entered_views.size() > 0 && exited_views.size() > 0 &&
         entered_views.back() == exited_views.back()) {
    common_ancestor = entered_views.back();
    entered_views.pop_back();
    exited_views.pop_back();
  }

  gfx::PointF transformed_point;
  // Send MouseLeaves.
  for (auto* view : exited_views) {
    blink::WebMouseEvent mouse_leave(event);
    mouse_leave.SetType(blink::WebInputEvent::kMouseLeave);
    // There is a chance of a race if the last target has recently created a
    // new compositor surface. The SurfaceID for that might not have
    // propagated to its embedding surface, which makes it impossible to
    // compute the transformation for it
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), view, &transformed_point,
            viz::EventSource::MOUSE)) {
      transformed_point = gfx::PointF();
    }
    mouse_leave.SetPositionInWidget(transformed_point.x(),
                                    transformed_point.y());
    view->ProcessMouseEvent(mouse_leave, ui::LatencyInfo());
  }

  // The ancestor might need to trigger MouseOut handlers.
  if (common_ancestor && common_ancestor != target) {
    blink::WebMouseEvent mouse_move(event);
    mouse_move.SetType(blink::WebInputEvent::kMouseMove);
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), common_ancestor, &transformed_point,
            viz::EventSource::MOUSE)) {
      transformed_point = gfx::PointF();
    }
    mouse_move.SetPositionInWidget(transformed_point.x(),
                                   transformed_point.y());
    common_ancestor->ProcessMouseEvent(mouse_move, ui::LatencyInfo());
  }

  // Send MouseMoves to trigger MouseEnter handlers.
  for (auto* view : entered_views) {
    if (view == target)
      continue;
    blink::WebMouseEvent mouse_enter(event);
    mouse_enter.SetType(blink::WebInputEvent::kMouseMove);
    if (!root_view->TransformPointToCoordSpaceForView(
            event.PositionInWidget(), view, &transformed_point,
            viz::EventSource::MOUSE)) {
      transformed_point = gfx::PointF();
    }
    mouse_enter.SetPositionInWidget(transformed_point.x(),
                                    transformed_point.y());
    view->ProcessMouseEvent(mouse_enter, ui::LatencyInfo());
  }

  last_mouse_move_target_ = target;
  last_mouse_move_root_view_ = root_view;
}

void RenderWidgetHostInputEventRouter::ReportBubblingScrollToSameView(
    const blink::WebGestureEvent& event,
    const RenderWidgetHostViewBase* view) {
#if 0
  // For now, we've disabled the DumpWithoutCrashing as it's no longer
  // providing useful information.
  // TODO(828422): Determine useful crash keys and reenable the report.
  base::debug::DumpWithoutCrashing();
#endif
}

namespace {

// Given |event| in root coordinates, return an event in |target_view|'s
// coordinates.
blink::WebGestureEvent GestureEventInTarget(
    const blink::WebGestureEvent& event,
    RenderWidgetHostViewBase* target_view) {
  const gfx::PointF point_in_target =
      target_view->TransformRootPointToViewCoordSpace(event.PositionInWidget());
  blink::WebGestureEvent event_for_target(event);
  event_for_target.SetPositionInWidget(point_in_target);
  return event_for_target;
}

}  // namespace

void RenderWidgetHostInputEventRouter::BubbleScrollEvent(
    RenderWidgetHostViewBase* target_view,
    const blink::WebGestureEvent& event,
    const RenderWidgetHostViewBase* resending_view) {
  DCHECK(target_view);
  DCHECK(event.GetType() == blink::WebInputEvent::kGestureScrollBegin ||
         event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
         event.GetType() == blink::WebInputEvent::kGestureFlingStart ||
         event.GetType() == blink::WebInputEvent::kGestureFlingCancel);

  ui::LatencyInfo latency_info =
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event);

  if (event.GetType() == blink::WebInputEvent::kGestureScrollBegin) {
    // If target_view has unrelated gesture events in progress, do
    // not proceed. This could cause confusion between independent
    // scrolls.
    if (target_view == touchscreen_gesture_target_.target ||
        target_view == touchpad_gesture_target_.target ||
        target_view == touch_target_.target) {
      return;
    }

    // This accounts for bubbling through nested OOPIFs. A gesture scroll
    // begin has been bubbled but the target has sent back a gesture scroll
    // event ack which didn't consume any scroll delta, and so another level
    // of bubbling is needed. This requires a GestureScrollEnd be sent to the
    // last view, which will no longer be the scroll target.
    if (bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(
          bubbling_gesture_scroll_target_.target,
          GestureEventInTarget(event, bubbling_gesture_scroll_target_.target));
    } else {
      first_bubbling_scroll_target_.target = target_view;
    }

    bubbling_gesture_scroll_target_.target = target_view;
    bubbling_gesture_scroll_source_device_ = event.SourceDevice();
  } else if (event.GetType() == blink::WebInputEvent::kGestureFlingCancel) {
    // TODO(828422): Remove once this issue no longer occurs.
    if (resending_view == last_fling_start_bubbled_target_) {
      ReportBubblingScrollToSameView(event, resending_view);
      last_fling_start_bubbled_target_ = nullptr;
      return;
    }
    // GFC event must get bubbled to the same target view that the last GFS has
    // been bubbled.
    if (last_fling_start_bubbled_target_) {
      last_fling_start_bubbled_target_->ProcessGestureEvent(
          GestureEventInTarget(event, last_fling_start_bubbled_target_),
          latency_info);
      last_fling_start_bubbled_target_ = nullptr;
    }
    return;
  } else {  // !(event.GetType() == blink::WebInputEvent::kGestureScrollBegin)
            // && !(event.GetType() ==
            // blink::WebInputEvent::kGestureFlingCancel)

    if (!bubbling_gesture_scroll_target_.target) {
      // The GestureScrollBegin event is not bubbled, don't bubble the rest of
      // the scroll events.
      return;
    }

    // Don't bubble the GSE events that are generated and sent to intermediate
    // bubbling targets.
    if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd &&
        target_view != first_bubbling_scroll_target_.target) {
      return;
    }
  }

  // If the router tries to resend a gesture scroll event back to the same
  // view, we could hang.
  DCHECK_NE(resending_view, bubbling_gesture_scroll_target_.target);
  // We've seen reports of this, but don't know the cause yet. For now,
  // instead of CHECKing or hanging, we'll report the issue and abort scroll
  // bubbling.
  // TODO(828422): Remove once this issue no longer occurs.
  if (resending_view == bubbling_gesture_scroll_target_.target) {
    ReportBubblingScrollToSameView(event, resending_view);
    first_bubbling_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_source_device_ =
        blink::kWebGestureDeviceUninitialized;
    return;
  }

  bubbling_gesture_scroll_target_.target->ProcessGestureEvent(
      GestureEventInTarget(event, bubbling_gesture_scroll_target_.target),
      latency_info);

  // The GFC should be sent to the view that handles the GFS.
  if (event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    last_fling_start_bubbled_target_ = bubbling_gesture_scroll_target_.target;
  }

  if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
      event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    first_bubbling_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_source_device_ =
        blink::kWebGestureDeviceUninitialized;
  }
}

void RenderWidgetHostInputEventRouter::SendGestureScrollBegin(
    RenderWidgetHostViewBase* view,
    const blink::WebGestureEvent& event) {
  blink::WebGestureEvent scroll_begin(event);
  scroll_begin.SetType(blink::WebInputEvent::kGestureScrollBegin);
  switch (event.GetType()) {
    case blink::WebInputEvent::kGestureScrollUpdate:
      scroll_begin.data.scroll_begin.delta_x_hint =
          event.data.scroll_update.delta_x;
      scroll_begin.data.scroll_begin.delta_y_hint =
          event.data.scroll_update.delta_y;
      scroll_begin.data.scroll_begin.delta_hint_units =
          event.data.scroll_update.delta_units;
      break;
    case blink::WebInputEvent::kGesturePinchBegin:
      scroll_begin.data.scroll_begin.delta_x_hint = 0;
      scroll_begin.data.scroll_begin.delta_y_hint = 0;
      scroll_begin.data.scroll_begin.delta_hint_units =
          blink::WebGestureEvent::kPrecisePixels;
      break;
    default:
      NOTREACHED();
  }
  view->ProcessGestureEvent(
      scroll_begin,
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event));
}

void RenderWidgetHostInputEventRouter::SendGestureScrollEnd(
    RenderWidgetHostViewBase* view,
    const blink::WebGestureEvent& event) {
  blink::WebGestureEvent scroll_end(event);
  scroll_end.SetType(blink::WebInputEvent::kGestureScrollEnd);
  scroll_end.SetTimeStamp(base::TimeTicks::Now());
  switch (event.GetType()) {
    case blink::WebInputEvent::kGestureScrollBegin:
      scroll_end.data.scroll_end.inertial_phase =
          event.data.scroll_begin.inertial_phase;
      scroll_end.data.scroll_end.delta_units =
          event.data.scroll_begin.delta_hint_units;
      break;
    case blink::WebInputEvent::kGestureScrollUpdate:
      scroll_end.data.scroll_end.inertial_phase =
          event.data.scroll_update.inertial_phase;
      scroll_end.data.scroll_end.delta_units =
          event.data.scroll_update.delta_units;
      break;
    case blink::WebInputEvent::kGesturePinchEnd:
      scroll_end.data.scroll_end.inertial_phase =
          blink::WebGestureEvent::kUnknownMomentumPhase;
      scroll_end.data.scroll_end.delta_units =
          blink::WebGestureEvent::kPrecisePixels;
      break;
    default:
      NOTREACHED();
  }
  view->ProcessGestureEvent(
      scroll_end,
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event));
}

void RenderWidgetHostInputEventRouter::CancelScrollBubbling(
    RenderWidgetHostViewBase* target_view) {
  DCHECK(target_view);
  if (target_view == first_bubbling_scroll_target_.target) {
    first_bubbling_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_source_device_ =
        blink::kWebGestureDeviceUninitialized;
  }
}

void RenderWidgetHostInputEventRouter::AddFrameSinkIdOwner(
    const viz::FrameSinkId& id,
    RenderWidgetHostViewBase* owner) {
  DCHECK(owner_map_.find(id) == owner_map_.end());
  // We want to be notified if the owner is destroyed so we can remove it from
  // our map.
  owner->AddObserver(this);
  owner_map_.insert(std::make_pair(id, owner));
}

void RenderWidgetHostInputEventRouter::RemoveFrameSinkIdOwner(
    const viz::FrameSinkId& id) {
  auto it_to_remove = owner_map_.find(id);
  if (it_to_remove != owner_map_.end()) {
    // If we remove a view from the observer list, we need to be sure to do a
    // cleanup of the various targets and target maps, else we will end up with
    // stale values if the view destructs and isn't an observer anymore.
    // Note: the view the iterator points at will be deleted in the following
    // call, and shouldn't be used after this point.
    OnRenderWidgetHostViewBaseDestroyed(it_to_remove->second);
  }

  for (auto it = hittest_data_.begin(); it != hittest_data_.end();) {
    if (it->first.frame_sink_id() == id)
      it = hittest_data_.erase(it);
    else
      ++it;
  }
}

void RenderWidgetHostInputEventRouter::OnHittestData(
    const FrameHostMsg_HittestData_Params& params) {
  if (owner_map_.find(params.surface_id.frame_sink_id()) == owner_map_.end()) {
    return;
  }
  HittestData data;
  data.ignored_for_hittest = params.ignored_for_hittest;
  hittest_data_[params.surface_id] = data;
}

RenderWidgetHostImpl*
RenderWidgetHostInputEventRouter::GetRenderWidgetHostAtPoint(
    RenderWidgetHostViewBase* root_view,
    const gfx::PointF& point,
    gfx::PointF* transformed_point) {
  if (!root_view)
    return nullptr;
  gfx::PointF point_in_screen =
      point + root_view->GetViewBounds().OffsetFromOrigin();
  return RenderWidgetHostImpl::From(
      FindViewAtLocation(root_view, point, point_in_screen,
                         viz::EventSource::MOUSE, transformed_point)
          .view->GetRenderWidgetHost());
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTouchscreenGestureEventTarget(
    RenderWidgetHostViewBase* root_view,
    const blink::WebGestureEvent& gesture_event) {
  // Since DispatchTouchscreenGestureEvent() doesn't pay any attention to the
  // target we could just return nullptr for pinch events, but since we know
  // where they are going we return the correct target.
  if (blink::WebInputEvent::IsPinchGestureEventType(gesture_event.GetType()))
    return {root_view, false, gesture_event.PositionInWidget(), true, false};

  // Android sends gesture events that have no corresponding touch sequence, so
  // these we hit-test explicitly.
  if (gesture_event.unique_touch_event_id == 0) {
    gfx::PointF transformed_point;
    gfx::PointF original_point(gesture_event.PositionInWidget());
    gfx::PointF original_point_in_screen(gesture_event.PositionInScreen());
    return FindViewAtLocation(root_view, original_point,
                              original_point_in_screen, viz::EventSource::TOUCH,
                              &transformed_point);
  }

  // Remaining gesture events will defer to the gesture event target queue
  // during dispatch.
  return {nullptr, false, base::nullopt, true, false};
}

bool RenderWidgetHostInputEventRouter::IsViewInMap(
    const RenderWidgetHostViewBase* view) const {
  for (auto entry : owner_map_) {
    if (entry.second == view)
      return true;
  }
  return false;
}

void RenderWidgetHostInputEventRouter::DispatchTouchscreenGestureEvent(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location) {
  if (gesture_event.GetType() == blink::WebInputEvent::kGesturePinchBegin) {
    in_touchscreen_gesture_pinch_ = true;
    // If the root view wasn't already receiving the gesture stream, then we
    // need to wrap the diverted pinch events in a GestureScrollBegin/End.
    // TODO(wjmaclean,kenrb,tdresser): When scroll latching lands, we can
    // revisit how this code should work.
    // https://crbug.com/526463
    auto* rwhi =
        static_cast<RenderWidgetHostImpl*>(root_view->GetRenderWidgetHost());
    // If the root view is the current gesture target, then we explicitly don't
    // send a GestureScrollBegin, as by the time we see GesturePinchBegin there
    // should have been one.
    if (root_view != touchscreen_gesture_target_.target &&
        !rwhi->is_in_touchscreen_gesture_scroll()) {
      base::Optional<cc::TouchAction> target_allowed_touch_action(
          cc::kTouchActionNone);
      if (touchscreen_gesture_target_.target) {
        target_allowed_touch_action =
            (static_cast<RenderWidgetHostImpl*>(
                 touchscreen_gesture_target_.target->GetRenderWidgetHost()))
                ->input_router()
                ->AllowedTouchAction();
      }
      DCHECK(target_allowed_touch_action.has_value());
      if (target_allowed_touch_action.value() &
          cc::TouchAction::kTouchActionPinchZoom) {
        gesture_pinch_did_send_scroll_begin_ = true;
        // The pinch gesture will be sent to the root view and it may not have a
        // valid touch action yet. In this case, set the touch action to auto.
        rwhi->input_router()->ForceSetTouchActionAuto();
        SendGestureScrollBegin(root_view, gesture_event);
      } else {
        // When target does not allow touch-action: pinch, instead of sending
        // pinch gestures to the root frame, we send all gesture pinch events
        // to the subframe target so the target can look after disposing of
        // them.
        in_touchscreen_gesture_pinch_ = false;
      }
    }
  }

  if (in_touchscreen_gesture_pinch_) {
    root_view->ProcessGestureEvent(gesture_event, latency);
    if (gesture_event.GetType() == blink::WebInputEvent::kGesturePinchEnd) {
      in_touchscreen_gesture_pinch_ = false;
      // If the root view wasn't already receiving the gesture stream, then we
      // need to wrap the diverted pinch events in a GestureScrollBegin/End.
      auto* rwhi =
          static_cast<RenderWidgetHostImpl*>(root_view->GetRenderWidgetHost());
      if (root_view != touchscreen_gesture_target_.target &&
          gesture_pinch_did_send_scroll_begin_ &&
          rwhi->is_in_touchscreen_gesture_scroll()) {
        SendGestureScrollEnd(root_view, gesture_event);
      }
      gesture_pinch_did_send_scroll_begin_ = false;
    }
    return;
  }

  if (gesture_event.GetType() == blink::WebInputEvent::kGestureFlingCancel &&
      last_fling_start_target_) {
    last_fling_start_target_->ProcessGestureEvent(gesture_event, latency);
    return;
  }

  auto gesture_target_it =
      touchscreen_gesture_target_map_.find(gesture_event.unique_touch_event_id);
  bool no_matching_id =
      gesture_target_it == touchscreen_gesture_target_map_.end();

  // We use GestureTapDown to detect the start of a gesture sequence since
  // there is no WebGestureEvent equivalent for ET_GESTURE_BEGIN. Note that
  // this means the GestureFlingCancel that always comes between
  // ET_GESTURE_BEGIN and GestureTapDown is sent to the previous target, in
  // case it is still in a fling.
  bool is_gesture_start =
      gesture_event.GetType() == blink::WebInputEvent::kGestureTapDown;

  if (gesture_event.unique_touch_event_id == 0) {
    // On Android it is possible for touchscreen gesture events to arrive that
    // are not associated with touch events, because non-synthetic events can be
    // created by ContentView. These will use the target found by the
    // RenderWidgetTargeter. These gesture events should always have a
    // unique_touch_event_id of 0.
    touchscreen_gesture_target_.target = target;
    touchscreen_gesture_target_in_map_ = IsViewInMap(target);
    if (!root_view->GetTransformToViewCoordSpace(
            touchscreen_gesture_target_.target,
            &touchscreen_gesture_target_.transform)) {
      touchscreen_gesture_target_.transform.MakeIdentity();
      if (target_location.has_value()) {
        touch_target_.transform.Translate(target_location.value() -
                                          gesture_event.PositionInWidget());
      }
    }
  } else if (no_matching_id && is_gesture_start) {
    // A long-standing Windows issues where occasionally a GestureStart is
    // encountered with no targets in the event queue. We never had a repro for
    // this, but perhaps we should drop these events and wait to see if a bug
    // (with a repro) gets filed, then just fix it.
    //
    // For now, we do a synchronous-only hit test here, which even though
    // incorrect is not likely to have a large effect in the short term.
    UMA_HISTOGRAM_BOOLEAN("Event.FrameEventRouting.NoGestureTarget", true);
    LOG(ERROR) << "Gesture sequence start detected with no target available.";
    // It is still safe to continue; we will recalculate the target.
    gfx::PointF transformed_point;
    gfx::PointF original_point(gesture_event.PositionInWidget());
    gfx::PointF original_point_in_screen(gesture_event.PositionInScreen());
    auto result =
        FindViewAtLocation(root_view, original_point, original_point_in_screen,
                           viz::EventSource::TOUCH, &transformed_point);
    // Re https://crbug.com/796656): Since we are already in an error case,
    // don't worry about the fact we're ignoring |result.should_query_view|, as
    // this is the best we can do until we fix https://crbug.com/595422.
    touchscreen_gesture_target_.target = result.view;
    touchscreen_gesture_target_in_map_ = IsViewInMap(result.view);
    if (!root_view->GetTransformToViewCoordSpace(
            touchscreen_gesture_target_.target,
            &touchscreen_gesture_target_.transform)) {
      touchscreen_gesture_target_.transform.MakeIdentity();
      if (target_location.has_value())
        touch_target_.transform.Translate(transformed_point - original_point);
    }
  } else if (is_gesture_start) {
    touchscreen_gesture_target_ = gesture_target_it->second;
    touchscreen_gesture_target_map_.erase(gesture_target_it);
    touchscreen_gesture_target_in_map_ =
        IsViewInMap(touchscreen_gesture_target_.target);

    // Abort any scroll bubbling in progress to avoid double entry.
    if (touchscreen_gesture_target_.target &&
        touchscreen_gesture_target_.target ==
            bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(
          bubbling_gesture_scroll_target_.target,
          DummyGestureScrollUpdate(gesture_event.TimeStamp(),
                                   bubbling_gesture_scroll_source_device_));
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  // If we set a target and it's not in the map, we won't get notified if the
  // target goes away, so drop the target and the resulting events.
  if (!touchscreen_gesture_target_in_map_)
    touchscreen_gesture_target_.target = nullptr;

  if (!touchscreen_gesture_target_.target) {
    root_view->GestureEventAck(gesture_event,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }

  blink::WebGestureEvent event(gesture_event);
  gfx::PointF transformed_point(gesture_event.PositionInWidget());
  touchscreen_gesture_target_.transform.TransformPoint(&transformed_point);
  event.SetPositionInWidget(transformed_point);

  if (events_being_flushed_) {
    touchscreen_gesture_target_.target->host()
        ->input_router()
        ->ForceSetTouchActionAuto();
  }
  touchscreen_gesture_target_.target->ProcessGestureEvent(event, latency);

  if (gesture_event.GetType() == blink::WebInputEvent::kGestureFlingStart)
    last_fling_start_target_ = touchscreen_gesture_target_.target;
}

void RenderWidgetHostInputEventRouter::RouteTouchscreenGestureEvent(
    RenderWidgetHostViewBase* root_view,
    const blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::kWebGestureDeviceTouchscreen, event->SourceDevice());
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTouchpadGestureEventTarget(
    RenderWidgetHostViewBase* root_view,
    const blink::WebGestureEvent& event) const {
  if (event.GetType() != blink::WebInputEvent::kGesturePinchBegin &&
      event.GetType() != blink::WebInputEvent::kGestureFlingCancel &&
      event.GetType() != blink::WebInputEvent::kGestureDoubleTap) {
    return {nullptr, false, base::nullopt, true, false};
  }

  gfx::PointF transformed_point;
  return FindViewAtLocation(root_view, event.PositionInWidget(),
                            event.PositionInScreen(), viz::EventSource::MOUSE,
                            &transformed_point);
}

void RenderWidgetHostInputEventRouter::RouteTouchpadGestureEvent(
    RenderWidgetHostViewBase* root_view,
    const blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::kWebGestureDeviceTouchpad, event->SourceDevice());
  event_targeter_->FindTargetAndDispatch(root_view, *event, latency);
}

void RenderWidgetHostInputEventRouter::DispatchTouchpadGestureEvent(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebGestureEvent& touchpad_gesture_event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location) {
  // Touchpad gesture flings should be treated as mouse wheels for the purpose
  // of routing.
  if (touchpad_gesture_event.GetType() ==
      blink::WebInputEvent::kGestureFlingStart) {
    if (wheel_target_.target) {
      blink::WebGestureEvent gesture_fling = touchpad_gesture_event;
      gesture_fling.SetPositionInWidget(gesture_fling.PositionInWidget() +
                                        wheel_target_.delta);
      wheel_target_.target->ProcessGestureEvent(gesture_fling, latency);
      last_fling_start_target_ = wheel_target_.target;
    } else {
      root_view->GestureEventAck(touchpad_gesture_event,
                                 INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    }
    return;
  }

  if (touchpad_gesture_event.GetType() ==
      blink::WebInputEvent::kGestureFlingCancel) {
    if (last_fling_start_target_) {
      last_fling_start_target_->ProcessGestureEvent(touchpad_gesture_event,
                                                    latency);
    } else if (target) {
      target->ProcessGestureEvent(touchpad_gesture_event, latency);
    } else {
      root_view->GestureEventAck(touchpad_gesture_event,
                                 INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    }
    return;
  }

  if (target) {
    touchpad_gesture_target_.target = target;
    // TODO(mohsen): Instead of just computing a delta, we should extract the
    // complete transform. We assume it doesn't change for the duration of the
    // touchpad gesture sequence, though this could be wrong; a better approach
    // might be to always transform each point to the
    // |touchpad_gesture_target_.target| for the duration of the sequence.
    DCHECK(target_location.has_value());
    touchpad_gesture_target_.delta =
        target_location.value() - touchpad_gesture_event.PositionInWidget();

    // Abort any scroll bubbling in progress to avoid double entry.
    if (touchpad_gesture_target_.target &&
        touchpad_gesture_target_.target ==
            bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(
          bubbling_gesture_scroll_target_.target,
          DummyGestureScrollUpdate(touchpad_gesture_event.TimeStamp(),
                                   bubbling_gesture_scroll_source_device_));
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  if (!touchpad_gesture_target_.target) {
    root_view->GestureEventAck(touchpad_gesture_event,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }

  blink::WebGestureEvent gesture_event = touchpad_gesture_event;
  // TODO(mohsen): Add tests to check event location.
  gesture_event.SetPositionInWidget(gesture_event.PositionInWidget() +
                                    touchpad_gesture_target_.delta);
  touchpad_gesture_target_.target->ProcessGestureEvent(gesture_event, latency);

  if (touchpad_gesture_event.GetType() ==
          blink::WebInputEvent::kGesturePinchEnd ||
      touchpad_gesture_event.GetType() ==
          blink::WebInputEvent::kGestureDoubleTap) {
    touchpad_gesture_target_.target = nullptr;
  }
}

RenderWidgetHostViewBase*
RenderWidgetHostInputEventRouter::FindViewFromFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) const {
  // TODO(kenrb): There should be a better way to handle hit tests to surfaces
  // that are no longer valid for hit testing. See https://crbug.com/790044.
  auto iter = owner_map_.find(frame_sink_id);
  // If the point hit a Surface whose namspace is no longer in the map, then
  // it likely means the RenderWidgetHostView has been destroyed but its
  // parent frame has not sent a new compositor frame since that happened.
  return iter == owner_map_.end() ? nullptr : iter->second;
}

std::vector<RenderWidgetHostView*>
RenderWidgetHostInputEventRouter::GetRenderWidgetHostViewsForTests() const {
  std::vector<RenderWidgetHostView*> hosts;
  for (auto entry : owner_map_)
    hosts.push_back(entry.second);

  return hosts;
}

RenderWidgetTargeter*
RenderWidgetHostInputEventRouter::GetRenderWidgetTargeterForTests() {
  return event_targeter_.get();
}

RenderWidgetTargetResult
RenderWidgetHostInputEventRouter::FindTargetSynchronously(
    RenderWidgetHostViewBase* root_view,
    const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    return FindMouseEventTarget(
        root_view, static_cast<const blink::WebMouseEvent&>(event));
  }
  if (event.GetType() == blink::WebInputEvent::kMouseWheel) {
    return FindMouseWheelEventTarget(
        root_view, static_cast<const blink::WebMouseWheelEvent&>(event));
  }
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    return FindTouchEventTarget(
        root_view, static_cast<const blink::WebTouchEvent&>(event));
  }
  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    auto gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    if (gesture_event.SourceDevice() ==
        blink::WebGestureDevice::kWebGestureDeviceTouchscreen) {
      return FindTouchscreenGestureEventTarget(root_view, gesture_event);
    }
    if (gesture_event.SourceDevice() ==
        blink::WebGestureDevice::kWebGestureDeviceTouchpad) {
      return FindTouchpadGestureEventTarget(root_view, gesture_event);
    }
  }
  NOTREACHED();
  return RenderWidgetTargetResult();
}

void RenderWidgetHostInputEventRouter::SetEventsBeingFlushed(
    bool events_being_flushed) {
  events_being_flushed_ = events_being_flushed;
}

void RenderWidgetHostInputEventRouter::DispatchEventToTarget(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location) {
  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    DispatchMouseEvent(root_view, target,
                       static_cast<const blink::WebMouseEvent&>(event), latency,
                       target_location);
    return;
  }
  if (event.GetType() == blink::WebInputEvent::kMouseWheel) {
    DispatchMouseWheelEvent(
        root_view, target, static_cast<const blink::WebMouseWheelEvent&>(event),
        latency, target_location);
    return;
  }
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    auto& touch_event = static_cast<const blink::WebTouchEvent&>(event);
    TouchEventWithLatencyInfo touch_with_latency(touch_event, latency);
    if (touch_emulator_ &&
        touch_emulator_->HandleTouchEvent(touch_with_latency.event)) {
      // We cheat a litle bit here, and assume that we know that even if the
      // target is a RenderWidgetHostViewChildFrame, that it would only try to
      // forward the ack to the root view anyways, so we send it there directly.
      root_view->ProcessAckedTouchEvent(touch_with_latency,
                                        INPUT_EVENT_ACK_STATE_CONSUMED);
      return;
    }
    DispatchTouchEvent(root_view, target, touch_event, latency, target_location,
                       false /* not emulated */);
    return;
  }
  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    auto gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    if (gesture_event.SourceDevice() ==
        blink::WebGestureDevice::kWebGestureDeviceTouchscreen) {
      DispatchTouchscreenGestureEvent(root_view, target, gesture_event, latency,
                                      target_location);
      return;
    }
    if (gesture_event.SourceDevice() ==
        blink::WebGestureDevice::kWebGestureDeviceTouchpad) {
      DispatchTouchpadGestureEvent(root_view, target, gesture_event, latency,
                                   target_location);
      return;
    }
  }
  NOTREACHED();
}

TouchEmulator* RenderWidgetHostInputEventRouter::GetTouchEmulator() {
  if (!touch_emulator_)
    touch_emulator_.reset(new TouchEmulator(this, last_device_scale_factor_));

  return touch_emulator_.get();
}

void RenderWidgetHostInputEventRouter::ForwardEmulatedGestureEvent(
    const blink::WebGestureEvent& event) {
  TRACE_EVENT0("input",
               "RenderWidgetHostInputEventRouter::ForwardEmulatedGestureEvent");
  // It's possible that since |last_emulated_event_root_view_| was set by the
  // outbound touch event that the view may have gone away. Before with dispatch
  // the GestureEvent, confirm the view is still available.
  if (!IsViewInMap(last_emulated_event_root_view_))
    return;
  DispatchTouchscreenGestureEvent(last_emulated_event_root_view_, nullptr,
                                  event, ui::LatencyInfo(),
                                  event.PositionInWidget());
}

void RenderWidgetHostInputEventRouter::ForwardEmulatedTouchEvent(
    const blink::WebTouchEvent& event,
    RenderWidgetHostViewBase* target) {
  TRACE_EVENT0("input",
               "RenderWidgetHostInputEventRouter::ForwardEmulatedTouchEvent");
  // Here we re-use the last root view we saw for a mouse move event, or fall
  // back to using |target| as the root_view if we haven't seen a mouse event;
  // this latter case only happens for injected touch events.
  // TODO(wjmaclean): Why doesn't this class just track its root view?
  DCHECK(IsViewInMap(static_cast<RenderWidgetHostViewBase*>(target)));
  last_emulated_event_root_view_ =
      last_mouse_move_root_view_ ? last_mouse_move_root_view_ : target;

  if (event.GetType() == blink::WebInputEvent::kTouchStart)
    active_touches_ += CountChangedTouchPoints(event);
  blink::WebFloatPoint position_in_widget = event.touches[0].PositionInWidget();
  gfx::PointF transformed_point = target->TransformRootPointToViewCoordSpace(
      gfx::PointF(position_in_widget.x, position_in_widget.y));
  DispatchTouchEvent(last_emulated_event_root_view_, target, event,
                     ui::LatencyInfo(), transformed_point, true /* emulated */);
}

void RenderWidgetHostInputEventRouter::SetCursor(const WebCursor& cursor) {
  if (!last_mouse_move_root_view_)
    return;

  last_device_scale_factor_ =
      last_mouse_move_root_view_->current_device_scale_factor();
  if (auto* cursor_manager = last_mouse_move_root_view_->GetCursorManager()) {
    for (auto it : owner_map_)
      cursor_manager->UpdateCursor(it.second, cursor);
  }
}

void RenderWidgetHostInputEventRouter::ShowContextMenuAtPoint(
    const gfx::Point& point,
    const ui::MenuSourceType source_type) {
  // It's possible that since |last_mouse_move_target_| was set by the
  // outbound mouse event that the view may have gone away. Before dispatching
  // the context menu, confirm the view is still available.
  if (!IsViewInMap(last_mouse_move_target_))
    return;

  auto* rwhi = static_cast<RenderWidgetHostImpl*>(
      last_mouse_move_target_->GetRenderWidgetHost());
  DCHECK(rwhi);
  rwhi->ShowContextMenuAtPoint(point, source_type);
}

void RenderWidgetHostInputEventRouter::SetMouseCaptureTarget(
    RenderWidgetHostViewBase* target,
    bool capture) {
  if (touch_emulator_ && touch_emulator_->enabled())
    return;

  if (capture) {
    mouse_capture_target_.target = target;
    return;
  }

  if (mouse_capture_target_.target == target)
    mouse_capture_target_.target = nullptr;
}

RenderWidgetHostImpl*
RenderWidgetHostInputEventRouter::GetMouseCaptureWidgetForTests() const {
  if (mouse_capture_target_.target)
    return mouse_capture_target_.target->host();
  return nullptr;
}

}  // namespace content
