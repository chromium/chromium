// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SIGNIN_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SIGNIN_COMMAND_H_

#import <Foundation/Foundation.h>

#include "components/signin/public/base/signin_metrics.h"

@protocol SystemIdentity;

typedef void (^ShowSigninCommandCompletionCallback)(BOOL succeeded);

typedef NS_ENUM(NSInteger, AuthenticationOperation) {
  // Operation to start a re-authenticate operation. The user is presented with
  // the SSOAuth re-authenticate web page.
  AuthenticationOperationReauthenticate,
  // Operation to start a sign-in and sync operation. The user is presented with
  // the sign-in page with the user consent.
  AuthenticationOperationSigninAndSync,
  // Operation to start a sign-in only operation. The user is presented with
  // the consistency web sign-in dialog.
  AuthenticationOperationSigninOnly,
  // Operation to add a secondary account. The user is presented with the
  // SSOAUth sin-in page.
  AuthenticationOperationAddAccount,
  // Operation to start a forced sign-in operation. The user is presented with
  // the sign-in page with information about the policy and cannot dimiss it.
  AuthenticationOperationForcedSigninAndSync,
  // Operation to start a sign-in and sync operation. The user is presented with
  // the sign-in page with the user consent. The views are the newer FRE style
  // views with the first being a screen that asks the user if they want to
  // sign in and the second being the "tangible sync" screen.
  AuthenticationOperationSigninAndSyncWithTwoScreens,
};

// A command to perform a sign in operation.
@interface ShowSigninCommand : NSObject

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)init NS_UNAVAILABLE;

// Initializes a command to perform the specified operation with a
// SigninInteractionController and invoke a possibly-nil callback when finished.
- (instancetype)initWithOperation:(AuthenticationOperation)operation
                         identity:(id<SystemIdentity>)identity
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
                         callback:(ShowSigninCommandCompletionCallback)callback
    NS_DESIGNATED_INITIALIZER;

// Initializes a ShowSigninCommand with `identity` and `callback` set to nil.
- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction;

// Initializes a ShowSigninCommand with PROMO_ACTION_NO_SIGNIN_PROMO and a nil
// callback.
- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint;

// The callback to be invoked after the operation is complete.
@property(copy, nonatomic, readonly)
    ShowSigninCommandCompletionCallback callback;

// The operation to perform during the sign-in flow.
@property(nonatomic, readonly) AuthenticationOperation operation;

// Chrome identity is only used for the AuthenticationOperationSigninAndSync
// operation (should be nil otherwise). If the identity is non-nil, the
// interaction view controller logins using this identity. If the identity is
// nil, the interaction view controller asks the user to choose an identity or
// to add a new one.
@property(nonatomic, readonly) id<SystemIdentity> identity;

// The access point of this authentication operation.
@property(nonatomic, readonly) signin_metrics::AccessPoint accessPoint;

// The user action from the sign-in promo to trigger the sign-in operation.
@property(nonatomic, readonly) signin_metrics::PromoAction promoAction;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SIGNIN_COMMAND_H_
