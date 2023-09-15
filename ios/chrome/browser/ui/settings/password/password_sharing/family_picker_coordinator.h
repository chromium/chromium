// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FamilyPickerCoordinatorDelegate;
@class RecipientInfoForIOSDisplay;

// This coordinator presents a list of Google Family members of a user that
// initiated sharing a password and allows choosing recipients.
@interface FamilyPickerCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          recipients:
                              (NSArray<RecipientInfoForIOSDisplay*>*)recipients
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<FamilyPickerCoordinatorDelegate> delegate;

// Indicates whether the family picker view displayed by this coordinator should
// navigate back to password picker view and have a back button. If false, the
// view will have a cancel button and will dismiss on tap.
@property(nonatomic) BOOL shouldNavigateBack;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_H_
