// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_TAB_HELPER_H_

#include "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class CommandDispatcher;
@protocol FormSuggestionProvider;
@class PasswordController;
@protocol PasswordControllerDelegate;
@protocol PasswordGenerationProvider;
@protocol PasswordsUiDelegate;
@class SharedPasswordController;

namespace password_manager {
class PasswordManager;
class PasswordManagerClient;
}  // namespace password_manager

// Class binding a PasswordController to a WebState. This class also opens a
// native Passwords UI on a specific link.
class PasswordTabHelper : public web::WebStateObserver,
                          public web::WebStatePolicyDecider,
                          public web::WebStateUserData<PasswordTabHelper> {
 public:
  PasswordTabHelper(const PasswordTabHelper&) = delete;
  PasswordTabHelper& operator=(const PasswordTabHelper&) = delete;

  ~PasswordTabHelper() override;

  // Sets the PasswordController delegate.
  void SetPasswordControllerDelegate(id<PasswordControllerDelegate> delegate);

  // Sets the CommandDispatcher.
  void SetDispatcher(CommandDispatcher* dispatcher);

  // Returns an object that can provide suggestions from the PasswordController.
  // May return nil.
  id<FormSuggestionProvider> GetSuggestionProvider();

  // Returns the PasswordManager owned by the PasswordController.
  password_manager::PasswordManager* GetPasswordManager();

  // Returns the PasswordManagerClient owned by the PasswordController.
  password_manager::PasswordManagerClient* GetPasswordManagerClient();

  // Returns an object that can provide password generation from the
  // PasswordController. May return nil.
  id<PasswordGenerationProvider> GetPasswordGenerationProvider();

  // Returns the SharedPasswordController owned by the PasswordController.
  SharedPasswordController* GetSharedPasswordController();

  // web::WebStatePolicyDecider:
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override;

 private:
  friend class web::WebStateUserData<PasswordTabHelper>;

  explicit PasswordTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The Objective-C password controller instance.
  __strong PasswordController* controller_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_TAB_HELPER_H_
