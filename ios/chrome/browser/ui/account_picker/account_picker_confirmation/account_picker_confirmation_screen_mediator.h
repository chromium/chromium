// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class AccountPickerConfiguration;
@class AccountPickerConfirmationScreenMediator;
@protocol AccountPickerConfirmationScreenConsumer;
class ChromeAccountManagerService;
@protocol SystemIdentity;

// Delegate for AccountPickerConfirmationScreenMediator.
@protocol AccountPickerConfirmationScreenMediatorDelegate <NSObject>

// Called when all identities are removed.
- (void)accountPickerConfirmationScreenMediatorNoIdentities:
    (AccountPickerConfirmationScreenMediator*)mediator;

@end

// Mediator for AccountPickerConfirmationScreenCoordinator.
@interface AccountPickerConfirmationScreenMediator : NSObject

// The designated initializer.
- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                configuration:
                                    (AccountPickerConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<AccountPickerConfirmationScreenMediatorDelegate>
    delegate;
@property(nonatomic, strong) id<AccountPickerConfirmationScreenConsumer>
    consumer;
// Identity presented to the user.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_H_
