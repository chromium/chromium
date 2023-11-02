// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_TAB_HELPER_H_

#include <memory>

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class AutofillAgent;
class ChromeBrowserState;
@protocol FormSuggestionProvider;
@class UIViewController;

namespace autofill {
class ChromeAutofillClientIOS;
}

namespace password_manager {
class PasswordManager;
}

// Class binding an instance of AutofillAgent to a WebState.
class AutofillTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<AutofillTabHelper> {
 public:
  AutofillTabHelper(const AutofillTabHelper&) = delete;
  AutofillTabHelper& operator=(const AutofillTabHelper&) = delete;

  ~AutofillTabHelper() override;

  // Sets a weak reference to the view controller used to present UI.
  void SetBaseViewController(UIViewController* base_view_controller);

  // Returns an object that can provide Autofill suggestions.
  id<FormSuggestionProvider> GetSuggestionProvider();

 private:
  friend class web::WebStateUserData<AutofillTabHelper>;

  AutofillTabHelper(web::WebState* web_state,
                    password_manager::PasswordManager* password_manager);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The BrowserState associated with this WebState.
  ChromeBrowserState* browser_state_;

  // The Objective-C AutofillAgent instance.
  __strong AutofillAgent* autofill_agent_;

  // The iOS AutofillClient instance.
  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_TAB_HELPER_H_
