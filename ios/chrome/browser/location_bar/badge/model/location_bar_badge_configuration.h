// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

enum class LocationBarBadgeType;

// Configuration for Location Bar Badge data.
@interface LocationBarBadgeConfiguration : NSObject

- (instancetype)initWithBadgeType:(LocationBarBadgeType)badgeType
               accessibilityLabel:(NSString*)accessibilityLabel
                       badgeImage:(UIImage*)image NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The badge type. Associated with the feature that the configuration is being
// sent from.
@property(nonatomic, assign) LocationBarBadgeType badgeType;

// The accessibility label string for the location bar badge button.
@property(nonatomic, strong) NSString* accessibilityLabel;

// The image of the badge.
@property(nonatomic, strong) UIImage* badgeImage;

// The accessibility hint string for the badge button.
@property(nonatomic, strong) NSString* accessibilityHint;

// The string shown with the badge. Primarily used for the text in a chip. If
// this string is not set, the badge will not expand into a chip.
@property(nonatomic, strong) NSString* badgeText;

// Whether to hide badge after chip collapse. Default is NO which allows the
// badge to persist after the chip collapses.
@property(nonatomic, assign) BOOL shouldHideBadgeAfterChipCollapse;

// Whether is badge is currently being used. Default is NO which implies the
// badge being visible but not active.
@property(nonatomic, assign, getter=isActive) BOOL active;

#pragma mark - Helper methods

// Whether a badge configuration is related to a contextual panel entrypoint
// badge.
- (BOOL)isContextualPanelEntrypointBadge;

// Whether a badge configuration is related to a badge from BadgeFactory.
- (BOOL)fromBadgeFactory;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_CONFIGURATION_H_
