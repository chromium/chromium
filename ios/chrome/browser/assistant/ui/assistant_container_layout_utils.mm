// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"

#import <algorithm>
#import <cmath>

namespace {

// Constants used for the container resizing animation.
constexpr CGFloat kRubberBandCoefficient = 0.10;

}  // namespace

const CGFloat kMorphingBaseMargin = 10.0;
const CGFloat kMorphingMediumMargin = 5.0;
const CGFloat kMorphingBaseCornerRadius = 36.0;
const CGFloat kMaxBackgroundDimmingAlpha = 0.4;

NSInteger RubberBandDistance(NSInteger offset, NSInteger dimension) {
  CGFloat float_offset = static_cast<CGFloat>(offset);
  CGFloat float_dimension = static_cast<CGFloat>(dimension);
  CGFloat distance =
      (1.0 - (1.0 / ((float_offset * kRubberBandCoefficient / float_dimension) +
                     1.0))) *
      float_dimension;
  return round(distance);
}

CGFloat InterpolateProgress(CGFloat current, CGFloat start, CGFloat end) {
  if (start >= end) {
    return 0.0;
  }
  CGFloat progress = (current - start) / (end - start);
  return std::max(0.0, std::min(progress, 1.0));
}

CGFloat InterpolateValue(CGFloat start_value,
                         CGFloat end_value,
                         CGFloat progress) {
  return start_value + (end_value - start_value) * progress;
}

ContainerMorphingConstraints CalculateMorphingConstraints(
    CGFloat height,
    CGFloat minimized_height,
    CGFloat medium_height,
    CGFloat large_height) {
  // Default bounds.
  CGFloat actual_height = height;
  CGFloat side_margin = 0;
  CGFloat bottom_margin = 0;
  CGFloat corner_radius = kMorphingBaseCornerRadius;

  // By default, round all corners.
  CACornerMask masked_corners = kCALayerMinXMinYCorner |
                                kCALayerMaxXMinYCorner |
                                kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;

  CGFloat background_dimming_alpha = 0.0;

  CGFloat lowest_detent =
      minimized_height >= 0
          ? minimized_height
          : (medium_height >= 0 ? medium_height : large_height);

  // Minimized (and rubber-banding downward).
  if (lowest_detent >= 0 && height <= lowest_detent) {
    if (lowest_detent == large_height) {
      // If the lowest detent is Large, strictly reduces its size rather than
      // translating the margin.
      actual_height = height;
      side_margin = 0;
      bottom_margin = 0;
      masked_corners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
      background_dimming_alpha = kMaxBackgroundDimmingAlpha;
    } else {
      // Lock size strictly to detent layout.
      actual_height = lowest_detent;
      if (lowest_detent == minimized_height) {
        side_margin = kMorphingBaseMargin;
        bottom_margin = kMorphingBaseMargin;
      } else {
        side_margin = kMorphingMediumMargin;
        bottom_margin = kMorphingBaseMargin;
      }
      // Subtract the deficit to physically drag the anchor bounds downwards.
      bottom_margin -= (lowest_detent - height);
    }
  }

  // Minimized -> Medium.
  else if (minimized_height >= 0 && medium_height >= 0 &&
           height > minimized_height && height < medium_height) {
    CGFloat progress =
        InterpolateProgress(height, minimized_height, medium_height);
    side_margin =
        InterpolateValue(kMorphingBaseMargin, kMorphingMediumMargin, progress);
    bottom_margin = kMorphingBaseMargin;
  }

  // Medium.
  else if (medium_height >= 0 && height == medium_height) {
    side_margin = kMorphingMediumMargin;
    bottom_margin = kMorphingBaseMargin;
  }

  // Medium -> Large.
  else if (medium_height >= 0 && large_height >= 0 && height > medium_height &&
           height < large_height) {
    CGFloat progress = InterpolateProgress(height, medium_height, large_height);
    side_margin = InterpolateValue(kMorphingMediumMargin, 0, progress);
    bottom_margin = InterpolateValue(kMorphingBaseMargin, 0, progress);
    background_dimming_alpha =
        InterpolateValue(0.0, kMaxBackgroundDimmingAlpha, progress);
  }

  // Large (and exceeding).
  else if (large_height >= 0 && height >= large_height) {
    side_margin = 0;
    bottom_margin = 0;
    masked_corners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
    background_dimming_alpha = kMaxBackgroundDimmingAlpha;
  }

  // Minimized -> Large (skipping Medium).
  else if (minimized_height >= 0 && medium_height < 0 && large_height >= 0 &&
           height > minimized_height && height < large_height) {
    CGFloat progress =
        InterpolateProgress(height, minimized_height, large_height);
    side_margin = InterpolateValue(kMorphingBaseMargin, 0, progress);
    bottom_margin = InterpolateValue(kMorphingBaseMargin, 0, progress);
    background_dimming_alpha =
        InterpolateValue(0.0, kMaxBackgroundDimmingAlpha, progress);
  }

  // Fallback (e.g. overscrolling past Medium with no Large available).
  else {
    if (medium_height >= 0 && height > medium_height) {
      side_margin = kMorphingMediumMargin;
      bottom_margin = kMorphingBaseMargin;
    } else {
      side_margin = kMorphingBaseMargin;
      bottom_margin = kMorphingBaseMargin;
    }
  }

  return {actual_height, side_margin,    bottom_margin,
          corner_radius, masked_corners, background_dimming_alpha};
}
