// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/touch_passthrough_manager.h"

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
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"

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
  if (is_passthrough_) {
    // This shouldn't happen, but if we do ever get a touch start when
    // we thought we were already passing through, we should reset our state.
    NOTREACHED();
    OnTouchEnd();
  }

  // Keep track of the current state.
  is_touch_down_ = true;

  // Perform a hit test to determine if this event is within a touch
  // passthrough region. Use an incrementing ID for each hit test so
  // that any callbacks that are received late can be ignored.
  hit_test_id_++;
  SendHitTest(point_in_frame_pixels,
              base::BindOnce(&TouchPassthroughManager::OnHitTestResult,
                             weak_ptr_factory_.GetWeakPtr(), hit_test_id_,
                             base::TimeTicks::Now(), point_in_frame_pixels));
}

void TouchPassthroughManager::OnTouchMove(
    const gfx::Point& point_in_frame_pixels) {
  DCHECK(is_touch_down_);

  if (is_passthrough_)
    SimulateMove(point_in_frame_pixels, base::TimeTicks::Now());
}

void TouchPassthroughManager::OnTouchEnd() {
  DCHECK(is_touch_down_);
  is_touch_down_ = false;

  if (is_passthrough_) {
    SimulateRelease(base::TimeTicks::Now());
    is_passthrough_ = false;
  }
}

void TouchPassthroughManager::OnHitTestResult(
    int hit_test_id,
    base::TimeTicks event_time,
    gfx::Point location,
    ui::AXPlatformTreeManager* hit_manager,
    ui::AXNodeID hit_node_id) {
  // Ignore the result if it arrived too late to do something about it.
  if (hit_test_id != hit_test_id_)
    return;

  // If it's not a touch passthrough node, we're done.
  if (!IsTouchPassthroughNode(hit_manager, hit_node_id))
    return;

  // If touch is no longer down, we need to just esnd a tap.
  if (!is_touch_down_) {
    SimulatePress(location, event_time);
    SimulateRelease(event_time);
    return;
  }

  // Otherwise, send a press and set a flag to keep passing through
  // events.
  SimulatePress(location, event_time);
  is_passthrough_ = true;
}

bool TouchPassthroughManager::IsTouchPassthroughNode(
    ui::AXPlatformTreeManager* hit_manager,
    ui::AXNodeID hit_node_id) {
  // Given the result of a hit test, walk up the tree to determine if
  // this node or an ancestor has the passthrough bit set.
  if (!hit_manager)
    return false;

  BrowserAccessibility* hit_node =
      static_cast<BrowserAccessibilityManager*>(hit_manager)
          ->GetFromID(hit_node_id);
  if (!hit_node)
    return false;

  while (hit_node) {
    if (hit_node->GetData().GetBoolAttribute(
            ax::mojom::BoolAttribute::kTouchPassthrough))
      return true;
    hit_node = hit_node->PlatformGetParent();
  }

  return false;
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
    base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                            int hit_node_id)> callback) {
  rfh_->AccessibilityHitTest(point_in_frame_pixels, ax::mojom::Event::kNone, 0,
                             std::move(callback));
}

void TouchPassthroughManager::CancelTouchesAndDestroyTouchDriver() {
  if (!touch_driver_)
    return;

  if (is_passthrough_) {
    touch_driver_->Cancel(kSyntheticTouchIndex);
    touch_driver_->DispatchEvent(gesture_target_, base::TimeTicks::Now());
  }

  is_touch_down_ = false;
  touch_driver_.reset();
  // `gesture_target_` is a raw pointer on a `unique_ptr` owned by
  // `gesture_controller_`. Hence we need to clear this raw_ptr first before
  // releasing `gesture_controller_`.
  gesture_target_ = nullptr;
  gesture_controller_.reset();
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
