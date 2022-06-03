// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_POPUP_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_POPUP_MENU_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol BadgeItem;

// Coordinator for the badge overflow popup menu.
@interface BadgePopupMenuCoordinator : ChromeCoordinator

// Updates the popup menu with |badgesItems|.
- (void)setBadgeItemsToShow:(NSArray<id<BadgeItem>>*)badgeItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_POPUP_MENU_COORDINATOR_H_
