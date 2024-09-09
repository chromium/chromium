// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_H_

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace password_manager {
struct CredentialUIEntry;
class SavedPasswordsPresenter;
}  // namespace password_manager

@protocol PasswordSharingCoordinatorDelegate;

// This is the main coordinator for the password sharing flow initiated from a
// password details view. It coordinates the whole flow including fetching
// recipient candidates, dispatching child coordinators (e.g. family picker,
// password picker) and sending sharing invitations.
@interface PasswordSharingCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   credentials:
                       (const std::vector<password_manager::CredentialUIEntry>&)
                           credentials
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initializes appropriate coordinator displaying the first step in the flow
// based on the fetched status.
- (void)showFirstStep;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<PasswordSharingCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_H_
