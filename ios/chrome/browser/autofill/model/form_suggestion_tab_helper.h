// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol FormInputSuggestionsProvider;
@protocol FormSuggestionProvider;
@class FormSuggestionController;

// Class binding a FormSuggestionController to a WebState.
class FormSuggestionTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<FormSuggestionTabHelper> {
 public:
  FormSuggestionTabHelper(const FormSuggestionTabHelper&) = delete;
  FormSuggestionTabHelper& operator=(const FormSuggestionTabHelper&) = delete;

  ~FormSuggestionTabHelper() override;

  // Returns an object that can provide an input accessory view from the
  // FormSuggestionController.
  id<FormInputSuggestionsProvider> GetAccessoryViewProvider();

 private:
  friend class web::WebStateUserData<FormSuggestionTabHelper>;

  FormSuggestionTabHelper(web::WebState* web_state,
                          NSArray<id<FormSuggestionProvider>>* providers);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The Objective-C password controller instance.
  __strong FormSuggestionController* controller_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_TAB_HELPER_H_
