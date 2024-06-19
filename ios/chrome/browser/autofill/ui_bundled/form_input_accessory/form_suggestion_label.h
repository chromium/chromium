// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_SUGGESTION_LABEL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_SUGGESTION_LABEL_H_

// TODO(crbug.com/40144558): Move Autofill ui code to i/c/b/ui/autofill.

#import <UIKit/UIKit.h>

@class FormSuggestion;
@class FormSuggestionLabel;

// Delegate for actions happening in FormSuggestionLabel.
@protocol FormSuggestionLabelDelegate

// User tapped on the suggestion.
- (void)didTapFormSuggestionLabel:(FormSuggestionLabel*)formSuggestionLabel;

@end

// Class for Autofill suggestion in the customized keyboard.
@interface FormSuggestionLabel : UIView

// Designated initializer. Initializes with `delegate` for `suggestion`.
- (instancetype)initWithSuggestion:(FormSuggestion*)suggestion
                             index:(NSUInteger)index
                    numSuggestions:(NSUInteger)numSuggestions
             accessoryTrailingView:(UIView*)accessoryTrailingView
                          delegate:(id<FormSuggestionLabelDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_SUGGESTION_LABEL_H_
