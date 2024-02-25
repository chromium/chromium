// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"

@protocol FamilyPromoCoordinatorDelegate;

// Presents the family promo view for users that are not a part of any Google
// Family.
@interface FamilyPromoCoordinator : ChromeCoordinator

- (instancetype)initWithFamilyPromoType:(FamilyPromoType)familyPromoType
                     baseViewController:(UIViewController*)viewController
                                browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<FamilyPromoCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_COORDINATOR_H_
