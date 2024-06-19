// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/common/unique_ids.h"

@class FormSuggestion;
@protocol FormInputAccessoryViewDelegate;

@protocol FormInputAccessoryConsumer <NSObject>

// Delegate used for form navigation.
@property(nonatomic, weak) id<FormInputAccessoryViewDelegate>
    navigationDelegate;

// Hides or shows the manual fill password button.
@property(nonatomic) BOOL passwordButtonHidden;

// Hides or shows the manual fill credit card button.
@property(nonatomic) BOOL creditCardButtonHidden;

// Hides or shows the manual fill address button.
@property(nonatomic) BOOL addressButtonHidden;

// Enables or disables the next button if any.
@property(nonatomic) BOOL formInputNextButtonEnabled;

// Enables or disables the previous button if any.
@property(nonatomic) BOOL formInputPreviousButtonEnabled;

// Main type of the form suggestions.
@property(nonatomic) autofill::FillingProduct mainFillingProduct;

// ID of the field that currently has focus.
@property(nonatomic) autofill::FieldRendererId currentFieldId;

// Replace the keyboard accessory view with one showing the passed suggestions.
// And form navigation buttons on iPhone (iPad already includes those).
- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions;

// Invoked after the user taps the "manual fill" button.
- (void)manualFillButtonPressed:(UIButton*)button;

// Invoked after the user taps the "password manual fill" button.
- (void)passwordManualFillButtonPressed:(UIButton*)button;

// Invoked after the user taps the "credit card manual fill" button.
- (void)creditCardManualFillButtonPressed:(UIButton*)button;

// Invoked after the user taps the "address manual fill" button.
- (void)addressManualFillButtonPressed:(UIButton*)button;

// Preferred omnibox position was updated. "isBottomOmnibox": whether the new
// position is bottom omnibox.
- (void)newOmniboxPositionIsBottom:(BOOL)isBottomOmnibox;

// Invoked after a height-only change happened to the keyboard's frame.
- (void)keyboardHeightChanged:(CGFloat)newHeight oldHeight:(CGFloat)oldHeight;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_
