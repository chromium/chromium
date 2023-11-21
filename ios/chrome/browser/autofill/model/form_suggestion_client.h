// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_CLIENT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_CLIENT_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

@class FormSuggestion;

// Handles user interaction with a FormSuggestion.
@protocol FormSuggestionClient<NSObject>

// Called when a suggestion is selected.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion;

// Called when a suggestion is selected. Provides the parameters required to
// fill the form, so this version of 'didSelectSuggestion' can be used without
// requiring a separate function call to provide the form activity parameters.
// If the parameters have already been provided by a previous call, then the
// 'didSelectSuggestion' overload above should be used.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                     params:(const autofill::FormActivityParams&)params;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_CLIENT_H_
