// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_INPUT_SUGGESTIONS_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_INPUT_SUGGESTIONS_PROVIDER_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/filling_product.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#include "ios/chrome/browser/autofill/model/form_suggestion_client.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

namespace web {
struct FormActivityParams;
class WebState;
}  // namespace web

@class FormSuggestion;
@protocol FormInputNavigator;
@protocol FormInputSuggestionsProvider;

// Block type to provide form suggestions asynchronously.
typedef void (^FormSuggestionsReadyCompletion)(
    NSArray<FormSuggestion*>* suggestions,
    id<FormInputSuggestionsProvider> provider);

// Represents an object that can provide form input suggestions.
@protocol FormInputSuggestionsProvider<FormSuggestionClient>

// A delegate for form navigation.
@property(nonatomic, weak) id<FormInputNavigator> formInputNavigator;

// The type of the current suggestion provider.
@property(nonatomic, readonly) SuggestionProviderType type;

// The main type of the current suggestions.
@property(nonatomic, readonly) autofill::FillingProduct mainFillingProduct;

// Asynchronously retrieves form suggestions from this provider for the
// specified form/field and returns it via `accessoryViewUpdateBlock`. View
// will be nil if no accessories are available from this provider.
- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState
          accessoryViewUpdateBlock:
              (FormSuggestionsReadyCompletion)accessoryViewUpdateBlock;

// Notifies this provider that the accessory view is going away.
- (void)inputAccessoryViewControllerDidReset;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_INPUT_SUGGESTIONS_PROVIDER_H_
