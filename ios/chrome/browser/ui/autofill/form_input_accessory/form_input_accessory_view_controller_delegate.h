// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class FormInputAccessoryViewController;

// Protocol to handle user interactions in a FormInputAccessoryViewController.
@protocol FormInputAccessoryViewControllerDelegate

// Invoked after the user taps the `accounts` button.
- (void)formInputAccessoryViewControllerAccountButtonPressed:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                                                      sender:(UIButton*)sender;

// Invoked after the user taps the `credit cards` button.
- (void)formInputAccessoryViewControllerCardButtonPressed:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                                                   sender:(UIButton*)sender;

// Invoked after the user taps the `keyboard` button.
- (void)formInputAccessoryViewControllerKeyboardButtonPressed:
    (FormInputAccessoryViewController*)formInputAccessoryViewController;

// Invoked after the user taps the `passwords` button.
- (void)formInputAccessoryViewControllerPasswordButtonPressed:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                                                       sender:(UIButton*)sender;

// Resets the delegate.
- (void)formInputAccessoryViewControllerReset:
    (FormInputAccessoryViewController*)formInputAccessoryViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
