// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class AccountMenuViewController;

// Presentation delegate for the account menu.
@protocol AccountMenuViewControllerPresentationDelegate <NSObject>

// The user tapped the close button.
- (void)viewControllerWantsToBeClosed:
    (AccountMenuViewController*)viewController;

// Shows https://myaccount.google.com/ for the account currently signed-in
// to Chrome. The content is displayed in a new view in the stack, i.e.
// it doesn't close the current view.
- (void)didTapManageYourGoogleAccount;

// The user tapped on "Edit account list".
- (void)didTapEditAccountList;

// Sign out, display a toast, and call `callback` with argument stating whether
// it’s a success.
- (void)signOutFromTargetRect:(CGRect)targetRect
                     callback:(void (^)(BOOL))callback;

// The user tapped on "Add account…".
- (void)didTapAddAccount;

@end
#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
