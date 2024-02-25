// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_COORDINATOR_DELEGATE_H_

@class PasswordSharingFirstRunCoordinator;

// Delegate for PasswordSharingFirstRunCoordinator.
@protocol PasswordSharingFirstRunCoordinatorDelegate

// Called when the user enters password sharing flow from the first run view by
// clicking the "Share" button.
- (void)passwordSharingFirstRunCoordinatorDidAccept:
    (PasswordSharingFirstRunCoordinator*)coordinator;

// Called when the user cancels or dismisses the password sharing first run
// view.
- (void)passwordSharingFirstRunCoordinatorWasDismissed:
    (PasswordSharingFirstRunCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_COORDINATOR_DELEGATE_H_
