// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/browser_controls.h"

#include <algorithm>  // for std::min and std::max

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

BrowserControls::BrowserControls(const Page& page)
    : page_(&page),
      top_shown_ratio_(0),
      bottom_shown_ratio_(0),
      baseline_top_content_offset_(0),
      baseline_bottom_content_offset_(0),
      accumulated_scroll_delta_(0),
      permitted_state_(cc::BrowserControlsState::kBoth) {}

void BrowserControls::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

void BrowserControls::ScrollBegin() {
  ResetBaseline();
}

ScrollOffset BrowserControls::ScrollBy(ScrollOffset pending_delta) {
  // If one or both of the top/bottom controls are showing, the shown ratio
  // needs to be computed.
  if (!TopHeight() && !BottomHeight())
    return pending_delta;

  if ((permitted_state_ == cc::BrowserControlsState::kShown &&
       pending_delta.y() > 0) ||
      (permitted_state_ == cc::BrowserControlsState::kHidden &&
       pending_delta.y() < 0))
    return pending_delta;

  float page_scale = page_->GetVisualViewport().Scale();

  // Update accumulated vertical scroll and apply it to browser controls
  // Compute scroll delta in viewport space by applying page scale
  accumulated_scroll_delta_ += pending_delta.y() * page_scale;

  // We want to base our calculations on top or bottom controls. After consuming
  // the scroll delta, we will calculate a shown ratio for the controls. The
  // top controls have the priority because they need to visually be in sync
  // with the web contents.
  bool base_on_top_controls = TopHeight();

  float old_top_offset = ContentOffset();
  float baseline_content_offset = base_on_top_controls
                                      ? baseline_top_content_offset_
                                      : baseline_bottom_content_offset_;
  float new_content_offset =
      baseline_content_offset - accumulated_scroll_delta_;
  float height = base_on_top_controls ? TopHeight() : BottomHeight();
  // Clamp and use the expected content offset so that we don't return
  // spurious remaining scrolls due to the imprecision of the shownRatio.
  new_content_offset = ClampTo(
      new_content_offset,
      base_on_top_controls ? TopMinHeight() : BottomMinHeight(), height);

  // The top and bottom controls ratios can be calculated independently.
  // However, we want the (normalized) ratios to be equal when scrolling.
  float shown_ratio = new_content_offset / height;
  float min_ratio =
      base_on_top_controls ? TopMinShownRatio() : BottomMinShownRatio();
  float normalized_shown_ratio =
      (ClampTo(shown_ratio, min_ratio, 1.f) - min_ratio) / (1.f - min_ratio);
  // Even though the real shown ratios (shown height / total height) of the top
  // and bottom controls can be different, they share the same
  // relative/normalized ratio to keep them in sync.
  SetShownRatio(
      TopMinShownRatio() + normalized_shown_ratio * (1.f - TopMinShownRatio()),
      BottomMinShownRatio() +
          normalized_shown_ratio * (1.f - BottomMinShownRatio()));

  // Reset baseline when controls are fully visible
  if (top_shown_ratio_ == 1 && bottom_shown_ratio_ == 1)
    ResetBaseline();

  // We negate the difference because scrolling down (positive delta) causes
  // browser controls to hide (negative offset difference).
  ScrollOffset applied_delta(
      0, base_on_top_controls
             ? (old_top_offset - new_content_offset) / page_scale
             : 0);
  return pending_delta - applied_delta;
}

void BrowserControls::ScrollEnd() {
  if ((top_shown_ratio_ == TopMinShownRatio() || top_shown_ratio_ == 1) &&
      (bottom_shown_ratio_ == BottomMinShownRatio() ||
       bottom_shown_ratio_ == 1)) {
    return;
  }

  // Both threshold values are copied from LayerTreeSettings, which are used in
  // BrowserControlsOffsetManager::ScrollEnd.
  constexpr float kTopControlsShowThreshold = 0.5f;
  constexpr float kTopControlsHideThreshold = 0.5f;
  float normalized_top_ratio =
      (top_shown_ratio_ - TopMinShownRatio()) / (1.f - TopMinShownRatio());
  if (normalized_top_ratio >= 1.f - kTopControlsHideThreshold) {
    // If we're showing so much that the hide threshold won't trigger, show.
    UpdateConstraintsAndState(permitted_state_,
                              cc::BrowserControlsState::kShown);
  } else if (normalized_top_ratio < kTopControlsShowThreshold) {
    // If we're showing so little that the show threshold won't trigger, hide.
    UpdateConstraintsAndState(permitted_state_,
                              cc::BrowserControlsState::kHidden);
  } else {
    NOTREACHED();
  }
}

void BrowserControls::ResetBaseline() {
  accumulated_scroll_delta_ = 0;
  baseline_top_content_offset_ = ContentOffset();
  baseline_bottom_content_offset_ = BottomContentOffset();
}

float BrowserControls::UnreportedSizeAdjustment() {
  return (ShrinkViewport() ? TopHeight() : 0) - ContentOffset();
}

float BrowserControls::ContentOffset() {
  return top_shown_ratio_ * TopHeight();
}

// Even though this is called *ContentOffset, the value from here isn't used to
// offset the content because only the top controls should do that. For now, the
// BottomContentOffset is the baseline offset when we don't have top controls.
float BrowserControls::BottomContentOffset() {
  return bottom_shown_ratio_ * BottomHeight();
}

void BrowserControls::SetShownRatio(float top_ratio, float bottom_ratio) {
  // The ratios can be > 1 during height change animations, so we shouldn't
  // clamp the values.
  top_ratio = std::max(0.f, top_ratio);
  bottom_ratio = std::max(0.f, bottom_ratio);

  if (top_shown_ratio_ == top_ratio && bottom_shown_ratio_ == bottom_ratio)
    return;

  top_shown_ratio_ = top_ratio;
  bottom_shown_ratio_ = bottom_ratio;
  page_->GetChromeClient().DidUpdateBrowserControls();
}

void BrowserControls::UpdateConstraintsAndState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current) {
  permitted_state_ = constraints;

  DCHECK(!(constraints == cc::BrowserControlsState::kShown &&
           current == cc::BrowserControlsState::kHidden));
  DCHECK(!(constraints == cc::BrowserControlsState::kHidden &&
           current == cc::BrowserControlsState::kShown));

  if (current == cc::BrowserControlsState::kShown) {
    top_shown_ratio_ = 1;
    bottom_shown_ratio_ = 1;
  } else if (current == cc::BrowserControlsState::kHidden) {
    top_shown_ratio_ = TopMinShownRatio();
    bottom_shown_ratio_ = BottomMinShownRatio();
  }
  page_->GetChromeClient().DidUpdateBrowserControls();
}

void BrowserControls::SetParams(cc::BrowserControlsParams params) {
  if (params_ == params) {
    return;
  }

  params_ = params;
  page_->GetChromeClient().DidUpdateBrowserControls();
}

float BrowserControls::TopMinShownRatio() {
  return TopHeight() ? params_.top_controls_min_height / TopHeight() : 0.f;
}

float BrowserControls::BottomMinShownRatio() {
  return BottomHeight() ? params_.bottom_controls_min_height / BottomHeight()
                        : 0.f;
}

}  // namespace blink
