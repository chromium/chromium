// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"

@class FormSuggestion;
@protocol FormInputAccessoryViewDelegate;
@protocol FormSuggestionClient;

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

// Removes the animations on the custom keyboard view.
- (void)removeAnimationsOnKeyboardView;

// Removes the presented keyboard view and the input accessory view.
- (void)restoreOriginalKeyboardView;

// Removes the presented keyboard view and the input accessory view until
// |continueCustomKeyboardView| is called.
- (void)pauseCustomKeyboardView;

// Adds the previously presented views to the keyboard. If they have not been
// reset.
- (void)continueCustomKeyboardView;

// Tells the consumer that suggestions are being fetched. The fetching is
// asynchronous, so this call gives the opportunity to do any view preparation
// that doesn't need the suggestions.
- (void)prepareToShowSuggestions;

// Replace the keyboard accessory view with one showing the passed suggestions.
// And form navigation buttons if not an iPad (which already includes those).
- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions
                suggestionClient:(id<FormSuggestionClient>)suggestionClient;

// Indicates that the keyboard state changed.
- (void)keyboardWillChangeToState:(KeyboardState)keyboardState;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_CONSUMER_H_
