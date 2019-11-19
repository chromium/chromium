// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_

#include <string>

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer_delegate.h"

class Browser;
@protocol BrowsingDataCommands;
@class ChromeIdentity;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Performs the sign-in steps and user interactions as part of the sign-in flow.
@interface AuthenticationFlowPerformer : NSObject

// Initializes a new AuthenticationFlowPerformer. |delegate| will be notified
// when each step completes.
- (instancetype)initWithDelegate:
    (id<AuthenticationFlowPerformerDelegate>)delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cancels any outstanding work and dismisses an alert view (if shown).
- (void)cancelAndDismiss;

// Starts sync for |browserState|.
- (void)commitSyncForBrowserState:(ios::ChromeBrowserState*)browserState;

// Fetches the managed status for |identity|.
- (void)fetchManagedStatus:(ios::ChromeBrowserState*)browserState
               forIdentity:(ChromeIdentity*)identity;

// Signs |identity| with |hostedDomain| into |browserState|.
- (void)signInIdentity:(ChromeIdentity*)identity
      withHostedDomain:(NSString*)hostedDomain
        toBrowserState:(ios::ChromeBrowserState*)browserState;

// Signs out of |browserState| and sends |didSignOut| to the delegate when
// complete.
- (void)signOutBrowserState:(ios::ChromeBrowserState*)browserState;

// Immediately signs out |browserState| without waiting for dependent services.
- (void)signOutImmediatelyFromBrowserState:
    (ios::ChromeBrowserState*)browserState;

// Asks the user whether to clear or merge their previous identity's data with
// that of |identity| or cancel sign-in, sending |didChooseClearDataPolicy:|
// or |didChooseCancel| to the delegate when complete according to the user
// action.
- (void)promptMergeCaseForIdentity:(ChromeIdentity*)identity
                           browser:(Browser*)browser
                    viewController:(UIViewController*)viewController;

// Clears browsing data from the bowser state assoiciated with |browser|, using
// |handler| to perform the removal. When removal is comeplete, the delegate is
// informed (via -didClearData).
- (void)clearDataFromBrowser:(Browser*)browser
              commandHandler:(id<BrowsingDataCommands>)handler;

// Determines whether the user must decide what to do with |identity|'s browsing
// data before signing into |browserState|.
- (BOOL)shouldHandleMergeCaseForIdentity:(ChromeIdentity*)identity
                            browserState:(ios::ChromeBrowserState*)browserState;

// Shows a confirmation dialog for signing in to an account managed by
// |hostedDomain|.
- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:
                                    (UIViewController*)viewController;

// Shows |error| to the user and calls |callback| on dismiss.
- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController;

@property(nonatomic, weak, readonly) id<AuthenticationFlowPerformerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_
