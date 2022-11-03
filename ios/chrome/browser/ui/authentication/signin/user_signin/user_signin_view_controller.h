// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

// Delegate that interacts with the user sign-in coordinator.
@protocol UserSigninViewControllerDelegate

// Returns whether the user has selected an identity from the unified consent
// screen.
- (BOOL)unifiedConsentCoordinatorHasIdentity;

// Performs add account operation.
- (void)userSigninViewControllerDidTapOnAddAccount;

// Performs scroll operation on unified consent screen.
- (void)userSigninViewControllerDidScrollOnUnifiedConsent;

// Performs operations to skip sign-in or undo existing sign-in.
- (void)userSigninViewControllerDidTapOnSkipSignin;

// Performs operations to skip sign-in or undo existing sign-in.
- (void)userSigninViewControllerDidTapOnSignin;

@end

// View controller used to show sign-in UI.
@interface UserSigninViewController
    : UIViewController <AuthenticationFlowDelegate>

// The delegate.
@property(nonatomic, weak) id<UserSigninViewControllerDelegate> delegate;

@property(nonatomic, assign, readonly) int acceptSigninButtonStringId;

// See `initWithEmbeddedViewController:`.
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

// Initializer with the UnifiedConsentViewController that is embedded in the
// UserSigninViewController.
- (instancetype)initWithEmbeddedViewController:
    (UIViewController*)embeddedViewController NS_DESIGNATED_INITIALIZER;

// Informs the view controller that the unified consent has reached the bottom
// of the screen.
- (void)markUnifiedConsentScreenReachedBottom;

// Sets the title, styling, and other button properties for the confirmation
// button based on the user consent text that is currently displayed on-screen
// and the whether the user has previously been signed-in.
- (void)updatePrimaryActionButtonStyle;

// Returns the supported orientations for the device type:
// `UIInterfaceOrientationPortrait` orientation on iPhone and all other
// orientations on iPad.
- (NSUInteger)supportedInterfaceOrientations;

// Blocks the UI (except the cancel button) when the sign-in is in progress.
- (void)signinWillStart;

// Unblocks the UI when the sign-in is done.
- (void)signinDidStop;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_
