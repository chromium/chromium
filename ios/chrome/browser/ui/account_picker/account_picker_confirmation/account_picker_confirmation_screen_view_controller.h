// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_consumer.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_screen/account_picker_screen_view_controller.h"

@class AccountPickerConfiguration;
@class AccountPickerConfirmationScreenViewController;
@protocol AccountPickerLayoutDelegate;

// Delegate protocol for AccountPickerConfirmationScreenViewController.
@protocol AccountPickerConfirmationScreenActionDelegate <NSObject>

// Called when the user taps on the Cancel button.
- (void)accountPickerConfirmationScreenViewControllerCancel:
    (AccountPickerConfirmationScreenViewController*)viewController;
// Called when the user taps on the identity chooser button.
- (void)accountPickerConfirmationScreenViewControllerOpenAccountList:
    (AccountPickerConfirmationScreenViewController*)viewController;
// Called when the user toggles the "Ask me every time"
// switch.
- (void)accountPickerConfirmationScreenViewController:
            (AccountPickerConfirmationScreenViewController*)viewController
                                      setAskEveryTime:(BOOL)askEveryTime;
// Called when the user taps on the submit button.
- (void)
    accountPickerConfirmationScreenViewControllerContinueWithSelectedIdentity:
        (AccountPickerConfirmationScreenViewController*)viewController;

@end

// View controller for AccountPickerConfirmationScreenCoordinator.
@interface AccountPickerConfirmationScreenViewController
    : UIViewController <AccountPickerScreenViewController,
                        AccountPickerConfirmationScreenConsumer>

- (instancetype)initWithConfiguration:
    (AccountPickerConfiguration*)configuration;

// Delegate for all the user actions.
@property(nonatomic, weak) id<AccountPickerConfirmationScreenActionDelegate>
    actionDelegate;
@property(nonatomic, weak) id<AccountPickerLayoutDelegate> layoutDelegate;

// Starts the spinner and disables buttons.
- (void)startSpinner;
// Stops the spinner and enables buttons.
- (void)stopSpinner;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_VIEW_CONTROLLER_H_
