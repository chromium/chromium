// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"

@class AccountMenuMediator;
@protocol SystemIdentity;

@protocol AccountMenuMediatorDelegate <SyncErrorSettingsCommandHandler>

// Requests to dismiss the account menu.
- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator;

// Start managed account switch.
- (void)triggerAccountSwitchWithTargetRect:(CGRect)targetRect
                               newIdentity:(id<SystemIdentity>)newIdentity
           viewWillBeDismissedAfterSignout:(BOOL)viewWillBeDismissedAfterSignout
                          signInCompletion:(ShowSigninCommandCompletionCallback)
                                               signInCompletion;

// Sign out, display a toast, and call `callback` with argument stating whether
// it’s a success.
- (void)signOutFromTargetRect:(CGRect)targetRect
                     callback:(void (^)(BOOL))callback;

// Shows https://myaccount.google.com/ for the account currently signed-in
// to Chrome. The content is displayed in a new view in the stack, i.e.
// it doesn't close the current view.
- (void)didTapManageYourGoogleAccount;

// The user tapped on "Edit account list".
- (void)didTapEditAccountList;

// The user tapped on "Add account…".
- (void)didTapAddAccount:(ShowSigninCommandCompletionCallback)callback;

// Blocks the user from using Chromium.
- (void)blockScene;

// Stops the `blockScene`.
- (void)unblockScene;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
