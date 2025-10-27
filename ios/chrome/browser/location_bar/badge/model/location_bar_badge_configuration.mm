// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"

#import "base/check.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"

@implementation LocationBarBadgeConfiguration

#pragma mark - Public

- (instancetype)initWithBadgeType:(LocationBarBadgeType)badgeType
               accessibilityLabel:(NSString*)accessibilityLabel
                       badgeImage:(UIImage*)image {
  self = [super init];
  if (self) {
    CHECK(badgeType != LocationBarBadgeType::kNone);
    CHECK(accessibilityLabel);
    CHECK(image);
    _badgeType = badgeType;
    _accessibilityLabel = accessibilityLabel;
    _badgeImage = image;
  }
  return self;
}

@end
