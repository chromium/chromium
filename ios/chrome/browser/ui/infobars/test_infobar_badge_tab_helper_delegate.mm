// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/test_infobar_badge_tab_helper_delegate.h"

#import <map>

#import "ios/chrome/browser/badges/ui_bundled/badge_tappable_item.h"

@interface TestInfobarTabHelperDelegate () {
  std::map<InfobarType, id<BadgeItem>> _badgeItems;
}

@end

@implementation TestInfobarTabHelperDelegate

- (void)dealloc {
  self.badgeTabHelper = nil;
}

#pragma mark - Public

- (id<BadgeItem>)itemForInfobarType:(InfobarType)type {
  return _badgeItems[type];
}

#pragma mark - InfobarBadgeTabHelperDelegate

- (BOOL)badgeSupportedForInfobarType:(InfobarType)infobarType {
  return infobarType == InfobarType::kInfobarTypePasswordSave;
}

- (void)updateBadgesShownForWebState:(web::WebState*)webState {
  _badgeItems.clear();
  std::map<InfobarType, BadgeState> badgeStatesForInfobarType =
      self.badgeTabHelper->GetInfobarBadgeStates();
  for (auto& infobarTypeBadgeStatePair : badgeStatesForInfobarType) {
    BadgeTappableItem* item =
        [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypePasswordSave];
    item.badgeState = infobarTypeBadgeStatePair.second;
    _badgeItems[infobarTypeBadgeStatePair.first] = item;
  }
}

@end
