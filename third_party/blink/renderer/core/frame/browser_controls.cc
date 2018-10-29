// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/browser_controls.h"

#include <algorithm>  // for std::min and std::max

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"

namespace blink {

BrowserControls::BrowserControls(const Page& page)
    : page_(&page),
      top_height_(0),
      bottom_height_(0),
      shown_ratio_(0),
      baseline_content_offset_(0),
      accumulated_scroll_delta_(0),
      shrink_viewport_(false),
      permitted_state_(cc::BrowserControlsState::kBoth) {}

void BrowserControls::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

void BrowserControls::ScrollBegin() {
  ResetBaseline();
}

FloatSize BrowserControls::ScrollBy(FloatSize pending_delta) {
  if ((permitted_state_ == cc::BrowserControlsState::kShown &&
       pending_delta.Height() > 0) ||
      (permitted_state_ == cc::BrowserControlsState::kHidden &&
       pending_delta.Height() < 0))
    return pending_delta;

  if (top_height_ == 0 && bottom_height_ == 0)
    return pending_delta;

  float height = top_height_;
  if (!top_height_)
    height = bottom_height_;

  // If there is no top height, base calculations off bottom height.
  float old_offset = top_height_ ? ContentOffset() : BottomContentOffset();
  float page_scale = page_->GetVisualViewport().Scale();

  // Update accumulated vertical scroll and apply it to browser controls
  // Compute scroll delta in viewport space by applying page scale
  accumulated_scroll_delta_ += pending_delta.Height() * page_scale;

  float new_content_offset =
      baseline_content_offset_ - accumulated_scroll_delta_;

  SetShownRatio(new_content_offset / height);

  // Reset baseline when controls are fully visible
  if (shown_ratio_ == 1)
    ResetBaseline();

  // Clamp and use the expected content offset so that we don't return
  // spurrious remaining scrolls due to the imprecision of the shownRatio.
  new_content_offset = std::min(new_content_offset, height);
  new_content_offset = std::max(new_content_offset, 0.f);

  // We negate the difference because scrolling down (positive delta) causes
  // browser controls to hide (negative offset difference).
  FloatSize applied_delta(
      0, top_height_ ? (old_offset - new_content_offset) / page_scale : 0);
  return pending_delta - applied_delta;
}

void BrowserControls::ResetBaseline() {
  accumulated_scroll_delta_ = 0;
  baseline_content_offset_ =
      top_height_ ? ContentOffset() : BottomContentOffset();
}

float BrowserControls::UnreportedSizeAdjustment() {
  return (shrink_viewport_ ? top_height_ : 0) - ContentOffset();
}

float BrowserControls::ContentOffset() {
  return shown_ratio_ * top_height_;
}

float BrowserControls::BottomContentOffset() {
  return shown_ratio_ * bottom_height_;
}

void BrowserControls::SetShownRatio(float shown_ratio) {
  shown_ratio = std::min(shown_ratio, 1.f);
  shown_ratio = std::max(shown_ratio, 0.f);

  if (shown_ratio_ == shown_ratio)
    return;

  shown_ratio_ = shown_ratio;
  page_->GetChromeClient().DidUpdateBrowserControls();
}

void BrowserControls::UpdateConstraintsAndState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {
  permitted_state_ = constraints;

  DCHECK(!(constraints == cc::BrowserControlsState::kShown &&
           current == cc::BrowserControlsState::kHidden));
  DCHECK(!(constraints == cc::BrowserControlsState::kHidden &&
           current == cc::BrowserControlsState::kShown));
}

void BrowserControls::SetHeight(float top_height,
                                float bottom_height,
                                bool shrink_viewport) {
  if (top_height_ == top_height && shrink_viewport_ == shrink_viewport &&
      bottom_height_ == bottom_height) {
    return;
  }

  top_height_ = top_height;
  bottom_height_ = bottom_height;
  shrink_viewport_ = shrink_viewport;
  page_->GetChromeClient().DidUpdateBrowserControls();
}

}  // namespace blink
