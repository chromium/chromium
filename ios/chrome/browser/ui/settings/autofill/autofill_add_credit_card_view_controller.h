// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"

// Accessibility identifier for the 'Add Credit Card' view.
extern NSString* const kAddCreditCardViewID;
// Accessibility identifier for the 'Add' Credit Card button.
extern NSString* const kSettingsAddCreditCardButtonID;
// Accessibility identifier for the Add Credit Card 'Cancel' button.
extern NSString* const kSettingsAddCreditCardCancelButtonID;

@protocol AddCreditCardViewControllerDelegate;

// The view controller for adding new credit card.
@interface AutofillAddCreditCardViewController : AutofillEditTableViewController

// Initializes a AutofillAddCreditCardViewController with passed delegate.
- (instancetype)initWithDelegate:
    (id<AddCreditCardViewControllerDelegate>)delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Returns "YES" if any of tableview cells has user input.
@property(nonatomic, getter=tableViewHasUserInput, readonly)
    BOOL tableViewHasUserInput;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_H_
