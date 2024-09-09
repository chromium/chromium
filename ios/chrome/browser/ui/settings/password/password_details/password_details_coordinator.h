// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"

namespace password_manager {
class AffiliatedGroup;
struct CredentialUIEntry;
}  // namespace password_manager

@protocol ApplicationCommands;
class Browser;
@protocol PasswordDetailsCoordinatorDelegate;
@protocol ReauthenticationProtocol;

// This coordinator presents a password details for the user.
@interface PasswordDetailsCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          credential:
                              (const password_manager::CredentialUIEntry&)
                                  credential
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                             context:(DetailsContext)context
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                     affiliatedGroup:(const password_manager::AffiliatedGroup&)
                                         affiliatedGroup
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                             context:(DetailsContext)context
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Displays the password data in edit mode without requiring any authentication.
- (void)showPasswordDetailsInEditModeWithoutAuthentication;

// Delegate.
@property(nonatomic, weak) id<PasswordDetailsCoordinatorDelegate> delegate;

// Whether the coordinator's view controller should be opened in edit mode.
@property(nonatomic, assign) BOOL openInEditMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_H_
