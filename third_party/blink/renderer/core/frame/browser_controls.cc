// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/browser_controls.h"

#include <algorithm>  // for std::min and std::max

#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"

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
