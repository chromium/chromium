// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button_util.h"

#import <algorithm>

#import "base/check.h"

namespace {

// The maximum amount of horizontal paddding.
const size_t kMaxHorizontalPadding = 14;
// The minimum amount of horizontal padding for the Track Button.
const size_t kMinHorizontalPadding = 6;
// The Track Button's ideal width is 17.5% of the TableViewCell's width.
const double kTargetButtonWidth = .175;
// The Track Button's ideal width is 21% of the TableViewCell's width.
const double kMaxButtonWidth = .21;

}  // namespace

namespace price_notifications {

size_t CalculateTrackButtonHorizontalPadding(double parent_cell_width,
                                             double button_text_width) {
  size_t target_width = parent_cell_width * kTargetButtonWidth;
  size_t delta = (target_width - button_text_width) / 2;
  return std::clamp(delta, kMinHorizontalPadding, kMaxHorizontalPadding);
}

WidthConstraintValues CalculateTrackButtonWidthConstraints(
    double parent_cell_width,
    double button_text_width,
    size_t horizontal_padding) {
  size_t width = button_text_width + horizontal_padding * 2;
  size_t max_width = parent_cell_width * kMaxButtonWidth;
  size_t target_width = std::min(width, max_width);
  return {max_width, target_width};
}

}  // namespace price_notifications
