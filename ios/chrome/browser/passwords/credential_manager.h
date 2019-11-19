// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_

#include "components/password_manager/core/browser/credential_manager_impl.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#import "ios/web/public/web_state.h"

namespace web {
class WebFrame;
}

// Owned by PasswordController. It is responsible for registering and handling
// callbacks for JS methods |navigator.credentials.*|.
// Expected flow of CredentialManager class:
// 0. Add script command callbacks, initialize JSCredentialManager
// 1. A command is sent from JavaScript to the browser.
// 2. HandleScriptCommand is called, it parses the message and constructs a
//     OnceCallback to be passed as parameter to proper CredentialManagerImpl
//     method. |promiseId| field from received JS message is bound to
//     constructed OnceCallback.
// 3. CredentialManagerImpl method is invoked, performs some logic with
//     PasswordStore, calls passed OnceCallback with result.
// 4. The OnceCallback uses JSCredentialManager to send result back to the
//     website.
class CredentialManager {
 public:
  CredentialManager(password_manager::PasswordManagerClient* client,
                    web::WebState* web_state);
  ~CredentialManager();

#if defined(UNIT_TEST)
  void set_leak_factory(
      std::unique_ptr<password_manager::LeakDetectionCheckFactory> factory) {
    impl_.set_leak_factory(std::move(factory));
  }
#endif  // defined(UNIT_TEST)

 private:
  // HandleScriptCommand parses JSON message and invokes Get, Store or
  // PreventSilentAccess on CredentialManagerImpl.
  void HandleScriptCommand(const base::DictionaryValue& json,
                           const GURL& origin_url,
                           bool user_is_interacting,
                           web::WebFrame* sender_frame);

  // Passed as callback to CredentialManagerImpl::Get.
  void SendGetResponse(
      int promise_id,
      password_manager::CredentialManagerError error,
      const base::Optional<password_manager::CredentialInfo>& info);
  // Passed as callback to CredentialManagerImpl::PreventSilentAccess.
  void SendPreventSilentAccessResponse(int promise_id);
  // Passed as callback to CredentialManagerImpl::Store.
  void SendStoreResponse(int promise_id);

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  password_manager::CredentialManagerImpl impl_;
  web::WebState* web_state_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManager);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_
