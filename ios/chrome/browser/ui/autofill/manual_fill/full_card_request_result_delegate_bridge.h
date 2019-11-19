// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FULL_CARD_REQUEST_RESULT_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FULL_CARD_REQUEST_RESULT_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"

// Obj-C delegate to receive the success or failure result, when asking credit
// card unlocking.
@protocol FullCardRequestResultDelegateObserving<NSObject>

// Called with unlocked credit card, when CVC input succeeded.
- (void)onFullCardRequestSucceeded:(const autofill::CreditCard&)card;

// Called when CVC input didn't succeeded, including when cancelled by user.
- (void)onFullCardRequestFailed;

@end

// Bridge between cpp payments::FullCardRequest::ResultDelegate and Obj-C
// ManualFillCardMediator.
class FullCardRequestResultDelegateBridge
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  FullCardRequestResultDelegateBridge(
      id<FullCardRequestResultDelegateObserving> delegate);
  ~FullCardRequestResultDelegateBridge() override;

  base::WeakPtr<FullCardRequestResultDelegateBridge> GetWeakPtr();

 private:
  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& full_card_request,
      const autofill::CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  __weak id<FullCardRequestResultDelegateObserving> delegate_ = nil;
  base::WeakPtrFactory<FullCardRequestResultDelegateBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FullCardRequestResultDelegateBridge);
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FULL_CARD_REQUEST_RESULT_DELEGATE_BRIDGE_H_
