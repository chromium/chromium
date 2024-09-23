// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_DELEGATE_H_

@class PasswordProtectionCoordinator;

// Delegate for the password protection coordinator.
@protocol PasswordProtectionCoordinatorDelegate <NSObject>

// Requests the delegate to stop the coordinator.
- (void)passwordProtectionCoordinatorWantsToBeStopped:
    (PasswordProtectionCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_DELEGATE_H_
