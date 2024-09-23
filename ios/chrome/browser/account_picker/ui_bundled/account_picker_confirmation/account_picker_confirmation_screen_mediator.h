// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class AccountPickerConfiguration;
@class AccountPickerConfirmationScreenMediator;
@protocol AccountPickerConfirmationScreenConsumer;
class ChromeAccountManagerService;
@protocol SystemIdentity;

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for AccountPickerConfirmationScreenCoordinator.
@interface AccountPickerConfirmationScreenMediator : NSObject

// The designated initializer.
- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                    configuration:(AccountPickerConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, strong) id<AccountPickerConfirmationScreenConsumer>
    consumer;
// Identity presented to the user.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_MEDIATOR_H_
