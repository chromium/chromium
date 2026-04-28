// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"

#import <algorithm>
#import <cmath>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Constants used for the container resizing animation.
constexpr CGFloat kRubberBandCoefficient = 0.10;

// Constants used for the side panel aesthetics.
constexpr CGFloat kAssistantSidePanelBorderWidth = 1.0;
constexpr CGFloat kAssistantSidePanelBorderAlpha = 0.46;

// Constants used for the floating card shadow.
const CGSize kAssistantSidePanelShadowOffset = {0, 13};
constexpr CGFloat kAssistantSidePanelShadowRadius = 8.0;
constexpr CGFloat kAssistantSidePanelShadowOpacity = 0.16;

}  // namespace

const CGFloat kMorphingBaseMargin = 10.0;
const CGFloat kMorphingMediumMargin = 5.0;
const CGFloat kMorphingBaseCornerRadius = 36.0;
const CGFloat kMorphingMediumBottomCornerRadius = 44.0;
const CGFloat kMaxBackgroundDimmingAlpha = 0.11;

const CGFloat kAssistantSidePanelMaxWidth = 400.0;
const CGFloat kAssistantSidePanelWidthMultiplier = 1.0 / 3.0;
const CGFloat kAssistantContainerMargin = 10.0;
const CGFloat kAssistantSidePanelCornerRadius = 22.0;

const NSTimeInterval kAssistantSheetSpringDuration = 0.3;

const NSTimeInterval kAssistantSidePanelInsetAnimationDuration = 0.2;
const CGFloat kAssistantSheetSpringDamping = 0.85;

const CGFloat kAssistantSheetMomentumProjectionSeconds = 0.2;

bool IsSidePanelLayout(UITraitCollection* trait_collection) {
  return IsAssistantSidePanelEnabled() &&
         IsRegularXRegularSizeClass(trait_collection);
}

bool IsIPhoneLandscapeLayout(UITraitCollection* trait_collection) {
  return trait_collection.userInterfaceIdiom == UIUserInterfaceIdiomPhone &&
         trait_collection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
}

void ApplyAssistantSidePanelAesthetics(UIView* content_view,
                                       UIView* shadow_view,
                                       bool active) {
  if (!active) {
    content_view.layer.cornerRadius = 0.0;
    content_view.layer.borderWidth = 0.0;
    shadow_view.layer.shadowOpacity = 0.0;
    shadow_view.backgroundColor = [UIColor clearColor];
    return;
  }

  content_view.layer.cornerRadius = kAssistantSidePanelCornerRadius;
  content_view.layer.cornerCurve = kCACornerCurveContinuous;
  content_view.layer.borderWidth = kAssistantSidePanelBorderWidth;
  content_view.layer.borderColor =
      [[UIColor whiteColor]
          colorWithAlphaComponent:kAssistantSidePanelBorderAlpha]
          .CGColor;

  // The view must be opaque to cast a shadow.
  shadow_view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  shadow_view.layer.cornerRadius = kAssistantSidePanelCornerRadius;
  shadow_view.layer.cornerCurve = kCACornerCurveContinuous;
  // TODO(crbug.com/494503434): Update the shadow color to a dynamic color or
  // handle dark mode properly later.
  shadow_view.layer.shadowColor = [UIColor blackColor].CGColor;
  shadow_view.layer.shadowOffset = kAssistantSidePanelShadowOffset;
  shadow_view.layer.shadowRadius = kAssistantSidePanelShadowRadius;
  shadow_view.layer.shadowOpacity = kAssistantSidePanelShadowOpacity;
}

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
  CGFloat top_corner_radius = kMorphingBaseCornerRadius;
  CGFloat bottom_corner_radius = kMorphingBaseCornerRadius;

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
      bottom_corner_radius = 0;
      background_dimming_alpha = kMaxBackgroundDimmingAlpha;
    } else {
      // Lock size strictly to detent layout.
      actual_height = lowest_detent;
      if (lowest_detent == minimized_height) {
        side_margin = kMorphingBaseMargin;
        bottom_margin = kMorphingBaseMargin;
      } else {
        side_margin = kMorphingMediumMargin;
        bottom_margin = kMorphingMediumMargin;
        bottom_corner_radius = kMorphingMediumBottomCornerRadius;
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
    bottom_margin =
        InterpolateValue(kMorphingBaseMargin, kMorphingMediumMargin, progress);
    bottom_corner_radius = InterpolateValue(
        kMorphingBaseCornerRadius, kMorphingMediumBottomCornerRadius, progress);
  }

  // Medium.
  else if (medium_height >= 0 && height == medium_height) {
    side_margin = kMorphingMediumMargin;
    bottom_margin = kMorphingMediumMargin;
    bottom_corner_radius = kMorphingMediumBottomCornerRadius;
  }

  // Medium -> Large.
  else if (medium_height >= 0 && large_height >= 0 && height > medium_height &&
           height < large_height) {
    CGFloat progress = InterpolateProgress(height, medium_height, large_height);
    side_margin = InterpolateValue(kMorphingMediumMargin, 0, progress);
    bottom_margin = InterpolateValue(kMorphingMediumMargin, 0, progress);
    bottom_corner_radius =
        InterpolateValue(kMorphingMediumBottomCornerRadius, 0, progress);
    background_dimming_alpha =
        InterpolateValue(0, kMaxBackgroundDimmingAlpha, progress);
  }

  // Large (and exceeding).
  else if (large_height >= 0 && height >= large_height) {
    side_margin = 0;
    bottom_margin = 0;
    bottom_corner_radius = 0;
    background_dimming_alpha = kMaxBackgroundDimmingAlpha;
  }

  // Minimized -> Large (skipping Medium).
  else if (minimized_height >= 0 && medium_height < 0 && large_height >= 0 &&
           height > minimized_height && height < large_height) {
    CGFloat progress =
        InterpolateProgress(height, minimized_height, large_height);
    side_margin = InterpolateValue(kMorphingBaseMargin, 0, progress);
    bottom_margin = InterpolateValue(kMorphingBaseMargin, 0, progress);
    bottom_corner_radius =
        InterpolateValue(kMorphingBaseCornerRadius, 0, progress);
    background_dimming_alpha =
        InterpolateValue(0, kMaxBackgroundDimmingAlpha, progress);
  }

  // Fallback (e.g. overscrolling past Medium with no Large available).
  else {
    if (medium_height >= 0 && height > medium_height) {
      side_margin = kMorphingMediumMargin;
      bottom_margin = kMorphingMediumMargin;
      bottom_corner_radius = kMorphingMediumBottomCornerRadius;
    } else {
      side_margin = kMorphingBaseMargin;
      bottom_margin = kMorphingBaseMargin;
    }
  }

  return {actual_height,     side_margin,          bottom_margin,
          top_corner_radius, bottom_corner_radius, background_dimming_alpha};
}
