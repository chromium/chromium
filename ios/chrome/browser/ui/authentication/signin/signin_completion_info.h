// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COMPLETION_INFO_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COMPLETION_INFO_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@protocol SystemIdentity;

// Embed different values related to the sign-in completion.
@interface SigninCompletionInfo : NSObject

// Returns an instance with `identity` and no completion action.
+ (instancetype)signinCompletionInfoWithIdentity:(id<SystemIdentity>)identity;

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
// `identity` is the identity chosen by the user to sign-in.
// `signinCompletionAction` is the action required to complete the sign-in.
- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
          signinCompletionAction:(SigninCompletionAction)signinCompletionAction
    NS_DESIGNATED_INITIALIZER;

// Identity used by the user to sign-in.
@property(nonatomic, strong, readonly) id<SystemIdentity> identity;
// Action to take to finish the sign-in. This action is in charged of the
// SigninCoordinator's owner.
@property(nonatomic, assign, readonly)
    SigninCompletionAction signinCompletionAction;

@end

// Called when the sign-in dialog is closed.
// `result` is the sign-in result state.
// `signinCompletionInfo` different values related to the sign-in, see
// SigninCompletionInfo class.
using SigninCoordinatorCompletionCallback =
    void (^)(SigninCoordinatorResult result, SigninCompletionInfo* info);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COMPLETION_INFO_H_
