// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_

#import <UIKit/UIKit.h>

#include "base/memory/ref_counted.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

class ChromeBrowserState;
class WebStateList;

@protocol FullCardRequestResultDelegateObserving;

// Bridge between manual fill credit card and payments' FullCardRequester to
// let user 'unlock' server side credit card by input correct CVC.
@interface ManualFillFullCardRequester : NSObject

// Inits the requests with required parameters and the |delegate| to receive the
// success/failure state of the request.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                        webStateList:(WebStateList*)webStateList
                      resultDelegate:
                          (id<FullCardRequestResultDelegateObserving>)delegate;

// Executes the request, putting up a CVC input requester then unlocking a
// server side credit card if the CVC is correct. The delegate will receive the
// result of the operation.
- (void)requestFullCreditCard:(autofill::CreditCard)card
       withBaseViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_
