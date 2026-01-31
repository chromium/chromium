// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_MEDIATOR_TESTING_H_

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/form_input_accessory_mediator.h"

namespace web {
class WebState;
}

// Testing category to expose private methods.
@interface FormInputAccessoryMediator (Testing)

// This method is mocked to verify the triggering of the suggestion refresh
// logic.
- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_MEDIATOR_TESTING_H_
