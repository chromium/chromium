// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

@class PasswordDetailsCoordinator;

// Delegate for PasswordIssuesCoordinator.
@protocol
    PasswordDetailsCoordinatorDelegate <PasswordManagerReauthenticationDelegate>

// Called when the view controller was removed from navigation controller.
- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator;

// Called when the user tapped on the cancel button. This is never called when
// the view is presented in the Settings context, because in these cases there
// is a Back button instead of Cancel.
@optional
- (void)passwordDetailsCancelButtonWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
