// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_COORDINATOR_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_COORDINATOR_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string_view>

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/constants.h"

@class AccountMenuMediator;
@class AuthenticationFlow;
@protocol SystemIdentity;

@protocol AccountMenuMediatorDelegate <NSObject>

// Requests to dismiss the account menu.
- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator
             withCancelationReason:
                 (signin_ui::CancelationReason)cancelationReason
                    signedIdentity:(id<SystemIdentity>)signedIdentity
                   userTappedClose:(BOOL)userTappedClose;

// Returns an authentication flow.
- (AuthenticationFlow*)authenticationFlow:(id<SystemIdentity>)identity
                               anchorRect:(CGRect)anchorRect;

// Sign out, display a toast, and call `callback` with argument stating whether
// it’s a success.
// It should only be called when the current scene is not blocked.
- (void)signOutFromTargetRect:(CGRect)targetRect
                   completion:(signin_ui::SignoutCompletionCallback)completion;

// Shows https://myaccount.google.com/ for the account currently signed-in
// to Chrome. The content is displayed in a new view in the stack, i.e.
// it doesn't close the current view.
- (void)didTapManageYourGoogleAccount;

// The user tapped on "Edit account list".
- (void)didTapManageAccounts;

// The user tapped on "Add account…".
- (void)didTapAddAccount;

// The signin is finished.
- (void)signinFinished;

// Called when the profile switching will happen. `completion` needs to be
// called once the account menu is dismissed.
- (void)profileWillSwitchWithCompletion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_COORDINATOR_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
