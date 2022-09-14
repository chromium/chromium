// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_OVERFLOW_MENU_UTIL_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_OVERFLOW_MENU_UTIL_H_

#import "ios/chrome/browser/ui/badges/badge_type.h"

typedef void (^ShowModalFunction)(BadgeType);
@class UIMenu;

// Returns new popup menu that will be displayed when users tap the overflow
// badge button.
UIMenu* GetOverflowMenuFromBadgeTypes(NSArray<NSNumber*>* badge_types,
                                      ShowModalFunction show_modal_function);

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_OVERFLOW_MENU_UTIL_H_
