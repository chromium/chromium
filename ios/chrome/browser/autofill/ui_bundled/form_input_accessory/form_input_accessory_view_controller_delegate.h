// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class FormInputAccessoryViewController;

namespace manual_fill {
enum class ManualFillDataType;
}

// Protocol to handle user interactions in a FormInputAccessoryViewController.
@protocol FormInputAccessoryViewControllerDelegate

// Invoked after the user taps the `accounts` button.
- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                   didPressAccountButton:(UIButton*)accountButton;

// Invoked after the user taps the `credit cards` button.
- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                didPressCreditCardButton:(UIButton*)creditCardButton;

// Invoked after the user taps the `keyboard` button.
- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                  didPressKeyboardButton:(UIButton*)keyboardButton;

// Invoked after the user taps the `passwords` button.
- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                  didPressPasswordButton:(UIButton*)passwordButton;

// Invoked after the user taps the `manual fill` button.
- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                didPressManualFillButton:(UIButton*)manualFillButton
                             forDataType:
                                 (manual_fill::ManualFillDataType)dataType;

// Invoked after the user taps the form input accessory view.
- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
            didTapFormInputAccessoryView:(UIView*)formInputAccessoryView;

// Resets the delegate.
- (void)formInputAccessoryViewControllerReset:
    (FormInputAccessoryViewController*)formInputAccessoryViewController;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
