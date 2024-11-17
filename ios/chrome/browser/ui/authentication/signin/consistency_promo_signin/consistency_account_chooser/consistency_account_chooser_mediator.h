// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller_model_delegate.h"

class ChromeAccountManagerService;
@protocol ConsistencyAccountChooserConsumer;
@class ConsistencyAccountChooserMediator;
@protocol SystemIdentity;

// Mediator for ConsistencyAccountChooserCoordinator.
@interface ConsistencyAccountChooserMediator
    : NSObject <ConsistencyAccountChooserTableViewControllerModelDelegate>

@property(nonatomic, strong) id<ConsistencyAccountChooserConsumer> consumer;
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// See -[SigninPromoViewMediator initWithProfile:].
- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity
                   accountManagerService:
                       (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_ACCOUNT_CHOOSER_CONSISTENCY_ACCOUNT_CHOOSER_MEDIATOR_H_
