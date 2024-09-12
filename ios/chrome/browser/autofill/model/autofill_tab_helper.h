// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_TAB_HELPER_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class AutofillAgent;
@protocol AutofillCommands;
@protocol FormSuggestionProvider;
@class UIViewController;

namespace autofill {
class ChromeAutofillClientIOS;
}

// Class binding an instance of AutofillAgent to a WebState.
class AutofillTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<AutofillTabHelper>,
                          public autofill::ChildFrameRegistrarObserver {
 public:
  AutofillTabHelper(const AutofillTabHelper&) = delete;
  AutofillTabHelper& operator=(const AutofillTabHelper&) = delete;

  ~AutofillTabHelper() override;

  // Sets a weak reference to the view controller used to present UI.
  void SetBaseViewController(UIViewController* base_view_controller);

  void SetCommandsHandler(id<AutofillCommands> commands_handler);

  // Returns an object that can provide Autofill suggestions.
  id<FormSuggestionProvider> GetSuggestionProvider();

  autofill::ChromeAutofillClientIOS* autofill_client() {
    return autofill_client_.get();
  }

 private:
  friend class web::WebStateUserData<AutofillTabHelper>;

  explicit AutofillTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // autofill::ChildFrameRegistrarObserver implementation.
  void OnDidDoubleRegistration(autofill::LocalFrameToken local) override;

  // The BrowserState associated with this WebState.
  raw_ptr<ProfileIOS> profile_;

  // The Objective-C AutofillAgent instance.
  __strong AutofillAgent* autofill_agent_;

  // The iOS AutofillClient instance.
  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;

  // The WebState holding this instance of the helper.
  raw_ptr<web::WebState> web_state_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_TAB_HELPER_H_
