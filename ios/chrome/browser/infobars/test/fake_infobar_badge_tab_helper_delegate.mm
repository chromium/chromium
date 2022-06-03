// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/test/fake_infobar_badge_tab_helper_delegate.h"

#include <map>

#import "ios/chrome/browser/ui/badges/badge_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeInfobarTabHelperDelegate () {
  std::map<BadgeType, id<BadgeItem>> _badgeItems;
}
@end

@implementation FakeInfobarTabHelperDelegate

#pragma mark - Public

- (id<BadgeItem>)itemForBadgeType:(BadgeType)type {
  return _badgeItems[type];
}

#pragma mark - InfobarBadgeTabHelperDelegate

- (void)addInfobarBadge:(id<BadgeItem>)badgeItem
            forWebState:(web::WebState*)webState {
  _badgeItems[badgeItem.badgeType] = badgeItem;
}

- (void)removeInfobarBadge:(id<BadgeItem>)badgeItem
               forWebState:(web::WebState*)webState {
  _badgeItems[badgeItem.badgeType] = nil;
}

- (void)updateInfobarBadge:(id<BadgeItem>)badgeItem
               forWebState:(web::WebState*)webState {
}

@end
