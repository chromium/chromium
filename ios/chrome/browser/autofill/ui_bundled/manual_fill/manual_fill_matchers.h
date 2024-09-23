// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_MATCHERS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_MATCHERS_H_

#import <Foundation/Foundation.h>

#import "base/strings/utf_string_conversions.h"

@protocol GREYMatcher;

namespace manual_fill {

// Returns a matcher for the scroll view in keyboard accessory bar.
id<GREYMatcher> FormSuggestionViewMatcher();

// Returns a matcher for the keyboard icon in the keyboard accessory bar.
id<GREYMatcher> KeyboardIconMatcher();

// Returns a matcher for the password icon in the keyboard accessory bar.
id<GREYMatcher> PasswordIconMatcher();

// Returns a matcher for the password table view in manual fallback.
id<GREYMatcher> PasswordTableViewMatcher();

// Returns a matcher for the password search bar in manual fallback.
id<GREYMatcher> PasswordSearchBarMatcher();

// Returns a matcher for the button to open Password Manager in manual
// fallback.
id<GREYMatcher> ManagePasswordsMatcher();

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManageSettingsMatcher();

// Returns a matcher for the button to open all passwords in manual fallback.
id<GREYMatcher> OtherPasswordsMatcher();

// Returns a matcher for the button to dismiss all passwords in manual fallback.
id<GREYMatcher> OtherPasswordsDismissMatcher();

// Returns a matcher for the a password in the manual fallback list.
id<GREYMatcher> PasswordButtonMatcher();

// Returns a matcher for the profiles icon in the keyboard accessory bar.
id<GREYMatcher> ProfilesIconMatcher();

// Returns a matcher for the profiles table view in manual fallback.
id<GREYMatcher> ProfilesTableViewMatcher();

// Returns a matcher for the button to open profile settings in manual
// fallback.
id<GREYMatcher> ManageProfilesMatcher();

// Returns a matcher for the ProfileTableView window.
id<GREYMatcher> ProfileTableViewWindowMatcher();

// Returns a matcher for the credit card icon in the keyboard accessory bar.
id<GREYMatcher> CreditCardIconMatcher();

// Returns a matcher for the credit card table view in manual fallback.
id<GREYMatcher> CreditCardTableViewMatcher();

// Returns a matcher for the button to open payment method settings in manual
// fallback.
id<GREYMatcher> ManagePaymentMethodsMatcher();

// Returns a matcher for the button to add a payment method in manual
// fallback.
id<GREYMatcher> AddPaymentMethodMatcher();

// Returns a matcher for the CreditCardTableView window.
id<GREYMatcher> CreditCardTableViewWindowMatcher();

// Returns a matcher for the button to trigger password generation on manual
// fallback.
id<GREYMatcher> SuggestPasswordMatcher();

// Returns a matcher for the header view in the fallback view.
id<GREYMatcher> ExpandedManualFillHeaderView();

// Matcher for the expanded manual fill view.
id<GREYMatcher> ExpandedManualFillView();

// Matcher for the keyboard accessory's manual fill button.
id<GREYMatcher> KeyboardAccessoryManualFillButton();

// Matcher for the segmented control's address tab.
id<GREYMatcher> SegmentedControlAddressTab();

// Matcher for the segmented control's password tab.
id<GREYMatcher> SegmentedControlPasswordTab();

// Matcher for the chip button with the given `title`.
id<GREYMatcher> ChipButton(std::u16string title);

// Matcher for the expanded password manual fill view button.
id<GREYMatcher> PasswordManualFillViewButton();

}  // namespace manual_fill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_MATCHERS_H_
