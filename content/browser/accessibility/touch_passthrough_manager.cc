// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/touch_passthrough_manager.h"

#include "base/containers/contains.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_touch_driver.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {

// Touch events require an index, so that you can distinguish between multiple
// fingers in multi-finger gestures.  Touch passthrough only simulates a single
// finger, so we always use the same touch index; it doesn't matter what
// number we pass here as long as it's always the same index.
constexpr int kSyntheticTouchIndex = 0;

TouchPassthroughManager::TouchPassthroughManager(RenderFrameHostImpl* rfh)
    : rfh_(rfh) {}

TouchPassthroughManager::~TouchPassthroughManager() {
  CancelTouchesAndDestroyTouchDriver();
}

void TouchPassthroughManager::OnTouchStart(
    const gfx::Point& point_in_frame_pixels) {
  // Enqueue the touch start event, to be processed based on the results
  // of the hit test.
  HitTestAndEnqueueEventOfType(EventType::kPress, point_in_frame_pixels);
}

void TouchPassthroughManager::OnTouchMove(
    const gfx::Point& point_in_frame_pixels) {
  // Enqueue the touch move event, to be processed based on the results
  // of the hit test.
  HitTestAndEnqueueEventOfType(EventType::kMove, point_in_frame_pixels);
}

void TouchPassthroughManager::OnTouchEnd() {
  // A touch end doesn't require a hit test, so enqueue it and then
  // try to process pending events now. It may not get processed right
  // away if the previous events were waiting on a hit test.
  EnqueueEventOfType(EventType::kRelease, /* pending = */ false, {});
  ProcessPendingEvents();
}

void TouchPassthroughManager::HitTestAndEnqueueEventOfType(
    TouchPassthroughManager::EventType type,
    const gfx::Point& point_in_frame_pixels) {
  // Enqueue one event struct. The rest of the details will get filled
  // in after the hit test by |OnHitTestResult|.
  int event_id =
      EnqueueEventOfType(type, /* pending = */ true, point_in_frame_pixels);
  SendHitTest(point_in_frame_pixels,
              base::BindOnce(&TouchPassthroughManager::OnHitTestResult,
                             weak_ptr_factory_.GetWeakPtr(), event_id));
}

int TouchPassthroughManager::EnqueueEventOfType(
    EventType type,
    bool pending,
    const gfx::Point& point_in_frame_pixels) {
  int event_id = next_event_id_;
  next_event_id_++;
  auto event = std::make_unique<TouchPassthroughEvent>();
  event->pending = pending;
  event->type = type;
  event->time = base::TimeTicks::Now();
  event->location = point_in_frame_pixels;
  id_to_event_[event_id] = std::move(event);
  return event_id;
}

void TouchPassthroughManager::OnHitTestResult(
    int event_id,
    BrowserAccessibilityManager* hit_manager,
    int hit_node_id) {
  // Exit if this event is no longer in our queue.
  // TODO(dmazzoni): add timeouts so that if a hit test result doesn't
  // come back quickly we don't wait on it forever; that will trigger a
  // real scenario where we could get a callback but the event isn't in
  // the queue.
  auto iter = id_to_event_.find(event_id);
  if (iter == id_to_event_.end())
    return;

  TouchPassthroughEvent* event = iter->second.get();
  DCHECK(event);

  // Store the tree ID and node ID if the hit test result landed on a
  // passthrough node.
  BrowserAccessibility* hit_node =
      GetTouchPassthroughNode(hit_manager, hit_node_id);
  if (hit_node) {
    event->tree_id = hit_manager->GetTreeID();
    event->node_id = hit_node->GetId();
  }

  // This node is no longer pending. Try to process pending events.
  event->pending = false;
  ProcessPendingEvents();
}

void TouchPassthroughManager::ProcessPendingEvents() {
  // Start with the tail of the queue - the last one in that hasn't been
  // processed yet - and keep processing events that are ready.
  while (current_event_id_ < next_event_id_) {
    // Peek at the front of the queue.
    auto iter = id_to_event_.find(current_event_id_);
    DCHECK(iter != id_to_event_.end());

    // If it's not ready, return. |ProcessPendingEvents| will be called
    // again when a hit test comes back.
    if (iter->second->pending)
      return;

    // Pop it off the queue and process it.
    std::unique_ptr<TouchPassthroughEvent> event = std::move(iter->second);
    id_to_event_.erase(iter);
    current_event_id_++;
    ProcessPendingEvent(std::move(event));
  }
}

