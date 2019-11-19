// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"

namespace blink {

namespace {

// Report Overscroll if OverscrollDelta is greater than minimumOverscrollDelta
// to maintain consistency as done in the compositor.
const float kMinimumOverscrollDelta = 0.1;

void AdjustOverscroll(FloatSize* unused_delta) {
  DCHECK(unused_delta);
  if (std::abs(unused_delta->Width()) < kMinimumOverscrollDelta)
    unused_delta->SetWidth(0);
  if (std::abs(unused_delta->Height()) < kMinimumOverscrollDelta)
    unused_delta->SetHeight(0);
}

}  // namespace

OverscrollController::OverscrollController(
    const VisualViewport& visual_viewport,
    ChromeClient& chrome_client)
    : visual_viewport_(&visual_viewport), chrome_client_(&chrome_client) {}

void OverscrollController::Trace(blink::Visitor* visitor) {
  visitor->Trace(visual_viewport_);
  visitor->Trace(chrome_client_);
}

void OverscrollController::ResetAccumulated(bool reset_x, bool reset_y) {
  if (reset_x)
    accumulated_root_overscroll_.SetWidth(0);
  if (reset_y)
    accumulated_root_overscroll_.SetHeight(0);
}

void OverscrollController::HandleOverscroll(
    const ScrollResult& scroll_result,
    const FloatPoint& position_in_root_frame,
    const FloatSize& velocity_in_root_frame) {
  DCHECK(visual_viewport_);
  DCHECK(chrome_client_);

  FloatSize unused_delta(scroll_result.unused_scroll_delta_x,
                         scroll_result.unused_scroll_delta_y);
  AdjustOverscroll(&unused_delta);

  FloatSize delta_in_viewport =
      unused_delta.ScaledBy(visual_viewport_->Scale());
  FloatSize velocity_in_viewport =
      velocity_in_root_frame.ScaledBy(visual_viewport_->Scale());
  FloatPoint position_in_viewport =
      visual_viewport_->RootFrameToViewport(position_in_root_frame);

  ResetAccumulated(scroll_result.did_scroll_x, scroll_result.did_scroll_y);

  if (delta_in_viewport != FloatSize()) {
    accumulated_root_overscroll_ += delta_in_viewport;
    chrome_client_->DidOverscroll(delta_in_viewport,
                                  accumulated_root_overscroll_,
                                  position_in_viewport, velocity_in_viewport);
  }
}

}  // namespace blink
