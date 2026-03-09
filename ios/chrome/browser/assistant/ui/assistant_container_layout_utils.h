// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_LAYOUT_UTILS_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_LAYOUT_UTILS_H_

#import <UIKit/UIKit.h>

// Base horizontal and vertical margins for the morphing container.
extern const CGFloat kMorphingBaseMargin;
// Horizontal margin when the container approaches the Medium detent.
extern const CGFloat kMorphingMediumMargin;
// Default corner radius for standard states.
extern const CGFloat kMorphingBaseCornerRadius;
// Maximum alpha for the background dimming view.
extern const CGFloat kMaxBackgroundDimmingAlpha;

// Encapsulates the dynamically computed styling properties with sub-pixel
// precision.
struct ContainerMorphingConstraints {
  CGFloat actual_height;
  CGFloat side_margin;
  CGFloat bottom_margin;
  CGFloat corner_radius;
  CACornerMask masked_corners;
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
