// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_COORDINATOR_H_

#include <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
class AutofillProfile;
}

@class AutofillProfileEditCoordinator;

@protocol AutofillProfileEditCoordinatorDelegate

// Called when the add view controller is to removed.
- (void)autofillProfileEditCoordinatorTableViewControllerDidFinish:
    (AutofillProfileEditCoordinator*)coordinator;

@end

// The coordinator for the view/edit profile screen.
@interface AutofillProfileEditCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                             profile:(const autofill::AutofillProfile&)profile
              migrateToAccountButton:(BOOL)showMigrateToAccountButton
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate.
@property(nonatomic, weak) id<AutofillProfileEditCoordinatorDelegate> delegate;

// Whether the coordinator's view controller should be opened in edit mode.
@property(nonatomic, assign) BOOL openInEditMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_COORDINATOR_H_
