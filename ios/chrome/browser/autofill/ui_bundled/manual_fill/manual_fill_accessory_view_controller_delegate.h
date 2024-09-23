// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class ManualFillAccessoryViewController;

// Protocol to handle user interactions in a ManualFillAccessoryViewController.
@protocol ManualFillAccessoryViewControllerDelegate

// Invoked after the user taps the `accounts` button.
- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                    didPressAccountButton:(UIButton*)accountButton;

// Invoked after the user taps the `credit cards` button.
- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                 didPressCreditCardButton:(UIButton*)creditCardButton;

// Invoked after the user taps the `keyboard` button.
- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                   didPressKeyboardButton:(UIButton*)keyboardButton;

// Invoked after the user taps the `passwords` button.
- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                   didPressPasswordButton:(UIButton*)passwordButton;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
