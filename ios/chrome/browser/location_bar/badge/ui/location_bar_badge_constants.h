// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The relative height of the badge button compared to the location
// bar's height.
extern const CGFloat kBadgeHeightMultiplier;

// The margins before and after the badge's label used as multipliers of
// the badge container's height.
extern const CGFloat kLabelTrailingSpaceMultiplier;
extern const CGFloat kLabelLeadingSpaceMultiplier;

// Infobar badges separator constants.
extern const CGFloat kSeparatorHeightMultiplier;
extern const CGFloat kSeparatorWidthConstant;

// Amount of time animating the badge into the location bar should take.
extern const NSTimeInterval kBadgeDisplayingAnimationTime;

// Amount of time animating the badge container (label) expanding/collapsing.
extern const NSTimeInterval kBadgeContainerExpandAnimationTime;
extern const NSTimeInterval kBadgeContainerCollapseAnimationTime;

// Badge container shadow constants.
extern const float kBadgeContainerShadowOpacity;
extern const float kBadgeContainerShadowRadius;
extern const CGSize kBadgeContainerShadowOffset;

// The point size of the badge's symbol.
extern const CGFloat kBadgeSymbolPointSize;

// The point size of the unified badge symbol.
extern const CGFloat kUnifiedBadgeSymbolPointSize;

// Accessibility identifier for the badge's image view.
extern NSString* const kLocationBarBadgeImageViewIdentifier;

// Accessibility identifier for the badge's label.
extern NSString* const kLocationBarBadgeLabelIdentifier;

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSTANTS_H_
