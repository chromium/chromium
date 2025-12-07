// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_controller.h"

FormSuggestionTabHelper::~FormSuggestionTabHelper() = default;

id<FormInputSuggestionsProvider>
FormSuggestionTabHelper::GetAccessoryViewProvider() {
  return controller_;
}

FormSuggestionTabHelper::FormSuggestionTabHelper(
    web::WebState* web_state,
    NSArray<id<FormSuggestionProvider>>* providers)
    : controller_([[FormSuggestionController alloc]
          initWithWebState:web_state
                 providers:providers]) {
  CHECK(web_state->IsRealized());
  web_state_observation_.Observe(web_state);
}

void FormSuggestionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
  [controller_ detachFromWebState];
  controller_ = nil;
}
