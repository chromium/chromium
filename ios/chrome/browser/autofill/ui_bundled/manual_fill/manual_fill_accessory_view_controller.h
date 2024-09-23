// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace manual_fill {

// Accessibility identifier of the keyboard button.
extern NSString* const AccessoryKeyboardAccessibilityIdentifier;
// Accessibility identifier of the password button.
extern NSString* const AccessoryPasswordAccessibilityIdentifier;
// Accessibility identifier of the address button.
extern NSString* const AccessoryAddressAccessibilityIdentifier;
// Accessibility identifier of the credit card button.
extern NSString* const AccessoryCreditCardAccessibilityIdentifier;

}  // namespace manual_fill

@protocol ManualFillAccessoryViewControllerDelegate;

// This view contains the icons to activate "Manual Fill". It is meant to be
// shown above the keyboard on iPhone and above the manual fill view.
@interface ManualFillAccessoryViewController : UIViewController

// Changing this property hides and shows the address button.
@property(nonatomic, assign, getter=isAddressButtonHidden)
    BOOL addressButtonHidden;

// Changing this property hides and shows the credit card button.
@property(nonatomic, assign, getter=isCreditCardButtonHidden)
    BOOL creditCardButtonHidden;

// Changing this property hides and shows the password button.
@property(nonatomic, assign, getter=isPasswordButtonHidden)
    BOOL passwordButtonHidden;

// Readonly property that returns if all manual fill buttons are hidden.
@property(nonatomic, readonly) BOOL allButtonsHidden;

// Instances an object with the desired delegate.
//
// @param delegate The delegate for this object.
// @return A fresh object with the passed delegate.
- (instancetype)initWithDelegate:
    (id<ManualFillAccessoryViewControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Resets to the original state, with the keyboard icon hidden and no icon
// selected.
- (void)resetAnimated:(BOOL)animated;

// Invoked after the user taps the `accounts` button.
- (void)accountButtonPressed:(UIButton*)accountButton;

// Invoked after the user taps the `credit cards` button.
- (void)cardButtonPressed:(UIButton*)creditCardButton;

// Invoked after the user taps the `passwords` button.
- (void)passwordButtonPressed:(UIButton*)passwordButton;

// Set the hidden property of this view controller's view.
- (void)setViewHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_H_
