// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller_delegate.h"

namespace autofill {
class PersonalDataManager;
}

@protocol AddCreditCardMediatorDelegate;

// The Mediator for validating and saving the credit card.
@interface AutofillAddCreditCardMediator
    : NSObject <AddCreditCardViewControllerDelegate>

// Designated initializer. |addCreditCardMediatorDelegate| and |dataManager|
// should not be nil.
- (instancetype)initWithDelegate:(id<AddCreditCardMediatorDelegate>)
                                     addCreditCardMediatorDelegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_MEDIATOR_H_
