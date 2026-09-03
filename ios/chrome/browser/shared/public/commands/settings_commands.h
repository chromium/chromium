// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SETTINGS_COMMANDS_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/ios/block_types.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill
namespace autofill::autofill_metrics {
enum class AutofillSettingsReferrer;
}  // namespace autofill::autofill_metrics
enum class DefaultBrowserSettingsPageSource;
namespace password_manager {
struct CredentialUIEntry;
enum class PasswordCheckReferrer;
}  // namespace password_manager
enum class PushNotificationClientId;

@protocol SettingsCommands

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the accounts settings UI, presenting from `baseViewController`. If
// `baseViewController` is nil BVC will be used as presenterViewController.
// `skipIfUINotAvailable` if YES, this command will be ignored when the tab
// is already presenting any view controllers.
- (void)showAccountsSettingsFromViewController:
            (UIViewController*)baseViewController
                          skipIfUINotAvailable:(BOOL)skipIfUINotAvailable;

// Shows the Gemini settings UI.
- (void)showGeminiSettings;

// Shows the Suggestions from Gemini Help Improve settings UI.
- (void)showSuggestionsFromGeminiHelpImprove;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the Google services settings UI, presenting from `baseViewController`.
// If `baseViewController` is nil BVC will be used as presenterViewController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the Sync settings UI, presenting from `baseViewController`.
// If `baseViewController` is nil BVC will be used as presenterViewController.
// The user must be signed-in and sign-in must be enabled.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the sync encryption passphrase UI, presenting from
// `baseViewController`.
// Does nothing if the current scene is blocked.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the sync encryption passphrase UI, presenting from
// `baseViewController`. `completion` is executed after the UI is dismissed.
// Does nothing if the current scene is blocked.
- (void)showSyncPassphraseSettingsFromViewController:
            (UIViewController*)baseViewController
                                          completion:
                                              (ProceduralBlock)completion;

// Shows the list of saved passwords in the settings.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the saved passwords settings index. `shouldShowLevelUpWalkthroughIPH`
// indicates whether the Level Up walkthrough IPH should be shown.
- (void)showSavedPasswordsSettingsFromViewController:
            (UIViewController*)baseViewController
                     shouldShowLevelUpWalkthroughIPH:
                         (BOOL)shouldShowLevelUpWalkthroughIPH;

// Shows Password Settings in the settings.
- (void)showPasswordSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Autofill and Passwords settings page.
- (void)showAutofillAndPasswordsSettingsWithReferrer:
    (autofill::autofill_metrics::AutofillSettingsReferrer)referrer;

// Shows the Identity Docs settings page.
- (void)showIdentityDocsWithReferrer:
    (autofill::autofill_metrics::AutofillSettingsReferrer)referrer;

// Shows the Travel Info settings page.
- (void)showTravelWithReferrer:
    (autofill::autofill_metrics::AutofillSettingsReferrer)referrer;

// Shows the Shopping settings page.
- (void)showShoppingWithReferrer:
    (autofill::autofill_metrics::AutofillSettingsReferrer)referrer;

// Shows password manager on main page with a purpose to run the credential
// exchange import flow. `UUID` is a token received from the OS during app
// launch needed to receive credentials from an OS library.
- (void)showPasswordManagerForCredentialImport:(NSUUID*)UUID
    API_AVAILABLE(ios(26.0));

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

// Shows the default search engine selection settings.
- (void)showDefaultSearchEngineSettings;

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

// Shows the Notification Settings page and highlights the row for the push
// notification client with the given `clientID`.
- (void)showNotificationsSettingsAndHighlightClient:
    (std::optional<PushNotificationClientId>)clientID;

// Shows the Autofill settings UI.
- (void)showAutofillSettings;

// Shows the Autofill settings UI from an Autofill notice (no back button).
- (void)showAutofillSettingsFromNotice;

// Shows the Enhanced Autofill settings UI (no back button). `completion` is
// executed after the UI is dismissed.
- (void)showEnhancedAutofillSettingsWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SETTINGS_COMMANDS_H_
