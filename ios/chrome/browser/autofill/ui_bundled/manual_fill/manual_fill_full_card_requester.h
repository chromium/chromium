// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/credit_card.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

class WebStateList;

@protocol FullCardRequestResultDelegateObserving;

// Bridge between manual fill credit card and payments' FullCardRequester to
// let user 'unlock' server side credit card by input correct CVC.
@interface ManualFillFullCardRequester : NSObject

// Inits the requests with required parameters and the `delegate` to receive the
// success/failure state of the request.
- (instancetype)initWithBrowserState:(ProfileIOS*)profile
                        webStateList:(WebStateList*)webStateList
                      resultDelegate:
                          (id<FullCardRequestResultDelegateObserving>)delegate;

// Executes the request, putting up a CVC input requester then unlocking a
// server side credit card if the CVC is correct. The delegate will receive the
// result of the operation.
- (void)requestFullCreditCard:(const autofill::CreditCard)card
       withBaseViewController:(UIViewController*)viewController
                   recordType:(autofill::CreditCard::RecordType)recordType
                    fieldType:(manual_fill::PaymentFieldType)fieldType;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_
