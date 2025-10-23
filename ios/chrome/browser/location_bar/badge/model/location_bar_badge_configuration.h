// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration for Location Bar Badge data.
@interface LocationBarBadgeConfiguration : NSObject

// Initialize with an `accessibilityLabel` and the badge `image`.
- (instancetype)initWithAccessibilityLabel:(NSString*)accessibilityLabel
                                badgeImage:(UIImage*)image
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

#pragma mark - Required

// The accessibility label string for the badge button.
@property(nonatomic, readonly) NSString* accessibilityLabel;

// The image of the badge.
@property(nonatomic, readonly) UIImage* badgeImage;

#pragma mark - Optional

// The accessibility hint string for the badge button. Used for additional
// accessibility hints.
@property(nonatomic, strong) NSString* accessibilityHint;

// The string shown with the badge. Primarily used for the text in a chip. If
// this string isn't set, a badge will not transform into a chip.
@property(nonatomic, strong) NSString* badgeText;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_CONFIGURATION_H_
