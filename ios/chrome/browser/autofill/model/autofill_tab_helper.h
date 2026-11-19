// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_TAB_HELPER_H_

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class AutofillAgent;
@class AutofillAgentDelegate;
@protocol AutofillCommands;
@protocol FormInputAccessoryCommands;
@protocol FormSuggestionProvider;
@protocol SnackbarCommands;
@class UIViewController;

namespace autofill {
class AutofillClientIOS;
class ChromeAutofillClientIOS;
class FormActivityTabHelper;
enum class SuggestionHidingReason;
}

// Class binding an instance of AutofillAgent to a WebState.
class AutofillTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<AutofillTabHelper>,
                          public autofill::ChildFrameRegistrarObserver,
                          public autofill::FormActivityObserver,
                          public web::WebFramesManager::Observer {
 public:
  AutofillTabHelper(const AutofillTabHelper&) = delete;
  AutofillTabHelper& operator=(const AutofillTabHelper&) = delete;

  ~AutofillTabHelper() override;

  // Sets a weak reference to the view controller used to present UI.
  void SetBaseViewController(UIViewController* base_view_controller);

  void SetAutofillHandler(id<AutofillCommands> autofill_handler);
  // Sets the handler for snackbar UI commands.
  void SetSnackbarHandler(id<SnackbarCommands> snackbar_handler);

  // Sets the handler for form input accessory commands.
  void SetFormInputAccessoryHandler(
      id<FormInputAccessoryCommands> form_input_accessory_handler);

  // Returns an object that can provide Autofill suggestions.
  id<FormSuggestionProvider> GetSuggestionProvider();

  autofill::AutofillClientIOS* autofill_client();

  // Returns the `ChromeAutofillClientIOS` instance bound to this tab helper.
  autofill::ChromeAutofillClientIOS* chrome_autofill_client() {
    return autofill_client_.get();
  }

 private:
  friend class web::WebStateUserData<AutofillTabHelper>;

  explicit AutofillTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WasHidden(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // autofill::FormActivityObserver implementation.
  void FormActivityRegistered(
      web::WebState* web_state,
      web::WebFrame* sender_frame,
      const autofill::FormActivityParams& params) override;

  // web::WebFramesManager::Observer implementation.
  void WebFrameBecameUnavailable(web::WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override;

  // autofill::ChildFrameRegistrarObserver implementation.
  void OnDidDoubleRegistration(autofill::LocalFrameToken local) override;

  // Hides the suggestions pushed by `BrowserAutofillManager` for `reason`.
  // iOS never forwards focus and navigation events to `BrowserAutofillManager`,
  // so without this the suggestion data would outlive the field it's for.
  void HideSuggestions(autofill::SuggestionHidingReason reason);

  // Re-creates `autofill_agent_delegate_` with `snackbar_handler_` and
  // assigns it to the `autofill_agent_`. If `snackbar_handler_` is nil, both
  // the delegate and its assignment are nilled out.
  void UpdateAutofillAgentDelegate();

  // The handler for snackbar commands.
  __weak id<SnackbarCommands> snackbar_handler_;

  // The delegate for the AutofillAgent.
  __strong AutofillAgentDelegate* autofill_agent_delegate_;

  // The Objective-C AutofillAgent instance.
  __strong AutofillAgent* autofill_agent_;

  // The iOS AutofillClient instance.
  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;

  // The WebState holding this instance of the helper.
  raw_ptr<web::WebState> web_state_;

  // Scoped WebState observation.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Scoped form activity observation, used to hide suggestions on focus
  // changes.
  base::ScopedObservation<autofill::FormActivityTabHelper,
                          autofill::FormActivityObserver>
      form_activity_observation_{this};

  // Scoped observation of the Autofill content world frames, used to hide
  // suggestions when the frame holding them goes away.
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      frames_manager_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_TAB_HELPER_H_
