// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_COORDINATOR_H_

#include <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutofillAddCreditCardCoordinatorDelegate;

// The coordinator for add credit card screen.
@interface AutofillAddCreditCardCoordinator : ChromeCoordinator

@property(weak, nonatomic) id<AutofillAddCreditCardCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_COORDINATOR_H_
