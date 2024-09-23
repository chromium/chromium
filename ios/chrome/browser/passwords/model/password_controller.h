// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_generation_provider.h"
#import "components/password_manager/ios/password_manager_client_bridge.h"
#import "components/password_manager/ios/password_reuse_detection_manager_client_bridge.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_manager_client.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_reuse_detection_manager_client.h"
#import "ios/web/public/web_state_observer_bridge.h"

@class CommandDispatcher;
@protocol PasswordControllerDelegate;
@class SharedPasswordController;

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace safe_browsing {
class PasswordReuseDetectionManagerClient;
}  // namespace safe_browsing

// Per-tab password controller. Handles password autofill and saving.
// TODO(crbug.com/40806286): Refactor this into an appropriately-scoped object,
// such as a browser agent.
@interface PasswordController
    : NSObject <CRWWebStateObserver,
                IOSChromePasswordManagerClientBridge,
                IOSChromePasswordReuseDetectionManagerClientBridge>

// An object that can provide suggestions from this PasswordController.
@property(nonatomic, readonly) id<FormSuggestionProvider> suggestionProvider;

// An object that can provide password generation from this PasswordController.
@property(nonatomic, readonly) id<PasswordGenerationProvider>
    generationProvider;

// The PasswordManagerClient owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerClient* passwordManagerClient;

// The PasswordReuseDetectionManagerClient owned by this PasswordController.
@property(nonatomic, readonly)
    safe_browsing::PasswordReuseDetectionManagerClient*
        passwordReuseDetectionManagerClient;

// Delegate used by this PasswordController to show UI on BVC.
@property(weak, nonatomic) id<PasswordControllerDelegate> delegate;

// CommandDispatcher for dispatching commands.
@property(nonatomic) CommandDispatcher* dispatcher;

// The shared password controller that handles all non //ios/chrome specific
// business logic.
@property(nonatomic, readonly)
    SharedPasswordController* sharedPasswordController;

// `webState` should not be nil.
- (instancetype)initWithWebState:(web::WebState*)webState;

// This is just for testing.
- (instancetype)
        initWithWebState:(web::WebState*)webState
                  client:
                      (std::unique_ptr<password_manager::PasswordManagerClient>)
                          passwordManagerClient
    reuseDetectionClient:
        (std::unique_ptr<safe_browsing::PasswordReuseDetectionManagerClient>)
            passwordReuseDetectionManagerClient NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CONTROLLER_H_
