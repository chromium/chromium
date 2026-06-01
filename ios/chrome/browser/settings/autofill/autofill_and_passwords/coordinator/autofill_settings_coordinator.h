// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AutofillSettingsCoordinator;

// Delegate for AutofillSettingsCoordinator.
@protocol AutofillSettingsCoordinatorDelegate <NSObject>

// Called when the coordinator should be stopped.
- (void)autofillSettingsCoordinatorDidRemove:
    (AutofillSettingsCoordinator*)coordinator;

@end

// Coordinator for the Autofill settings page.
@interface AutofillSettingsCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<AutofillSettingsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                          browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                    browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_COORDINATOR_H_
