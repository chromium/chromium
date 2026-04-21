// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_LAYOUT_UTILS_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_LAYOUT_UTILS_H_

#import <UIKit/UIKit.h>

// Morphing sheet layout constants for the sheet presentation.

// Base margins for floating states.
extern const CGFloat kMorphingBaseMargin;
// Margin for Medium detent.
extern const CGFloat kMorphingMediumMargin;
// Default top corner radius.
extern const CGFloat kMorphingBaseCornerRadius;
// Bottom corner radius for Medium state.
extern const CGFloat kMorphingMediumBottomCornerRadius;
// Max opacity of dimming view.
extern const CGFloat kMaxBackgroundDimmingAlpha;

// Assistant Side Panel constants (iPad).

// Maximum width of the panel.
extern const CGFloat kAssistantSidePanelMaxWidth;
// Width as fraction of screen width.
extern const CGFloat kAssistantSidePanelWidthMultiplier;
// Margin from screen edges.
extern const CGFloat kAssistantContainerMargin;
// Corner radius for all 4 corners.
extern const CGFloat kAssistantSidePanelCornerRadius;

// Animation constants for sheet and side panel.

// Spring duration for sheet.
extern const NSTimeInterval kAssistantSheetSpringDuration;
// Duration for inset changes.
extern const NSTimeInterval kAssistantSidePanelInsetAnimationDuration;
// Spring damping ratio.
extern const CGFloat kAssistantSheetSpringDamping;
// Momentum projection time.
extern const CGFloat kAssistantSheetMomentumProjectionSeconds;

// Returns true if the layout traits dictate presenting the Assistant as a side
// panel. This requires the iPad idiom AND the regular horizontal size class.
bool IsSidePanelLayout(UITraitCollection* trait_collection);

// Returns true if the layout is currently iPhone landscape.
bool IsIPhoneLandscapeLayout(UITraitCollection* trait_collection);

// Applies side panel aesthetics (rounded corners on content, shadow on shadow
// view).
void ApplyAssistantSidePanelAesthetics(UIView* content_view,
                                       UIView* shadow_view,
                                       bool active);

// Encapsulates the dynamically computed styling properties with sub-pixel
// precision.
struct ContainerMorphingConstraints {
  CGFloat actual_height;
  CGFloat side_margin;
  CGFloat bottom_margin;
  CGFloat top_corner_radius;
  CGFloat bottom_corner_radius;
  CGFloat background_dimming_alpha;
};

// Computes the rubber banding distance for downward gestures.
NSInteger RubberBandDistance(NSInteger offset, NSInteger dimension);

// Calculates a fractional progress value for a given distance.
CGFloat InterpolateProgress(CGFloat current, CGFloat start, CGFloat end);

// Maps a numeric value linearly along a progress fraction.
// Returns CGFloat to ensure smooth animations on high-density displays.
CGFloat InterpolateValue(CGFloat start_value,
                         CGFloat end_value,
                         CGFloat progress);

// Calculates dynamic layout dimensions for a given height and detent bounds.
ContainerMorphingConstraints CalculateMorphingConstraints(
    CGFloat height,
    CGFloat minimized_height,
    CGFloat medium_height,
    CGFloat large_height);

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_LAYOUT_UTILS_H_
