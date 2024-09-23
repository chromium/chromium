// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_table_view_controller_model_delegate.h"

@protocol AccountPickerSelectionScreenConsumer;
@class AccountPickerSelectionScreenMediator;
class ChromeAccountManagerService;
@protocol SystemIdentity;

// Mediator for AccountPickerSelectionScreenCoordinator.
@interface AccountPickerSelectionScreenMediator
    : NSObject <AccountPickerSelectionScreenTableViewControllerModelDelegate>

@property(nonatomic, strong) id<AccountPickerSelectionScreenConsumer> consumer;
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// See -[SigninPromoViewMediator initWithBrowserState:].
- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity
                   accountManagerService:
                       (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_MEDIATOR_H_
