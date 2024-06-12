// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/badges/ui_bundled/badge_type.h"

@class BadgeButtonActionHandler;
@class BadgeButton;
@protocol BadgeDelegate;
class InfoBarIOS;

// BadgeButtonFactory Factory creates BadgButton objects with certain
// styles and configurations, depending on its type.
@interface BadgeButtonFactory : NSObject

// Action handler delegate for the buttons.
@property(nonatomic, weak) id<BadgeDelegate> delegate;

// Returns a properly configured BadgeButton associated with `badgeType` with
// the use of `infoBar`.
- (BadgeButton*)badgeButtonForBadgeType:(BadgeType)badgeType
                           usingInfoBar:(InfoBarIOS*)infoBar;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_BUTTON_FACTORY_H_
