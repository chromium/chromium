// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_INTERNAL_H_

#import "ios/web_view/public/cwv_credit_card.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface CWVCreditCard ()

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
    NS_DESIGNATED_INITIALIZER;

// The internal autofill credit card that is wrapped by this object.
// Intentionally not declared as a property to avoid issues when read by
// -[NSObject valueForKey:].
- (autofill::CreditCard*)internalCard;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_INTERNAL_H_
