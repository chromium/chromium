// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_EGTEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_EGTEST_UTILS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@class GREYElementInteraction;
@protocol GREYMatcher;

namespace password_manager_test_utils {

// How many points to scroll at a time when searching for an element. Setting it
// too low means searching takes too long and the test might time out. Setting
// it too high could result in scrolling way past the searched element.
constexpr int kScrollAmount = 150;

// Error message for when a new password form wasn't successfully saved in the
// password store.
constexpr NSString* kPasswordStoreErrorMessage =
    @"Stored form was not found in the PasswordStore results.";

// Matcher for a specific state of the Password Checkup cell in the
// Password Manager.
id<GREYMatcher> PasswordCheckupCellForState(PasswordCheckUIState state,
                                            int number);

// Matcher for the table view of a password issues page.
id<GREYMatcher> PasswordIssuesTableView();

// Matcher for the password in a password details view.
id<GREYMatcher> PasswordDetailPassword();

// Matcher for the "Edit" button of the navigation bar.
id<GREYMatcher> NavigationBarEditButton();

// Matcher for the "Done" button of the navigation bar that's showing when in
// edit mode.
id<GREYMatcher> EditDoneButton();

// Matcher for the "Save Password" button the confirmation dialog that pops up
// in the password details page after editing a password.
id<GREYMatcher> EditPasswordConfirmationButton();

// Matcher for the "Delete Password" associated with the credential with given
// `username` and `password`.
id<GREYMatcher> DeleteButtonForUsernameAndPassword(NSString* username,
                                                   NSString* password);

// Matcher for the "Delete" button of the confirmation dialog that pops up in
// the password details page when trying to delete a password.
id<GREYMatcher> DeletePasswordConfirmationButton();

// GREYElementInteraction* for the item on the password issues list
// with the given `matcher`. It scrolls in `direction` if necessary to ensure
// that the matched item is interactable.
GREYElementInteraction* GetInteractionForIssuesListItem(
    id<GREYMatcher> matcher,
    GREYDirection direction);

// GREYElementInteraction* for the cell on the password issues list with
// the given `username` and `domain`. It scrolls down if necessary to ensure
// that the matched cell is interactable. `compromised_description` is nil when
// the password is not compromised.
GREYElementInteraction* GetInteractionForPasswordIssueEntry(
    NSString* domain,
    NSString* username,
    NSString* compromised_description = nil);

// Saves a password form in the store.
void SavePasswordForm(NSString* password = @"concrete password",
                      NSString* username = @"concrete username",
                      NSString* origin = @"https://example.com");

// Saves a compromised password form in the store.
void SaveCompromisedPasswordForm(NSString* password = @"concrete password",
                                 NSString* username = @"concrete username",
                                 NSString* origin = @"https://example.com");

// Saves a muted compromised password form in the store.
void SaveMutedCompromisedPasswordForm(
    NSString* origin = @"https://example.com",
    NSString* username = @"concrete username",
    NSString* password = @"concrete password");

// Opens the Password Manager page from the NTP.
void OpenPasswordManager();

// Taps the "Edit" button of the navigation bar.
void TapNavigationBarEditButton();

// Deletes the credential with given `username` and `password` from the details
// page.
void DeleteCredential(NSString* username, NSString* password);

}  // namespace password_manager_test_utils

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_EGTEST_UTILS_H_
