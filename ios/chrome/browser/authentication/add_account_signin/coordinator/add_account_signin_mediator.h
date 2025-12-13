// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ADD_ACCOUNT_SIGNIN_COORDINATOR_ADD_ACCOUNT_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ADD_ACCOUNT_SIGNIN_COORDINATOR_ADD_ACCOUNT_SIGNIN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol AddAccountSigninMediatorDelegate;
class AuthenticationService;

@interface AddAccountSigninMediator : NSObject

@property(nonatomic, weak) id<AddAccountSigninMediatorDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithAuthenticationService:(AuthenticationService*)service
    NS_DESIGNATED_INITIALIZER;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ADD_ACCOUNT_SIGNIN_COORDINATOR_ADD_ACCOUNT_SIGNIN_MEDIATOR_H_
