// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_ITEM_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_ITEM_H_

#import <UIKit/UIKit.h>

enum class LocationBarBadgeType;

// Controls container layout and highlight styling of a badge item.
enum class LocationBarBadgeDisplayState {
  // Collapses the badge container to display only the badge icon.
  kCollapsed,
  // Expands the badge container to display the badge icon and text label.
  kExpanded,
  // Collapses the badge container to icon-only with highlighted styling.
  kHighlighted,
};

// Immutable UI value object representing a badge item in the location bar.
@interface LocationBarBadgeItem : NSObject

// The type of badge represented by this item.
@property(nonatomic, assign, readonly) LocationBarBadgeType badgeType;
// Symbol image rendered inside the badge icon view. Cannot be `nil`.
@property(nonatomic, strong, readonly) UIImage* iconImage;
// Optional text shown when the badge container is expanded; `nil` when
// icon-only.
@property(nonatomic, copy, readonly) NSString* labelText;
// Accessibility identifier for the badge button container. Cannot be `nil`.
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;
// Spoken VoiceOver label describing the badge button. Cannot be `nil`.
@property(nonatomic, copy, readonly) NSString* accessibilityLabel;
// Whether the unread indicator dot is visible on the badge.
@property(nonatomic, assign, readonly) BOOL hasUnreadIndicator;
// Visual layout and highlight state for the badge container.
@property(nonatomic, assign, readonly)
    LocationBarBadgeDisplayState displayState;

// Initializes a badge item (`iconImage`, `accessibilityIdentifier`, and
// `accessibilityLabel` cannot be `nil`; `labelText` may be `nil`).
- (instancetype)initWithBadgeType:(LocationBarBadgeType)badgeType
                        iconImage:(UIImage*)iconImage
                        labelText:(NSString*)labelText
          accessibilityIdentifier:(NSString*)accessibilityIdentifier
               accessibilityLabel:(NSString*)accessibilityLabel
               hasUnreadIndicator:(BOOL)hasUnreadIndicator
                     displayState:(LocationBarBadgeDisplayState)displayState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_ITEM_H_
