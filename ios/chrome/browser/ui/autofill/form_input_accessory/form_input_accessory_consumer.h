// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/unique_ids.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"

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

// Type of the form suggestions.
@property(nonatomic) autofill::PopupType suggestionType;

// ID of the field that currently has focus.
@property(nonatomic) autofill::FieldRendererId currentFieldId;

// Replace the keyboard accessory view with one showing the passed suggestions.
// And form navigation buttons on iPhone (iPad already includes those).
- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_
