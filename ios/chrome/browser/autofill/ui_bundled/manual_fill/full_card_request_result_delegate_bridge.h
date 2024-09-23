// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FULL_CARD_REQUEST_RESULT_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FULL_CARD_REQUEST_RESULT_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"

// Obj-C delegate to receive the success or failure result, when asking credit
// card unlocking.
@protocol FullCardRequestResultDelegateObserving <NSObject>

// Called with unlocked credit card, when authentication succeeded.
- (void)onFullCardRequestSucceeded:(const autofill::CreditCard&)card
                         fieldType:(manual_fill::PaymentFieldType)fieldType;

// Called when authentication didn't succeeded, including when cancelled by
// user.
- (void)onFullCardRequestFailed;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FULL_CARD_REQUEST_RESULT_DELEGATE_BRIDGE_H_
