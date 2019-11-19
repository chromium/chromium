// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/credential_manager.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/ios/credential_manager_util.h"
#include "ios/chrome/browser/passwords/js_credential_manager.h"
#include "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::CredentialManagerError;
using password_manager::CredentialInfo;
using password_manager::CredentialType;
using password_manager::CredentialMediationRequirement;

namespace {

// Script command prefix for CM API calls. Possible commands to be sent from
// injected JS are 'credentials.get', 'credentials.store' and
// 'credentials.preventSilentAccess'.
constexpr char kCommandPrefix[] = "credentials";

}  // namespace

CredentialManager::CredentialManager(
    password_manager::PasswordManagerClient* client,
    web::WebState* web_state)
    : impl_(client), web_state_(web_state) {
  subscription_ = web_state_->AddScriptCommandCallback(
      base::Bind(&CredentialManager::HandleScriptCommand,
                 base::Unretained(this)),
      kCommandPrefix);
}

CredentialManager::~CredentialManager() {}

void CredentialManager::HandleScriptCommand(const base::DictionaryValue& json,
                                            const GURL& origin_url,
                                            bool user_is_interacting,
                                            web::WebFrame* sender_frame) {
  if (!sender_frame->IsMainFrame()) {
    // Credentials manager is only supported on main frame.
    return;
  }
  double promise_id_double = -1;
  // |promiseId| field should be an integer value, but since JavaScript has only
  // one type for numbers (64-bit float), all numbers in the messages are sent
  // as doubles.
  if (!json.GetDouble("promiseId", &promise_id_double)) {
    DLOG(ERROR) << "Received bad json - no valid 'promiseId' field";
    return;
  }
  int promise_id = static_cast<int>(promise_id_double);

  if (!password_manager::WebStateContentIsSecureHtml(web_state_)) {
    RejectCredentialPromiseWithInvalidStateError(
        web_state_, promise_id,
        base::ASCIIToUTF16(
            "Credential Manager API called from insecure context"));
    return;
  }

  std::string command;
  if (!json.GetString("command", &command)) {
    DLOG(ERROR) << "Received bad json - no valid 'command' field";
    return;
  }

  if (command == "credentials.get") {
    CredentialMediationRequirement mediation;
    if (!ParseMediationRequirement(json, &mediation)) {
      RejectCredentialPromiseWithTypeError(
          web_state_, promise_id,
          base::ASCIIToUTF16(
              "CredentialRequestOptions: Invalid 'mediation' value."));
      return;
    }
    bool include_passwords;
    if (!password_manager::ParseIncludePasswords(json, &include_passwords)) {
      RejectCredentialPromiseWithTypeError(
          web_state_, promise_id,
          base::ASCIIToUTF16(
              "CredentialRequestOptions: Invalid 'password' value."));
      return;
    }
    std::vector<GURL> federations;
    if (!password_manager::ParseFederations(json, &federations)) {
      RejectCredentialPromiseWithTypeError(
          web_state_, promise_id,
          base::ASCIIToUTF16(
              "CredentialRequestOptions: invalid 'providers' value."));
      return;
    }
    impl_.Get(mediation, include_passwords, federations,
              base::BindOnce(&CredentialManager::SendGetResponse,
                             base::Unretained(this), promise_id));
    return;
  }
  if (command == "credentials.store") {
    CredentialInfo credential;
    std::string parse_message;
    if (!ParseCredentialDictionary(json, &credential, &parse_message)) {
      RejectCredentialPromiseWithTypeError(web_state_, promise_id,
                                           base::UTF8ToUTF16(parse_message));
      return;
    }
    impl_.Store(credential,
                base::BindOnce(&CredentialManager::SendStoreResponse,
                               base::Unretained(this), promise_id));
    return;
  }
  if (command == "credentials.preventSilentAccess") {
    impl_.PreventSilentAccess(
        base::BindOnce(&CredentialManager::SendPreventSilentAccessResponse,
                       base::Unretained(this), promise_id));
  }
}

void CredentialManager::SendGetResponse(
    int promise_id,
    CredentialManagerError error,
    const base::Optional<CredentialInfo>& info) {
  switch (error) {
    case CredentialManagerError::SUCCESS:
      ResolveCredentialPromiseWithCredentialInfo(web_state_, promise_id, info);
      break;
    case CredentialManagerError::PENDING_REQUEST:
      RejectCredentialPromiseWithInvalidStateError(
          web_state_, promise_id,
          base::ASCIIToUTF16("Pending 'get()' request."));
      break;
    case CredentialManagerError::PASSWORDSTOREUNAVAILABLE:
      RejectCredentialPromiseWithNotSupportedError(
          web_state_, promise_id,
          base::ASCIIToUTF16("Password store is unavailable."));
      break;
    case CredentialManagerError::UNKNOWN:
      RejectCredentialPromiseWithNotSupportedError(
          web_state_, promise_id,
          base::ASCIIToUTF16("Unknown error has occurred."));
  }
}

void CredentialManager::SendPreventSilentAccessResponse(int promise_id) {
  ResolveCredentialPromiseWithUndefined(web_state_, promise_id);
}

void CredentialManager::SendStoreResponse(int promise_id) {
  ResolveCredentialPromiseWithUndefined(web_state_, promise_id);
}
