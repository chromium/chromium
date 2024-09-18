// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MUTATOR_H_

#import <Foundation/Foundation.h>

@class AccountMenuViewController;

// Mutator for account menu.
@protocol AccountMenuMutator <NSObject>

// The user requested to close the view controller.
- (void)viewControllerWantsToBeClosed:
    (AccountMenuViewController*)viewController;

// Sign out, display a toast.
- (void)signOutFromTargetRect:(CGRect)targetRect;

// The user tapped on the `index`-th account.
- (void)accountTappedWithGaiaID:(NSString*)index targetRect:(CGRect)targetRect;

// The user tapped on the error button.
- (void)didTapErrorButton;

// Shows https://myaccount.google.com/ for the account currently signed-in
// to Chrome. The content is displayed in a new view in the stack, i.e.
// it doesn't close the current view.
- (void)didTapManageYourGoogleAccount;

// The user tapped on "Edit account list".
- (void)didTapEditAccountList;

// The user tapped on "Add account…".
- (void)didTapAddAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MUTATOR_H_
