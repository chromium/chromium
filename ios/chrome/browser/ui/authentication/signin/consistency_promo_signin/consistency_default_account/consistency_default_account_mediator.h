// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_MEDIATOR_H_

#import <Foundation/Foundation.h>

class ChromeAccountManagerService;
@class ChromeIdentity;
@class ConsistencyDefaultAccountMediator;
@protocol ConsistencyDefaultAccountConsumer;

// Delegate for ConsistencyDefaultAccountMediator.
@protocol ConsistencyDefaultAccountMediatorDelegate <NSObject>

// Called when all identities are removed.
- (void)consistencyDefaultAccountMediatorNoIdentities:
    (ConsistencyDefaultAccountMediator*)mediator;

@end

// Mediator for ConsistencyDefaultAccountCoordinator.
@interface ConsistencyDefaultAccountMediator : NSObject

// The designated initializer.
- (instancetype)initWithAccountManagerService:
    (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<ConsistencyDefaultAccountMediatorDelegate>
    delegate;
@property(nonatomic, strong) id<ConsistencyDefaultAccountConsumer> consumer;
// Identity presented to the user.
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_MEDIATOR_H_
