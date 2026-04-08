// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AutofillAndPasswordsCoordinator;

// Delegate for AutofillAndPasswordsCoordinator.
@protocol AutofillAndPasswordsCoordinatorDelegate <NSObject>

// Called when the view controller is removed from navigation controller.
- (void)autofillAndPasswordsCoordinatorDidRemove:
    (AutofillAndPasswordsCoordinator*)coordinator;

@end

// Coordinator for the Autofill and Passwords settings page.
@interface AutofillAndPasswordsCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<AutofillAndPasswordsCoordinatorDelegate> delegate;

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_COORDINATOR_H_
