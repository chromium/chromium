// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_badge_model.h"

#import "base/check_op.h"
#include "ios/chrome/browser/ui/badges/badge_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBadgeModel () {
  // The badge's type.
  BadgeType _badgeType;
}
@end

@implementation InfobarBadgeModel
// Synthesized from BadgeItem.
@synthesize tappable = _tappable;
// Synthesized from BadgeItem.
@synthesize badgeState = _badgeState;
// Synthesized from BadgeItem.
@synthesize fullScreen = _fullScreen;

- (instancetype)initWithInfobarType:(InfobarType)type {
  self = [super init];
  if (self) {
    _tappable = YES;
    _badgeState = BadgeStateNone;
    _fullScreen = NO;
    _badgeType = BadgeTypeForInfobarType(type);
    DCHECK_NE(BadgeType::kBadgeTypeNone, _badgeType);
  }
  return self;
}

#pragma mark - BadgeViewModel

- (BadgeType)badgeType {
  return _badgeType;
}

@end
