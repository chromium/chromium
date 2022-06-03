// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_CREDIT_CARD_EDIT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_CREDIT_CARD_EDIT_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"

namespace autofill {
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

// The table view for the credit card editor.
@interface AutofillCreditCardEditTableViewController
    : AutofillEditTableViewController

// Initializes a AutofillCreditCardEditTableViewController with
// |creditCard| and |dataManager|. These cannot be null.
- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
               personalDataManager:(autofill::PersonalDataManager*)dataManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_CREDIT_CARD_EDIT_TABLE_VIEW_CONTROLLER_H_
