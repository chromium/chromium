// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_view_controller.h"

@class FormSuggestion;

// Exposes private methods for unit testing.
@interface FormInputAccessoryViewController (Testing)

// Exposed for unit testing.
- (void)updateFormSuggestionView:(NSArray<FormSuggestion*>*)suggestions;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_TESTING_H_
