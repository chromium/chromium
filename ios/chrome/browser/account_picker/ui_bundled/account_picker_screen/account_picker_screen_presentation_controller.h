// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class AccountPickerScreenNavigationController;
@class AccountPickerScreenPresentationController;

// Delegate for handling presentation controller actions.
@protocol AccountPickerScreenPresentationControllerDelegate <NSObject>

// The background dim view was tapped.
- (void)accountPickerScreenPresentationControllerBackgroundTapped:
    (AccountPickerScreenPresentationController*)controller;

@end

// Presentation controller to present AccountPickerScreenNavigationController
// from the bottom of the screen. Related to
// AccountPickerScreenNavigationController.
@interface AccountPickerScreenPresentationController : UIPresentationController

- (instancetype)initWithAccountPickerScreenNavigationController:
                    (AccountPickerScreenNavigationController*)viewcontroller
                                       presentingViewController:
                                           (UIViewController*)
                                               presentingViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
    NS_UNAVAILABLE;

// Delegate for actions.
@property(nonatomic, weak) id<AccountPickerScreenPresentationControllerDelegate>
    actionDelegate;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_PRESENTATION_CONTROLLER_H_
