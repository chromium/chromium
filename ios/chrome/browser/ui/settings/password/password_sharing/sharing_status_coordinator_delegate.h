// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_COORDINATOR_DELEGATE_H_

@class SharingStatusCoordinator;

// Delegate for SharingStatusCoordinator.
@protocol SharingStatusCoordinatorDelegate

// Called when the user cancels or dismisses the sharing status view.
- (void)sharingStatusCoordinatorWasDismissed:
    (SharingStatusCoordinator*)coordinator;

// Called when the animation finishes and the user does not cancel it. The
// actual password sharing should kick off at this point in the main password
// sharing coordinator.
- (void)startPasswordSharing;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_COORDINATOR_DELEGATE_H_
