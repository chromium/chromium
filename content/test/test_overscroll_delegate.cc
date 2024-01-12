// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_overscroll_delegate.h"

namespace content {

TestOverscrollDelegate::TestOverscrollDelegate(const gfx::Size& display_size)
    : display_size_(display_size),
      current_mode_(OVERSCROLL_NONE),
      completed_mode_(OVERSCROLL_NONE),
      delta_x_(0.f),
      delta_y_(0.f) {}

TestOverscrollDelegate::~TestOverscrollDelegate() {}

void TestOverscrollDelegate::Reset() {
  current_mode_ = OVERSCROLL_NONE;
  completed_mode_ = OVERSCROLL_NONE;
  historical_modes_.clear();
  delta_x_ = delta_y_ = 0.f;
}

gfx::Size TestOverscrollDelegate::GetDisplaySize() const {
  return display_size_;
}

bool TestOverscrollDelegate::OnOverscrollUpdate(float delta_x, float delta_y) {
  delta_x_ = delta_x;
  delta_y_ = delta_y;
  return true;
}

void TestOverscrollDelegate::OnOverscrollComplete(
    OverscrollMode overscroll_mode) {
  DCHECK_EQ(current_mode_, overscroll_mode);
  completed_mode_ = overscroll_mode;
  current_mode_ = OVERSCROLL_NONE;
}

void TestOverscrollDelegate::OnOverscrollModeChange(
    OverscrollMode old_mode,
    OverscrollMode new_mode,
    OverscrollSource source,
    cc::OverscrollBehavior behavior) {
  DCHECK_EQ(current_mode_, old_mode);
  current_mode_ = new_mode;
  historical_modes_.push_back(new_mode);
  delta_x_ = delta_y_ = 0.f;
}

std::optional<float> TestOverscrollDelegate::GetMaxOverscrollDelta() const {
  return delta_cap_;
}

}  // namespace content
