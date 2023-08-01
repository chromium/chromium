// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_COORDINATOR_DELEGATE_H_

@class AutofillAddCreditCardCoordinator;

// Delegate for the autofill add creditc card coordinator.
@protocol AutofillAddCreditCardCoordinatorDelegate <NSObject>

// Requests the delegate to stop the `coordinator`
- (void)autofillAddCreditCardCoordinatorWantsToBeStopped:
    (AutofillAddCreditCardCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_COORDINATOR_DELEGATE_H_
