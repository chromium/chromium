// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SETTINGS_COMMANDS_H_

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill
enum class DefaultBrowserSettingsPageSource;
namespace password_manager {
struct CredentialUIEntry;
enum class PasswordCheckReferrer;
}  // namespace password_manager

@protocol SettingsCommands

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the accounts settings UI, presenting from `baseViewController`. If
// `baseViewController` is nil BVC will be used as presenterViewController.
// `skipIfUINotAvailable` if YES, this command will be ignored when the tab
// is already presenting any view controllers.
- (void)showAccountsSettingsFromViewController:
            (UIViewController*)baseViewController
                          skipIfUINotAvailable:(BOOL)skipIfUINotAvailable;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the Google services settings UI, presenting from `baseViewController`.
// If `baseViewController` is nil BVC will be used as presenterViewController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the Sync settings UI, presenting from `baseViewController`.
// If `baseViewController` is nil BVC will be used as presenterViewController.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the sync encryption passphrase UI, presenting from
// `baseViewController`.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of saved passwords in the settings. `showCancelButton`
// indicates whether a cancel button should be added as the left navigation item
// of the saved passwords view.
- (void)showSavedPasswordsSettingsFromViewController:
            (UIViewController*)baseViewController
                                    showCancelButton:(BOOL)showCancelButton;

// Shows the password details page for a credential. `editMode` indicates
// whether the details page should be opened in edit mode.
- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode;

// Shows the address details view. `editMode` indicates whether the details page
// should be opened in edit mode. `offerMigrateToAccount` indicates whether or
// not the option to migrate the address to the account should be available.
- (void)showAddressDetails:(autofill::AutofillProfile)address
                inEditMode:(BOOL)editMode
     offerMigrateToAccount:(BOOL)offerMigrateToAccount;

// Shows the list of profiles (addresses) in the settings.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of credit cards in the settings.
- (void)showCreditCardSettings;

// Shows the credit card details view. `editMode` indicates whether the details
// page should be opened in edit mode.
- (void)showCreditCardDetails:(autofill::CreditCard)creditCard
                   inEditMode:(BOOL)editMode;

// Shows the settings page informing the user how to set Chrome as the default
// browser.
- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserSettingsPageSource)
                                                source;

// Shows the settings page allowing the user to clear their browsing data.
- (void)showClearBrowsingDataSettings;

// Shows the Safety Check page and starts the Safety Check for `referrer`.
- (void)showAndStartSafetyCheckForReferrer:
    (password_manager::PasswordCheckReferrer)referrer;

// Shows the Safe Browsing page.
- (void)showSafeBrowsingSettings;

// Navigates the user to the Safe Browsing settings menu page when the user
// clicks the inline promo's primary button.
- (void)showSafeBrowsingSettingsFromPromoInteraction;

// Shows the Password Manager's search page.
- (void)showPasswordSearchPage;

// Shows the Content Settings page in the settings on top of baseViewController.
- (void)showContentsSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Notifications Settings page in the settings.
- (void)showNotificationsSettings;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SETTINGS_COMMANDS_H_
