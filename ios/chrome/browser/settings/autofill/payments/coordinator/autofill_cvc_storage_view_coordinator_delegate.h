// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CVC_STORAGE_VIEW_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CVC_STORAGE_VIEW_COORDINATOR_DELEGATE_H_

@class AutofillCvcStorageViewCoordinator;

// Delegate for AutofillCvcStorageViewCoordinator.
@protocol AutofillCvcStorageViewCoordinatorDelegate

- (void)autofillCvcStorageCoordinatorWantsToBeStopped:
    (AutofillCvcStorageViewCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CVC_STORAGE_VIEW_COORDINATOR_DELEGATE_H_
