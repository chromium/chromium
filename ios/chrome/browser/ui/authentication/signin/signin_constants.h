// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "url/gurl.h"

@class ChromeIdentity;

// Sign-in result returned Sign-in result.
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult) {
  // Sign-in has been canceled by the user or by another reason.
  SigninCoordinatorResultCanceledByUser,
  // Sign-in has been done, but the user didn’t accept nor refuse to sync.
  SigninCoordinatorResultInterrupted,
  // Sign-in has been done, the user has been explicitly accepted or refused
  // sync.
  SigninCoordinatorResultSuccess,
};

// Action to do when the sign-in dialog needs to be interrupted.
typedef NS_ENUM(NSUInteger, SigninCoordinatorInterruptAction) {
  // Stops the sign-in coordinator without dismissing the view.
  SigninCoordinatorInterruptActionNoDismiss,
  // Stops the sign-in coordinator and dismisses the view without animation.
  SigninCoordinatorInterruptActionDismissWithoutAnimation,
  // Stops the sign-in coordinator and dismisses the view with animation.
  SigninCoordinatorInterruptActionDismissWithAnimation,
};

// Name of notification sent when the user has attempted a sign-in.
extern NSString* const kUserSigninAttemptedNotification;
// Name of accessibility identifier for the skip sign-in button.
extern NSString* const kSkipSigninAccessibilityIdentifier;
// Name of accessibility identifier for the add account button in the sign-in
// flow.
extern NSString* const kAddAccountAccessibilityIdentifier;
// Name of accessibility identifier for the confirmation "Yes I'm In" sign-in
// button.
extern NSString* const kConfirmationAccessibilityIdentifier;
// Name of accessibility identifier for the more button in the sign-in flow.
extern NSString* const kMoreAccessibilityIdentifier;

// Action that is required to do to complete the sign-in. This action is in
// charge of the SigninCoordinator's owner.
typedef NS_ENUM(NSUInteger, SigninCompletionAction) {
  // No action needed.
  SigninCompletionActionNone,
  // The advanced settings sign-in view is needed to finish the sign-in.
  // This case is only used for the first run sign-in.
  SigninCompletionActionShowAdvancedSettingsSignin,
  // The completion URL needs to be opened.
  SigninCompletionActionOpenCompletionURL,
};

// Embed different values related to the sign-in completion.
@interface SigninCompletionInfo : NSObject

// Returns an instance with |identity| and no completion action.
+ (instancetype)signinCompletionInfoWithIdentity:(ChromeIdentity*)identity;

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
// |identity| is the identity chosen by the user to sign-in.
// |signinCompletionAction| is the action required to complete the sign-in.
// If the |signinCompletionAction| is SigninCompletionActionOpenCompletionURL, a
// completionURL must be added.
- (instancetype)initWithIdentity:(ChromeIdentity*)identity
          signinCompletionAction:(SigninCompletionAction)signinCompletionAction
    NS_DESIGNATED_INITIALIZER;

// Identity used by the user to sign-in.
@property(nonatomic, strong, readonly) ChromeIdentity* identity;
// Action to take to finish the sign-in. This action is in charged of the
// SigninCoordinator's owner.
@property(nonatomic, assign, readonly)
    SigninCompletionAction signinCompletionAction;
// URL to be opened. Optional, only used when signinCompletionAction is
// SigninCompletionActionOpenCompletionURL.
@property(nonatomic, assign) GURL completionURL;

@end

// Called when the sign-in dialog is closed.
// |result| is the sign-in result state.
// |signinCompletionInfo| different values related to the sign-in, see
// SigninCompletionInfo class.
using SigninCoordinatorCompletionCallback =
    void (^)(SigninCoordinatorResult result, SigninCompletionInfo* info);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_
