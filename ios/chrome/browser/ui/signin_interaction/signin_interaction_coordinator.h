// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
@class ChromeIdentity;

// The coordinator for Sign In Interaction. This coordinator handles the
// presentation and dismissal of the UI. This object should not be destroyed
// while |active| is true, or UI dismissal or completion callbacks may not
// execute. It is safe to destroy in the |completion| block.
@interface SigninInteractionCoordinator : ChromeCoordinator

// Indicates whether this coordinator is currently presenting UI.
@property(nonatomic, assign, readonly, getter=isActive) BOOL active;

// Returns YES if the Google services settings view is presented.
@property(nonatomic, assign, readonly, getter=isSettingsViewPresented)
    BOOL settingsViewPresented;

// Designated initializer.
// * |browserState| is the current browser state. Must not be nil.
// * |dispatcher| is the dispatcher to be sent commands from this class.
- (instancetype)initWithBrowser:(Browser*)browser
                     dispatcher:
                         (id<ApplicationCommands, BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// Creates a coordinator that uses |viewController| and |browser|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Starts user sign-in. If a sign in operation is already in progress, this
// method does nothing.
// * |identity|, if not nil, the user will be signed in without requiring user
//   input, using this Chrome identity.
// * |accessPoint| represents the access point that initiated the sign-in.
// * |promoAction| is the action taken on a Signin Promo.
// * |presentingViewController| is the top presented view controller.
// * |completion| will be called when the operation is done, and
//   |succeeded| will notify the caller on whether the user is now signed in.
- (void)signInWithIdentity:(ChromeIdentity*)identity
                 accessPoint:(signin_metrics::AccessPoint)accessPoint
                 promoAction:(signin_metrics::PromoAction)promoAction
    presentingViewController:(UIViewController*)viewController
                  completion:(signin_ui::CompletionCallback)completion;

// Re-authenticate the user. This method will always show a sign-in web flow.
// If a sign in operation is already in progress, this method does nothing.
// * |accessPoint| represents the access point that initiated the sign-in.
// * |promoAction| is the action taken on a Signin Promo.
// * |presentingViewController| is the top presented view controller.
// * |completion| will be called when the operation is done, and
// |succeeded| will notify the caller on whether the user has been
// correctly re-authenticated.
- (void)reAuthenticateWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
             presentingViewController:(UIViewController*)viewController
                           completion:(signin_ui::CompletionCallback)completion;

// Starts the flow to add an identity via ChromeIdentityInteractionManager.
// If a sign in operation is already in progress, this method does nothing.
// * |accessPoint| represents the access point that initiated the sign-in.
// * |promoAction| is the action taken on a Signin Promo.
// * |presentingViewController| is the top presented view controller.
// * |completion| will be called when the operation is done, and
// |succeeded| will notify the caller on whether the user has been
// correctly re-authenticated.
- (void)addAccountWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
         presentingViewController:(UIViewController*)viewController
                       completion:(signin_ui::CompletionCallback)completion;

// Presents the advanced sign-in settings screen.
// * |presentingViewController| is the top presented view controller.
- (void)showAdvancedSigninSettingsWithPresentingViewController:
    (UIViewController*)viewController;

// Cancels any current process. Calls the completion callback when done.
// |abortAndDismissSettingsViewAnimated:completion:| should not be called after
// this call.
// TODO(crbug.com/965481): This method should be merged with:
//   * |cancelAndDismiss|
//   * |abortAndDismissSettingsViewAnimated:completion:|.
- (void)cancel;

// Cancels any current process and dismisses any UI (including the settings
// view). Calls the completion callback when done.
// |abortAndDismissSettingsViewAnimated:completion:| should not be called after
// this call.
// TODO(crbug.com/965481): This method should be merged with:
//   * |cancel|
//   * |abortAndDismissSettingsViewAnimated:completion:|.
- (void)cancelAndDismiss;

// Aborts and dismisses the settings view. This methods can only be called when
// -SigninInteractionCoordinator.isSettingsViewPresented is YES.
// TODO(crbug.com/965481): This method should be merged with:
//   * |cancel|
//   * |cancelAndDismiss|
- (void)abortAndDismissSettingsViewAnimated:(BOOL)animated
                                 completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_COORDINATOR_H_
