// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SIGNIN_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHOW_SIGNIN_COMMAND_H_

#import <Foundation/Foundation.h>

#include "components/signin/public/base/signin_metrics.h"

@class SigninCompletionInfo;
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult);
@protocol SystemIdentity;

using ShowSigninCommandCompletionCallback =
    void (^)(SigninCoordinatorResult result, SigninCompletionInfo*);

enum class AuthenticationOperation {
  // Operation to start a re-authenticate operation. The user is presented with
  // the SSOAuth re-authenticate dialog. This command can only be used if there
  // is a primary account. Please note that the primary account can disappear
  // (for external reasons) when the reauth is in progress.
  kPrimaryAccountReauth,
  // Operation to sign-in again with the previously signed-in account. The user
  // is presented with the SSOAuth dialog. This command can only be used if
  // there is no primary account.
  kResignin,
  // Operation to start a sign-in only operation. The user is presented with
  // the consistency web sign-in dialog.
  kSigninOnly,
  // Operation to add a secondary account. The user is presented with the
  // SSOAUth sign-in page. This command can only be used if there is a primary
  // account.
  kAddAccount,
  // Operation to start a forced sign-in operation. The user is presented with
  // the sign-in page with information about the policy and cannot dimiss it.
  kForcedSigninAndSync,
  // Operation to trigger sign-in only operation, without presenting UI if an
  // identity is selected in `-ShowSigninCommand.identity`. Otherwise,
  // a dialog to choose an identity is presented and the user is signed in as
  // soon as the identity is selected.
  kInstantSignin,
  // Operation to trigger sign-in and then history sync.
  // If there is at least one identity on the device, the user is presented with
  // the sign-in bottom sheet to sign-in.
  // If there is no identity on the device, the user is presented the SSO add
  // account dialog to sign-in.
  // Once signed in, the history sync opt-in is displayed.
  kSheetSigninAndHistorySync,
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

// If YES, the sign-in command will not be presented and ignored if there is
// any dialog already presented on the NTP.
// Default value: NO.
@property(nonatomic, assign) BOOL skipIfUINotAvaible;

// The callback to be invoked after the operation is complete.
@property(nonatomic, copy, readonly)
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
