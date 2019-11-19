// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_TAPPABLE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_TAPPABLE_ITEM_H_

#import "ios/chrome/browser/ui/badges/badge_item.h"

// Holds properties and values needed to configure a BadgeButton that is
// tappable.
@interface BadgeTappableItem : NSObject <BadgeItem>

- (instancetype)initWithBadgeType:(BadgeType)badgeType
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_TAPPABLE_ITEM_H_
