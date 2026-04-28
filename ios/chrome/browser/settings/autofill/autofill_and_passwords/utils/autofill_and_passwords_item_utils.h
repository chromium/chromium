// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_

#import <Foundation/Foundation.h>

@class TableViewDetailIconItem;

// Returns the detail text for the passwords item.
NSString* PasswordsItemDetailText(BOOL enabled);

// Returns the detail text for the autofill credit card item.
NSString* AutofillCreditCardItemDetailText(BOOL enabled);

// Returns the detail text for the autofill profile item.
NSString* AutofillProfileItemDetailText(BOOL enabled);

// Returns the detail text for the identity docs item.
NSString* IdentityDocsItemDetailText(BOOL enabled);

// Returns the passwords item.
TableViewDetailIconItem* PasswordsItem(BOOL enabled);

// Returns the autofill credit card item.
TableViewDetailIconItem* AutofillCreditCardItem(BOOL enabled);

// Returns the autofill profile item.
TableViewDetailIconItem* AutofillProfileItem(BOOL enabled);

// Returns the identity docs item.
TableViewDetailIconItem* IdentityDocsItem(BOOL enabled);

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_
