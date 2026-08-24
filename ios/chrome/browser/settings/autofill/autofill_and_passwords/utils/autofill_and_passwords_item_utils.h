// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UTILS_AUTOFILL_AND_PASSWORDS_ITEM_UTILS_H_

#import <Foundation/Foundation.h>

@class TableViewDetailIconItem;
@class TableViewHeaderFooterItem;
@class TableViewSwitchItem;

// Returns the detail text based on the `enabled` state. If `enabled` is YES, it
// returns the localized "On" string; otherwise it returns the localized "Off"
// string.
NSString* DetailTextForEnabledState(BOOL enabled);

// Returns the passwords item. `enabled` indicates if the passwords saving
// service is enabled.
TableViewDetailIconItem* PasswordsItem(BOOL enabled);

// Returns the autofill credit card item. `enabled` indicates if autofill for
// credit cards is enabled.
TableViewDetailIconItem* AutofillCreditCardItem(BOOL enabled);

// Returns the autofill profile item. `enabled` indicates if autofill for
// profiles/addresses is enabled.
TableViewDetailIconItem* AutofillProfileItem(BOOL enabled);

// Returns the identity docs item. `enabled` indicates if autofill for identity
// documents is enabled.
TableViewDetailIconItem* IdentityDocsItem(BOOL enabled);

// Returns the travel info item. `enabled` indicates if autofill for travel
// info is enabled.
TableViewDetailIconItem* TravelInfoItem(BOOL enabled);

// Returns the shopping info item. `enabled` indicates if autofill for shopping
// info is enabled.
TableViewDetailIconItem* ShoppingInfoItem(BOOL enabled);

// Returns the suggestions from gemini item. `enabled` indicates if Suggestions
// from Gemini is enabled.
TableViewDetailIconItem* SuggestionsFromGeminiItem(BOOL enabled);

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
