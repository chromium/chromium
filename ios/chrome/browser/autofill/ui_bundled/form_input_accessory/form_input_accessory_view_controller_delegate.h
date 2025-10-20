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

// Invoked after the user taps a button that opens the manual fallback menu.
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
