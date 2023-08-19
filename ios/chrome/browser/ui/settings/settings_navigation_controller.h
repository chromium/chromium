// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

class Browser;
@protocol BrowserCommands;
@protocol BrowsingDataCommands;
enum class DefaultBrowserPromoSource;
@protocol ImportDataControllerDelegate;
@protocol SnackbarCommands;
@class UserFeedbackData;
namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager
namespace autofill {
class CreditCard;
}  // namespace autofill

// The accessibility identifier for the settings' "Done" button.
extern NSString* const kSettingsDoneButtonId;

@protocol SettingsNavigationControllerDelegate<NSObject>

// Informs the delegate that the settings navigation controller should be
// closed.
- (void)closeSettings;

// Informs the delegate that settings navigation controller has been dismissed
// (e.g. it was swiped down). This means that closeSettings wasn't called and we
// need to perform some clean up tasks.
- (void)settingsWasDismissed;

// Asks the delegate for a handler that can be passed into child view
// controllers when they are created.
- (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
    handlerForSettings;

// Asks the delegate for an ApplicationCommands handler that can be passed into
// child view controllers when they are created.
- (id<ApplicationCommands>)handlerForApplicationCommands;

// Asks the delegate for a SnackbarCommands handler that can be passed into
// child view controllers when they are created.
- (id<SnackbarCommands>)handlerForSnackbarCommands;

@end

// Controller to modify user settings.
@interface SettingsNavigationController
    : UINavigationController <ApplicationSettingsCommands, KeyCommandActions>

// Creates a new SettingsTableViewController and the chrome around it.
// `browser` is the browser where settings are being displayed and should not be
// nil nor Off-the-Record. `delegate` may be nil.
+ (instancetype)
    mainSettingsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate;

// Creates a new AccountsTableViewController and the chrome around it.
// `browser` is the browser where settings are being displayed and should not be
// nil. `delegate` may be nil.
+ (instancetype)
    accountsControllerForBrowser:(Browser*)browser
                        delegate:
                            (id<SettingsNavigationControllerDelegate>)delegate;

// Creates a new GoogleServicesSettingsCollectionViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    googleServicesControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate;

// Creates a new SettingsNavigationController that displays the sync management
// UI. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    syncSettingsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate;

// Creates a new SyncEncryptionPassphraseCollectionViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    syncPassphraseControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate;

// Creates a new view controller presenting the saved passwords list and the
// chrome around it. `browser` is the browser where settings are being displayed
// and should not be nil. `delegate` may be nil. `showCancelButton` indicates
// whether a cancel button should be shown in the upper left corner if the
// navigation stack is empty.
+ (instancetype)
    savePasswordsControllerForBrowser:(Browser*)browser
                             delegate:(id<SettingsNavigationControllerDelegate>)
                                          delegate
                     showCancelButton:(BOOL)showCancelButton;

// Creates a new PasswordManagerViewController in search mode and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    passwordManagerSearchControllerForBrowser:(Browser*)browser
                                     delegate:
                                         (id<SettingsNavigationControllerDelegate>)
                                             delegate;

// Creates a new PasswordDetailsViewController and the chrome around it.
// `browser` is the browser where the view is being displayed and should not be
// nil. `delegate` button should be shown in the upper left corner if the
// navigation stack is empty.
+ (instancetype)
    passwordDetailsControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate
                             credential:
                                 (password_manager::CredentialUIEntry)credential
                       showCancelButton:(BOOL)showCancelButton;

// Creates and displays a new UIViewController for user to report an issue.
// `browser` is the browser where settings are being displayed and should not be
// nil. `dataSource` is used to populate the UIViewController. `dispatcher`,
// which can be nil, is an object that can perform operations for the view
// controller. `delegate` may be nil.
+ (instancetype)
    userFeedbackControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate
                    userFeedbackData:(UserFeedbackData*)userFeedbackData
                             handler:(id<ApplicationCommands>)handler;

// Creates and displays a new ImportDataTableViewController. `browserState`
// should not be nil.
// TODO(crbug.com/1018746) pass Browser instead of BrowserState
+ (instancetype)
    importDataControllerForBrowser:(Browser*)browser
                          delegate:
                              (id<SettingsNavigationControllerDelegate>)delegate
                importDataDelegate:
                    (id<ImportDataControllerDelegate>)importDataDelegate
                         fromEmail:(NSString*)fromEmail
                           toEmail:(NSString*)toEmail;

// Creates a new AutofillProfileTableViewController and the chrome around
// it. `browser` is the browser where settings are being displayed and should
// not be nil. `delegate` may be nil.
+ (instancetype)
    autofillProfileControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate;

// Creates a new PrivacyController `browser` is the browser where settings are
// being displayed and should not be nil. `delegate` may be nil.
+ (instancetype)
    privacyControllerForBrowser:(Browser*)browser
                       delegate:
                           (id<SettingsNavigationControllerDelegate>)delegate;

// Creates a new AutofillCreditCardCollectionViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    autofillCreditCardControllerForBrowser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate;

// Creates a new AutofillCreditCardEditTableViewController and the chrome around
// it. `browser` is the browser where settings are being displayed and should
// not be nil. `delegate` may be nil.
+ (instancetype)
    autofillCreditCardEditControllerForBrowser:(Browser*)browser
                                      delegate:
                                          (id<SettingsNavigationControllerDelegate>)
                                              delegate
                                    creditCard:
                                        (const autofill::CreditCard*)creditCard;

// Creates a new DefaultBrowserSettingsTableViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    defaultBrowserControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate
                          sourceForUMA:(DefaultBrowserPromoSource)source;

// Creates a new ClearBrowsingDataTableViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    clearBrowsingDataControllerForBrowser:(Browser*)browser
                                 delegate:
                                     (id<SettingsNavigationControllerDelegate>)
                                         delegate;

// Creates a new SafetyCheckTableViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    safetyCheckControllerForBrowser:(Browser*)browser
                           delegate:(id<SettingsNavigationControllerDelegate>)
                                        delegate;

// Creates a new PrivacySafeBrowsingViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    safeBrowsingControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate;

// Creates a new InactiveTabsSettingsTableViewController and the chrome around
// it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    inactiveTabsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate;

// Creates a new ContentSettingTableViewController and the chrome
// around it. `browser` is the browser where settings are being displayed and
// should not be nil. `delegate` may be nil.
+ (instancetype)
    contentSettingsControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate;

// Initializes the UINavigationController with `rootViewController`.
- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
                                   browser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
    NS_UNAVAILABLE;
- (instancetype)initWithNavigationBarClass:(Class)navigationBarClass
                              toolbarClass:(Class)toolbarClass NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns a new Done button for a UINavigationItem which will call
// closeSettings when it is pressed. Should only be called by view controllers
// owned by SettingsNavigationController.
- (UIBarButtonItem*)doneButton;

// Notifies this `SettingsNavigationController` of a dismissal such
// that it has a possibility to do necessary clean up.
- (void)cleanUpSettings;

// Closes this `SettingsNavigationController` by asking its delegate.
- (void)closeSettings;

// Pops the top view controller if there exists more than one view controller in
// the navigation stack. Closes the settings if the top view controller is the
// only view controller in the navigation stack.
- (void)popViewControllerOrCloseSettingsAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_NAVIGATION_CONTROLLER_H_
