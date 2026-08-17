// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CREDIT_CARD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CREDIT_CARD_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AutofillCreditCardCoordinator;

// Delegate for AutofillCreditCardCoordinator.
@protocol AutofillCreditCardCoordinatorDelegate <NSObject>

// Called when the coordinator is removed or dismissed.
- (void)autofillCreditCardCoordinatorDidRemove:
    (AutofillCreditCardCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_PAYMENTS_COORDINATOR_AUTOFILL_CREDIT_CARD_COORDINATOR_DELEGATE_H_
