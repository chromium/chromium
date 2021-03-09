// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_manager_client_bridge.h"
#import "components/password_manager/ios/password_manager_driver_bridge.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_client.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_driver.h"
#import "ios/web/public/web_state_observer_bridge.h"

@protocol ApplicationCommands;
class Browser;
@class NotifyUserAutoSigninViewController;
@protocol PasswordBreachCommands;
@protocol PasswordsUiDelegate;
@class SharedPasswordController;

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

// Delegate for registering view controller and displaying its view. Used to
// add views to BVC.
@protocol PasswordControllerDelegate

// Adds |viewController| as child controller in order to display auto sign-in
// notification. Returns YES if view was displayed, NO otherwise.
- (BOOL)displaySignInNotification:(UIViewController*)viewController
                        fromTabId:(NSString*)tabId;

// Opens the list of saved passwords in the settings.
- (void)displaySavedPasswordList;

@end

// Per-tab password controller. Handles password autofill and saving.
@interface PasswordController
    : NSObject <CRWWebStateObserver, IOSChromePasswordManagerClientBridge>

// An object that can provide suggestions from this PasswordController.
@property(nonatomic, readonly) id<FormSuggestionProvider> suggestionProvider;

// The PasswordManagerClient owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerClient* passwordManagerClient;

// The PasswordManagerDriver owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerDriver* passwordManagerDriver;

// The base view controller from which to present UI.
@property(nonatomic, readwrite, weak) UIViewController* baseViewController;

// Delegate used by this PasswordController to show UI on BVC.
@property(weak, nonatomic) id<PasswordControllerDelegate> delegate;

// The browser.
@property(nonatomic, assign) Browser* browser;

// The shared password controller that handles all non //ios/chrome specific
// business logic.
@property(nonatomic, readonly)
    SharedPasswordController* sharedPasswordController;

// |webState| should not be nil.
- (instancetype)initWithWebState:(web::WebState*)webState;

// This is just for testing.
- (instancetype)
   initWithWebState:(web::WebState*)webState
             client:(std::unique_ptr<password_manager::PasswordManagerClient>)
                        passwordManagerClient NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CONTROLLER_H_
