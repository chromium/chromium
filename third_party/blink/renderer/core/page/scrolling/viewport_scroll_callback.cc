// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/viewport_scroll_callback.h"

#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

ViewportScrollCallback::ViewportScrollCallback(
    BrowserControls* browser_controls,
    OverscrollController* overscroll_controller,
    RootFrameViewport& root_frame_viewport)
    : browser_controls_(browser_controls),
      overscroll_controller_(overscroll_controller),
      root_frame_viewport_(&root_frame_viewport) {}

ViewportScrollCallback::~ViewportScrollCallback() = default;

void ViewportScrollCallback::Trace(Visitor* visitor) const {
  visitor->Trace(browser_controls_);
  visitor->Trace(overscroll_controller_);
  visitor->Trace(root_frame_viewport_);
  ScrollStateCallback::Trace(visitor);
}

bool ViewportScrollCallback::ShouldScrollBrowserControls(
    const ScrollOffset& delta,
    ui::ScrollGranularity granularity) const {
  if (granularity != ui::ScrollGranularity::kScrollByPixel &&
      granularity != ui::ScrollGranularity::kScrollByPrecisePixel)
    return false;

  if (!root_frame_viewport_)
    return false;

  ScrollOffset max_scroll = root_frame_viewport_->MaximumScrollOffset();
  ScrollOffset scroll_offset = root_frame_viewport_->GetScrollOffset();

  // Always give the delta to the browser controls if the scroll is in
  // the direction to show the browser controls. If it's in the
  // direction to hide the browser controls, only give the delta to the
  // browser controls when the frame can scroll.
  return delta.y() < 0 || scroll_offset.y() < max_scroll.y();
}

bool ViewportScrollCallback::ScrollBrowserControls(ScrollState& state) {
  // Scroll browser controls.
  if (browser_controls_) {
    if (state.isBeginning())
      browser_controls_->ScrollBegin();

    ScrollOffset delta(state.deltaX(), state.deltaY());
    ui::ScrollGranularity granularity = state.delta_granularity();
    if (ShouldScrollBrowserControls(delta, granularity)) {
      ScrollOffset remaining_delta = browser_controls_->ScrollBy(delta);
      ScrollOffset consumed = delta - remaining_delta;
      state.ConsumeDeltaNative(consumed.x(), consumed.y());
      return !consumed.IsZero();
    }
  }

  return false;
}

void ViewportScrollCallback::Invoke(ScrollState* state) {
  DCHECK(state);
  if (!root_frame_viewport_)
    return;

  bool browser_controls_did_scroll = ScrollBrowserControls(*state);

  ScrollResult result = PerformNativeScroll(*state);

  // We consider browser controls movement to be scrolling.
  result.did_scroll_y |= browser_controls_did_scroll;

  // Handle Overscroll.
  if (overscroll_controller_) {
    gfx::PointF position(state->positionX(), state->positionY());
    gfx::Vector2dF velocity(state->velocityX(), state->velocityY());
    overscroll_controller_->HandleOverscroll(result, position, velocity);
  }
}

void ViewportScrollCallback::SetScroller(ScrollableArea* scroller) {
  DCHECK(scroller);
  root_frame_viewport_->SetLayoutViewport(*scroller);
}

ScrollResult ViewportScrollCallback::PerformNativeScroll(ScrollState& state) {
  DCHECK(root_frame_viewport_);

  ScrollOffset delta(state.deltaX(), state.deltaY());
  ui::ScrollGranularity granularity = state.delta_granularity();

  ScrollResult result = root_frame_viewport_->UserScroll(
      granularity, delta, ScrollableArea::ScrollCallback());

  // The viewport consumes everything.
  // TODO(bokan): This isn't actually consuming everything but doing so breaks
  // the main thread pull-to-refresh action. crbug.com/607210.
  state.ConsumeDeltaNative(delta.x() - result.unused_scroll_delta_x,
                           delta.y() - result.unused_scroll_delta_y);

  return result;
}

}  // namespace blink
