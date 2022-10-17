// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
@protocol LegacySigninScreenConsumer;
@protocol SystemIdentity;

// Mediator that handles the sign-in operation.
@interface LegacySigninScreenMediator : NSObject

// The designated initializer.
- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                        authenticationService:
                            (AuthenticationService*)authenticationService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for this mediator.
@property(nonatomic, weak) id<LegacySigninScreenConsumer> consumer;

// The identity currently selected.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// Whether an account has been added. Must be set externally.
@property(nonatomic, assign) BOOL addedAccount;

// Disconnect the mediator.
- (void)disconnect;

// Sign in the selected account.
- (void)startSignInWithAuthenticationFlow:
            (AuthenticationFlow*)authenticationFlow
                               completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_MEDIATOR_H_
