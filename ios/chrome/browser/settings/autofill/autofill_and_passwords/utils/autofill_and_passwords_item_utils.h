// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_

#import <Foundation/Foundation.h>

@class TableViewDetailIconItem;
@class TableViewHeaderFooterItem;
@class TableViewSwitchItem;

// Returns the detail text for the passwords item.
NSString* PasswordsItemDetailText(BOOL enabled);

// Returns the detail text for the autofill credit card item.
NSString* AutofillCreditCardItemDetailText(BOOL enabled);

// Returns the detail text for the autofill profile item.
NSString* AutofillProfileItemDetailText(BOOL enabled);

// Returns the detail text for the identity docs item.
NSString* IdentityDocsItemDetailText(BOOL enabled);

// Returns the detail text for the travel info item.
NSString* TravelInfoItemDetailText(BOOL enabled);

// Returns the detail text for the shopping info item.
NSString* ShoppingInfoItemDetailText(BOOL enabled);

// Returns the passwords item.
TableViewDetailIconItem* PasswordsItem(BOOL enabled);

// Returns the autofill credit card item.
TableViewDetailIconItem* AutofillCreditCardItem(BOOL enabled);

// Returns the autofill profile item.
TableViewDetailIconItem* AutofillProfileItem(BOOL enabled);

// Returns the identity docs item.
TableViewDetailIconItem* IdentityDocsItem(BOOL enabled);

// Returns the travel info item.
TableViewDetailIconItem* TravelInfoItem(BOOL enabled);

// Returns the shopping info item.
TableViewDetailIconItem* ShoppingInfoItem(BOOL enabled);

// Returns the autofill settings item.
TableViewDetailIconItem* AutofillSettingsItem();

// Returns the switch item for Enhanced Autofill.
//   `itemType`: The type of the item.
//   `enabled`: Whether the switch is initially on or off.
//   `target`: The target for the action selector.
//   `action`: The action selector when the switch state changes.
TableViewSwitchItem* EnhancedAutofillSwitchItem(NSInteger itemType,
                                                BOOL enabled,
                                                id target,
                                                SEL action);

// Returns the footer item for the Enhanced Autofill switch section.
TableViewHeaderFooterItem* EnhancedAutofillSwitchFooter(NSInteger itemType);

// Returns the header item for the "When on" section.
TableViewHeaderFooterItem* EnhancedAutofillWhenOnSectionHeader(
    NSInteger itemType);

// Returns the detail icon item for "Can fill difficult fields".
TableViewDetailIconItem* EnhancedAutofillCanFillDifficultFieldsItem(
    NSInteger itemType);

// Returns the header item for the "Things to consider" section.
TableViewHeaderFooterItem* EnhancedAutofillThingsToConsiderSectionHeader(
    NSInteger itemType);

// Returns the detail icon item for "Data usage".
TableViewDetailIconItem* EnhancedAutofillDataUsageItem(NSInteger itemType);

// Returns the detail icon item for "Enterprise managed logging disabled".
TableViewDetailIconItem* EnhancedAutofillEnterpriseManagedLoggingDisabledItem(
    NSInteger itemType);

// Returns the switch item for Autofill AI User Verification.
//   `itemType`: The type of the item.
//   `enabled`: Whether the switch interaction is enabled.
//   `on`: Whether the switch is initially on or off.
//   `target`: The target for the action selector.
//   `action`: The action selector when the switch state changes.
TableViewSwitchItem* AutofillVerificationSwitchItem(NSInteger itemType,
                                                    BOOL enabled,
                                                    BOOL on,
                                                    id target,
                                                    SEL action);

// Returns the footer item for the Autofill AI User Verification switch section.
TableViewHeaderFooterItem* AutofillVerificationSwitchFooter(NSInteger itemType);

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_
