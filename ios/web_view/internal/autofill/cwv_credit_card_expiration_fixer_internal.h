// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_EXPIRATION_FIXER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_EXPIRATION_FIXER_INTERNAL_H_

#import "ios/web_view/public/cwv_credit_card_expiration_fixer.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string16.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface CWVCreditCardExpirationFixer ()

// Initializes with a |creditCard| that needs its expiration date fixed and
// a |callback| to be invoked with a new expiration. |callback| should be ran
// with a month and year in MM and YYYY format.
- (instancetype)
    initWithCreditCard:(const autofill::CreditCard&)creditCard
              callback:(base::OnceCallback<void(const base::string16&,
                                                const base::string16&)>)callback
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_EXPIRATION_FIXER_INTERNAL_H_
