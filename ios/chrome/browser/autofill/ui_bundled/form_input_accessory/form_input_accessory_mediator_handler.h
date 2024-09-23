// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_HANDLER_H_

#import <UIKit/UIKit.h>

namespace manual_fill {
enum class ManualFillDataType;
}

enum class SuggestionFeatureForIPH;

// Handler in charge of accessory mediator events.
@protocol FormInputAccessoryMediatorHandler <NSObject>

// The mediator detected that the keyboard input view should be reset.
- (void)resetFormInputView;

// The mediator shows autofill suggestion tip if needed.
- (void)showAutofillSuggestionIPHIfNeededFor:
    (SuggestionFeatureForIPH)featureForIPH;

// The mediator notifies that the autofill suggestion has been selected.
- (void)notifyAutofillSuggestionWithIPHSelectedFor:
    (SuggestionFeatureForIPH)featureForIPH;

// Invoked if manual fill for the `dataType` should be started immediately.
- (void)startManualFillForDataType:(manual_fill::ManualFillDataType)dataType;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_MEDIATOR_HANDLER_H_