void TouchPassthroughManager::ProcessPendingEvent(
    std::unique_ptr<TouchPassthroughEvent> event) {
  // This function is called once for each event in the queue, in the
  // order the events were originally generated, but after the hit test
  // result is available. That allows us to process the nodes based on
  // some simple logic to determine if events need to be passed through
  // or not.

  // If it's a release event and we're currently passing through a touch
  // down, pass through a touch release.
  if (is_touch_down_ && (event->type == EventType::kRelease)) {
    SimulateRelease(event->time);
    is_touch_down_ = false;
    return;
  }

  // If the node ID is different, the user moved from one passthrough
  // region to a different one. Send a cancel touch event for the
  // previous passthrough region, but then keep going.
  if (is_touch_down_ && (event->tree_id != current_tree_id_ ||
                         event->node_id != current_node_id_)) {
    SimulateCancel(event->time);
    is_touch_down_ = false;
  }

  // If the current touch is no longer within a passthrough region, we're
  // done.
  if (event->tree_id == ui::AXTreeIDUnknown())
    return;

  // We're in a passthrough region. Send either a touch down or touch move.
  current_tree_id_ = event->tree_id;
  current_node_id_ = event->node_id;
  if (!is_touch_down_) {
    is_touch_down_ = true;
    SimulatePress(event->location, event->time);
  } else {
    SimulateMove(event->location, event->time);
  }
}

BrowserAccessibility* TouchPassthroughManager::GetTouchPassthroughNode(
    BrowserAccessibilityManager* hit_manager,
    int hit_node_id) {
  // Given the result of a hit test, walk up the tree to determine if
  // this node or an ancestor has the passthrough bit set.
  if (!hit_manager)
    return nullptr;

  BrowserAccessibility* hit_node = hit_manager->GetFromID(hit_node_id);
  if (!hit_node)
    return nullptr;

  while (hit_node) {
    if (hit_node->GetData().GetBoolAttribute(
            ax::mojom::BoolAttribute::kTouchPassthrough))
      return hit_node;
    hit_node = hit_node->PlatformGetParent();
  }

  return nullptr;
}

void TouchPassthroughManager::CreateTouchDriverIfNeeded() {
  RenderWidgetHostImpl* rwh = rfh_->GetRenderWidgetHost();
  std::unique_ptr<SyntheticGestureTarget> gesture_target_unique_ptr =
      rwh->GetView()->CreateSyntheticGestureTarget();
  gesture_target_ = gesture_target_unique_ptr.get();
  gesture_controller_ = std::make_unique<SyntheticGestureController>(
      rwh, std::move(gesture_target_unique_ptr));
  touch_driver_ = std::make_unique<SyntheticTouchDriver>();
}

void TouchPassthroughManager::SendHitTest(
    const gfx::Point& point_in_frame_pixels,
    base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                            int hit_node_id)> callback) {
  rfh_->AccessibilityHitTest(point_in_frame_pixels, ax::mojom::Event::kNone, 0,
                             std::move(callback));
}

void TouchPassthroughManager::CancelTouchesAndDestroyTouchDriver() {
  if (!touch_driver_)
    return;

  if (is_touch_down_) {
    touch_driver_->Cancel(kSyntheticTouchIndex);
    touch_driver_->DispatchEvent(gesture_target_, base::TimeTicks::Now());
  }

  is_touch_down_ = false;
  touch_driver_.reset();
  gesture_controller_.reset();
  gesture_target_ = nullptr;
}

void TouchPassthroughManager::SimulatePress(const gfx::Point& point,
                                            const base::TimeTicks& time) {
  CreateTouchDriverIfNeeded();

  gfx::Point css_point = ToCSSPoint(point);
  touch_driver_->Press(css_point.x(), css_point.y(), kSyntheticTouchIndex);
  touch_driver_->DispatchEvent(gesture_target_, time);
}

void TouchPassthroughManager::SimulateMove(const gfx::Point& point,
                                           const base::TimeTicks& time) {
  DCHECK(touch_driver_);
  gfx::Point css_point = ToCSSPoint(point);
  touch_driver_->Move(css_point.x(), css_point.y(), kSyntheticTouchIndex);
  touch_driver_->DispatchEvent(gesture_target_, time);
}

void TouchPassthroughManager::SimulateCancel(const base::TimeTicks& time) {
  DCHECK(touch_driver_);
  touch_driver_->Cancel(kSyntheticTouchIndex);
  touch_driver_->DispatchEvent(gesture_target_, time);
}

void TouchPassthroughManager::SimulateRelease(const base::TimeTicks& time) {
  DCHECK(touch_driver_);
  touch_driver_->Release(kSyntheticTouchIndex);
  touch_driver_->DispatchEvent(gesture_target_, time);
}

gfx::Point TouchPassthroughManager::ToCSSPoint(
    gfx::Point point_in_frame_pixels) {
  gfx::Point result = point_in_frame_pixels;

  // Scale by the device scale factor.
  float dsf = rfh_->AccessibilityGetDeviceScaleFactor();
  if (IsUseZoomForDSFEnabled())
    result = ScaleToRoundedPoint(result, 1.0 / dsf);

  // Offset by the top controls height.
  RenderWidgetHostImpl* rwhi = rfh_->GetRenderWidgetHost();
  RenderFrameMetadataProviderImpl* render_frame_metadata_provider =
      rwhi->render_frame_metadata_provider();
  const cc::RenderFrameMetadata render_frame_metadata =
      render_frame_metadata_provider->LastRenderFrameMetadata();
  float offset = render_frame_metadata.top_controls_height / dsf;
  result.Offset(0, offset);

  return result;
}

}  // namespace content
