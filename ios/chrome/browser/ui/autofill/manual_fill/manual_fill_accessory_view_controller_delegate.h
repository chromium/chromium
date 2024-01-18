// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class ManualFillAccessoryViewController;

// Protocol to handle user interactions in a ManualFillAccessoryViewController.
@protocol ManualFillAccessoryViewControllerDelegate

// Invoked after the user taps the `accounts` button.
- (void)manualFillAccessoryViewControllerAccountButtonPressed:
            (ManualFillAccessoryViewController*)
                manualFillAccessoryViewController
                                                       sender:(UIButton*)sender;

// Invoked after the user taps the `credit cards` button.
- (void)manualFillAccessoryViewControllerCardButtonPressed:
            (ManualFillAccessoryViewController*)
                manualFillAccessoryViewController
                                                    sender:(UIButton*)sender;

// Invoked after the user taps the `keyboard` button.
- (void)manualFillAccessoryViewControllerKeyboardButtonPressed:
    (ManualFillAccessoryViewController*)manualFillAccessoryViewController;

// Invoked after the user taps the `passwords` button.
- (void)
    manualFillAccessoryViewControllerPasswordButtonPressed:
        (ManualFillAccessoryViewController*)manualFillAccessoryViewController
                                                    sender:(UIButton*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_DELEGATE_H_
