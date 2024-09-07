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

// Used when the family (recipient) picker is the first view displayed in the
// sharing flow. This happens when the user initiated sharing from a credential
// group containing only one credential.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
    NS_DESIGNATED_INITIALIZER;

// Used when the sharing flow is initiated from a credential group that has more
// than 1 credential. In this case the first view in the flow is a credential
// picker and the family picker should be pushed in its `navigationController`
// instead of being presented.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          recipients:
                              (NSArray<RecipientInfoForIOSDisplay*>*)recipients;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<FamilyPickerCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_H_
