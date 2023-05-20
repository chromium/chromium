// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_

@class PasswordDetailsCoordinator;

// Delegate for PasswordIssuesCoordinator.
@protocol PasswordDetailsCoordinatorDelegate

// Called when the view controller was removed from navigation controller.
- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
