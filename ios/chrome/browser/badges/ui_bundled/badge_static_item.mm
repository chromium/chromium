// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_static_item.h"

#import "ios/chrome/browser/badges/ui_bundled/badge_type.h"

@interface BadgeStaticItem ()

// The BadgeType of this item.
@property(nonatomic, assign) BadgeType badgeType;

@end

@implementation BadgeStaticItem
// Synthesized from protocol.
@synthesize tappable = _tappable;
// Sythesized from protocol.
@synthesize badgeState = _badgeState;
// Synthesized from BadgeItem.
@synthesize fullScreen = _fullScreen;

- (instancetype)initWithBadgeType:(BadgeType)badgeType {
  self = [super init];
  if (self) {
    _badgeType = badgeType;
    _tappable = NO;
    _badgeState = BadgeStateNone;
    _fullScreen = badgeType == kBadgeTypeIncognito;
  }
  return self;
}

#pragma mark - BadgeItem

- (BadgeType)badgeType {
  return _badgeType;
}

@end
