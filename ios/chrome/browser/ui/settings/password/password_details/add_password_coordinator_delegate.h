// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_DELEGATE_H_

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// Delegate for AddPasswordCoordinator.
@protocol AddPasswordCoordinatorDelegate

// Called when the add view controller is to removed.
- (void)passwordDetailsTableViewControllerDidFinish:
    (AddPasswordCoordinator*)coordinator;

// Called after a new credential is added or an existing one is updated via the
// add credential flow.
- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::PasswordForm&)password;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_COORDINATOR_DELEGATE_H_
