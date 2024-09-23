// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_DELEGATE_H_

@class ManualFillAllPasswordCoordinator;

@protocol ManualFillAllPasswordCoordinatorDelegate <NSObject>

// Resquests the delegate to dismiss the coordinator.
- (void)manualFillAllPasswordCoordinatorWantsToBeDismissed:
    (ManualFillAllPasswordCoordinator*)coordinator;

// Requests the delegate to open the details of a credential in edit mode.
- (void)manualFillAllPasswordCoordinator:
            (ManualFillAllPasswordCoordinator*)coordinator
    didTriggerOpenPasswordDetailsInEditMode:
        (password_manager::CredentialUIEntry)credential;

@end
#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_DELEGATE_H_
