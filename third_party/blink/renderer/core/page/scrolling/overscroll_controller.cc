// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

// Report Overscroll if OverscrollDelta is greater than minimumOverscrollDelta
// to maintain consistency as done in the compositor.
const float kMinimumOverscrollDelta = 0.1;

void AdjustOverscroll(gfx::Vector2dF* unused_delta) {
  DCHECK(unused_delta);
  if (std::abs(unused_delta->x()) < kMinimumOverscrollDelta)
    unused_delta->set_x(0);
  if (std::abs(unused_delta->y()) < kMinimumOverscrollDelta)
    unused_delta->set_y(0);
}

}  // namespace

OverscrollController::OverscrollController(
    const VisualViewport& visual_viewport,
    ChromeClient& chrome_client)
    : visual_viewport_(&visual_viewport), chrome_client_(&chrome_client) {}

void OverscrollController::Trace(Visitor* visitor) const {
  visitor->Trace(visual_viewport_);
  visitor->Trace(chrome_client_);
}

void OverscrollController::ResetAccumulated(bool reset_x, bool reset_y) {
  if (reset_x)
    accumulated_root_overscroll_.set_x(0);
  if (reset_y)
    accumulated_root_overscroll_.set_y(0);
}

void OverscrollController::HandleOverscroll(
    const ScrollResult& scroll_result,
    const gfx::PointF& position_in_root_frame,
    const gfx::Vector2dF& velocity_in_root_frame) {
  DCHECK(visual_viewport_);
  DCHECK(chrome_client_);

  gfx::Vector2dF unused_delta(scroll_result.unused_scroll_delta_x,
                              scroll_result.unused_scroll_delta_y);
  AdjustOverscroll(&unused_delta);

  gfx::Vector2dF delta_in_viewport =
      gfx::ScaleVector2d(unused_delta, visual_viewport_->Scale());
  gfx::Vector2dF velocity_in_viewport =
      gfx::ScaleVector2d(velocity_in_root_frame, visual_viewport_->Scale());
  gfx::PointF position_in_viewport =
      visual_viewport_->RootFrameToViewport(position_in_root_frame);

  ResetAccumulated(scroll_result.did_scroll_x, scroll_result.did_scroll_y);

  if (!delta_in_viewport.IsZero()) {
    accumulated_root_overscroll_ += delta_in_viewport;
    chrome_client_->DidOverscroll(delta_in_viewport,
                                  accumulated_root_overscroll_,
                                  position_in_viewport, velocity_in_viewport);
  }
}

}  // namespace blink
