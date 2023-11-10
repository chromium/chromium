// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APPLICATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APPLICATION_COMMANDS_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"
#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

class GURL;
@class OpenNewTabCommand;
@class ShowSigninCommand;
@class StartVoiceSearchCommand;
@class UIViewController;
enum class DefaultBrowserPromoSource;
namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer
namespace autofill {
class CreditCard;
}  // namespace autofill
namespace password_manager {
struct CredentialUIEntry;
enum class PasswordCheckReferrer;
enum class WarningType;
}  // namespace password_manager

// This protocol groups commands that are part of ApplicationCommands, but
// may also be forwarded directly to a settings navigation controller.
@protocol ApplicationSettingsCommands

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the accounts settings UI, presenting from `baseViewController`. If
// `baseViewController` is nil BVC will be used as presenterViewController.
// `skipIfUINotAvailable` if YES, this command will be ignored when the tab
// is already presenting any view controllers.
- (void)showAccountsSettingsFromViewController:
            (UIViewController*)baseViewController
                          skipIfUINotAvailable:(BOOL)skipIfUINotAvailable;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Google services settings UI, presenting from `baseViewController`.
// If `baseViewController` is nil BVC will be used as presenterViewController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Sync settings UI, presenting from `baseViewController`.
// If `baseViewController` is nil BVC will be used as presenterViewController.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
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

// Shows the password details page for a credential.
// `showCancelButton` indicates whether a cancel button should be added as the
// left navigation item of the password details view.
- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                        showCancelButton:(BOOL)showCancelButton;

// Shows the list of profiles (addresses) in the settings.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of credit cards in the settings.
- (void)showCreditCardSettings;

// Shows the credit card details view.
- (void)showCreditCardDetails:(const autofill::CreditCard*)creditCard;

// Shows the settings page informing the user how to set Chrome as the default
// browser.
- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserPromoSource)source;

// Shows the settings page allowing the user to clear their browsing data.
- (void)showClearBrowsingDataSettings;

// Shows the Safety Check page and starts the Safety Check for `referrer`.
// `showHalfSheet` determines whether the Safety Check will be displayed as a
// half-sheet, or full-page modal.
- (void)showAndStartSafetyCheckInHalfSheet:(BOOL)showHalfSheet
                                  referrer:
                                      (password_manager::PasswordCheckReferrer)
                                          referrer;

// Shows the Safe Browsing page.
- (void)showSafeBrowsingSettings;

// Shows the Password Manager's search page.
- (void)showPasswordSearchPage;

// Shows the Content Settings page in the settings on top of baseViewController.
- (void)showContentsSettingsFromViewController:
    (UIViewController*)baseViewController;

@end

// Protocol for commands that will generally be handled by the application,
// rather than a specific tab; in practice this means the MainController
// instance.
// This protocol includes all of the methods in ApplicationSettingsCommands; an
// object that implements the methods in this protocol should be able to forward
// ApplicationSettingsCommands to the settings view controller if necessary.

@protocol ApplicationCommands <NSObject, ApplicationSettingsCommands>

// Dismisses all modal dialogs with a completion block that is called when
// modals are dismissed (animations done).
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion;

// Shows the Password Checkup page for `referrer`.
- (void)showPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer;

// Opens the Password Issues list displaying compromised, weak or reused
// credentials for `warningType` and `referrer`.
- (void)
    showPasswordIssuesWithWarningType:(password_manager::WarningType)warningType
                             referrer:(password_manager::PasswordCheckReferrer)
                                          referrer;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Settings UI, presenting from `baseViewController`.
- (void)showSettingsFromViewController:(UIViewController*)baseViewController;

// Presents the Trusted Vault reauth dialog.
// `baseViewController` presents the sign-in.
// `trigger` UI elements where the trusted vault reauth has been triggered.
- (void)
    showTrustedVaultReauthForFetchKeysFromViewController:
        (UIViewController*)baseViewController
                                                 trigger:
                                                     (syncer::
                                                          TrustedVaultUserActionTriggerForUMA)
                                                         trigger;

// Presents the Trusted Vault degraded recoverability (to enroll additional
// recovery factors).
// `baseViewController` presents the sign-in.
// `trigger` UI elements where the trusted vault reauth has been triggered.
- (void)
    showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
        (UIViewController*)baseViewController
                                                              trigger:
                                                                  (syncer::
                                                                       TrustedVaultUserActionTriggerForUMA)
                                                                      trigger;

// Starts a voice search on the current BVC.
- (void)startVoiceSearch;

// Shows the History UI.
- (void)showHistory;

// Closes the History UI and opens a URL.
- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command;

// Closes the History UI.
- (void)closeSettingsUI;

// Prepare to show the TabSwitcher UI.
- (void)prepareTabSwitcher;

// Shows the TabSwitcher UI.
- (void)displayTabSwitcherInGridLayout;

// Same as displayTabSwitcherInGridLayout, but also force tab switcher to
// regular tabs page.
- (void)displayRegularTabSwitcherInGridLayout;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the settings Privacy UI.
- (void)showPrivacySettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Report an Issue UI, presenting from `baseViewController`.
- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender;

// Shows the Report an Issue UI, presenting from `baseViewController`, using
// `specificProductData` for additional product data to be sent in the report.
- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData;

// Opens the `command` URL in a new tab.
// TODO(crbug.com/907527): Check if it is possible to merge it with the
// URLLoader methods.
- (void)openURLInNewTab:(OpenNewTabCommand*)command;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the signin UI, presenting from `baseViewController`.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the consistency promo UI that allows users to sign in to Chrome using
// the default accounts on the device.
// Redirects to `url` when the sign-in flow is complete.
- (void)showWebSigninPromoFromViewController:
            (UIViewController*)baseViewController
                                         URL:(const GURL&)url;

// Shows a notification with the signed-in user account.
- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController;

// Sets whether the UI is displaying incognito content.
- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible;

// Open a new window with `userActivity`
- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APPLICATION_COMMANDS_H_
