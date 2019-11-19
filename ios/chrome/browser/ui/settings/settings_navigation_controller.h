// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/application_commands.h"

class Browser;
@protocol BrowserCommands;
@protocol ImportDataControllerDelegate;
@protocol UserFeedbackDataSource;

// The accessibility identifier for the settings' "Done" button.
extern NSString* const kSettingsDoneButtonId;

@protocol SettingsControllerProtocol<NSObject>

@optional

// Notifies the controller that the settings screen is being dismissed.
- (void)settingsWillBeDismissed;

// Notifies the controller that is popped out from the settings navigation
// controller.
- (void)viewControllerWasPopped;

@end

@protocol SettingsNavigationControllerDelegate<NSObject>

// Informs the delegate that the settings navigation controller should be
// closed.
- (void)closeSettings;

// Informs the delegate that settings navigation controller has been dismissed
// (e.g. it was swiped down). This means that closeSettings wasn't called and we
// need to perform some clean up tasks.
- (void)settingsWasDismissed;

// Asks the delegate for a dispatcher that can be passed into child view
// controllers when they are created.
- (id<ApplicationCommands, BrowserCommands>)dispatcherForSettings;

@end

// Controller to modify user settings.
@interface SettingsNavigationController
    : UINavigationController<ApplicationSettingsCommands>

// Creates a new SettingsTableViewController and the chrome around it.
// |browser| is the browser where settings are being displayed and should not be
// nil nor Off-the-Record. |delegate| may be nil.
+ (instancetype)
    mainSettingsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate;

// Creates a new AccountsTableViewController and the chrome around it.
// |browser| is the browser where settings are being displayed and should not be
// nil. |delegate| may be nil.
+ (instancetype)
    accountsControllerForBrowser:(Browser*)browser
                        delegate:
                            (id<SettingsNavigationControllerDelegate>)delegate;

// Creates a new GoogleServicesSettingsCollectionViewController and the chrome
// around it. |browser| is the browser where settings are being displayed and
// should not be nil. |delegate| may be nil.
+ (instancetype)
    googleServicesControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate;

// Creates a new SyncEncryptionPassphraseCollectionViewController and the chrome
// around it. |browser| is the browser where settings are being displayed and
// should not be nil. |delegate| may be nil.
+ (instancetype)
    syncPassphraseControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate;

// Creates a new SavePasswordsCollectionViewController and the chrome around it.
// |browser| is the browser where settings are being displayed and should not be
// nil. |delegate| may be nil.
+ (instancetype)
    savePasswordsControllerForBrowser:(Browser*)browser
                             delegate:(id<SettingsNavigationControllerDelegate>)
                                          delegate;

// Creates and displays a new UIViewController for user to report an issue.
// |browser| is the browser where settings are being displayed and should not be
// nil. |dataSource| is used to populate the UIViewController. |dispatcher|,
// which can be nil, is an object that can perform operations for the view
// controller. |delegate| may be nil.
+ (instancetype)
    userFeedbackControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate
                  feedbackDataSource:(id<UserFeedbackDataSource>)dataSource
                          dispatcher:(id<ApplicationCommands>)dispatcher;

// Creates and displays a new ImportDataTableViewController. |browserState|
// should not be nil.
// TODO(crbug.com/1018746) pass Browser instead of BrowserState
+ (instancetype)
    importDataControllerForBrowser:(Browser*)browser
                          delegate:
                              (id<SettingsNavigationControllerDelegate>)delegate
                importDataDelegate:
                    (id<ImportDataControllerDelegate>)importDataDelegate
                         fromEmail:(NSString*)fromEmail
                           toEmail:(NSString*)toEmail
                        isSignedIn:(BOOL)isSignedIn;

// Creates a new AutofillProfileTableViewController and the chrome around
// it. |browser| is the browser where settings are being displayed and should
// not be nil. |delegate| may be nil.
+ (instancetype)
    autofillProfileControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate;

// Creates a new AutofillCreditCardCollectionViewController and the chrome
// around it. |browser| is the browser where settings are being displayed and
// should not be nil. |delegate| may be nil.
+ (instancetype)
    autofillCreditCardControllerForBrowser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate;

// Initializes the UINavigationController with |rootViewController|.
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

// Notifies this |SettingsNavigationController| of a dismissal such
// that it has a possibility to do necessary clean up.
- (void)cleanUpSettings;

// Closes this |SettingsNavigationController| by asking its delegate.
- (void)closeSettings;

// Pops the top view controller if there exists more than one view controller in
// the navigation stack. Closes the settings if the top view controller is the
// only view controller in the navigation stack.
- (void)popViewControllerOrCloseSettingsAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_NAVIGATION_CONTROLLER_H_
