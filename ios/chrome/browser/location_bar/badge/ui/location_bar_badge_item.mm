// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_item.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"

@implementation LocationBarBadgeItem

- (instancetype)initWithBadgeType:(LocationBarBadgeType)badgeType
                        iconImage:(UIImage*)iconImage
                        labelText:(NSString*)labelText
          accessibilityIdentifier:(NSString*)accessibilityIdentifier
               accessibilityLabel:(NSString*)accessibilityLabel
               hasUnreadIndicator:(BOOL)hasUnreadIndicator
                     displayState:(LocationBarBadgeDisplayState)displayState {
  CHECK(iconImage);
  CHECK(accessibilityIdentifier);
  CHECK(accessibilityLabel);
  self = [super init];
  if (self) {
    _badgeType = badgeType;
    _iconImage = iconImage;
    _labelText = [labelText copy];
    _accessibilityIdentifier = [accessibilityIdentifier copy];
    _accessibilityLabel = [accessibilityLabel copy];
    _hasUnreadIndicator = hasUnreadIndicator;
    _displayState = displayState;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  LocationBarBadgeItem* other =
      base::apple::ObjCCast<LocationBarBadgeItem>(object);
  // Only `labelText` may be `nil`; other object properties are non-`nil`.
  return other && self.badgeType == other.badgeType &&
         [self.iconImage isEqual:other.iconImage] &&
         (self.labelText == other.labelText ||
          [self.labelText isEqualToString:other.labelText]) &&
         [self.accessibilityIdentifier
             isEqualToString:other.accessibilityIdentifier] &&
         [self.accessibilityLabel isEqualToString:other.accessibilityLabel] &&
         self.hasUnreadIndicator == other.hasUnreadIndicator &&
         self.displayState == other.displayState;
}

- (NSUInteger)hash {
  return self.iconImage.hash ^ self.labelText.hash ^
         self.accessibilityIdentifier.hash ^ self.accessibilityLabel.hash ^
         ((static_cast<NSUInteger>(self.badgeType) << 3) |
          (static_cast<NSUInteger>(self.displayState) << 1) |
          (self.hasUnreadIndicator ? 1U : 0U));
}

@end
