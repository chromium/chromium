// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/js_credential_manager.h"

#include "base/json/string_escape.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Takes CredentialInfo and returns string representing invocation of
// Credential's constructor with proper values and type.
std::string CredentialInfoToJsCredential(
    const password_manager::CredentialInfo& info) {
  if (info.type ==
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED) {
    return base::StringPrintf(
        "new FederatedCredential({id: %s, name: %s, iconURL: %s, provider: "
        "%s})",
        base::GetQuotedJSONString(info.id.value_or(base::string16())).c_str(),
        base::GetQuotedJSONString(info.name.value_or(base::string16())).c_str(),
        base::GetQuotedJSONString(info.icon.spec()).c_str(),
        base::GetQuotedJSONString(info.federation.GetURL().spec()).c_str());
  }
  if (info.type == password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD) {
    return base::StringPrintf(
        "new PasswordCredential({id: %s, name: %s, iconURL: %s, password: %s})",
        base::GetQuotedJSONString(info.id.value_or(base::string16())).c_str(),
        base::GetQuotedJSONString(info.name.value_or(base::string16())).c_str(),
        base::GetQuotedJSONString(info.icon.spec()).c_str(),
        base::GetQuotedJSONString(info.password.value_or(base::string16()))
            .c_str());
  }
  /* if (info.type == CREDENTIAL_TYPE_EMPTY) */
  return std::string();
}

void ResolveOrRejectPromise(web::WebState* web_state,
                            int promise_id,
                            bool resolve,
                            const std::string& script) {
  DCHECK(web_state);
  std::string js_code = base::StringPrintf(
      "__gCrWeb.credentialManager.%s[%d](%s); delete "
      "__gCrWeb.credentialManager.%s[%d];",
      resolve ? "resolvers_" : "rejecters_", promise_id, script.c_str(),
      resolve ? "resolvers_" : "rejecters_", promise_id);
  web_state->ExecuteJavaScript(base::UTF8ToUTF16(js_code));
}

}  // namespace

void ResolveCredentialPromiseWithCredentialInfo(
    web::WebState* web_state,
    int promise_id,
    const base::Optional<password_manager::CredentialInfo>& info) {
  DCHECK(web_state);
  std::string credential_str = info.has_value()
                                   ? CredentialInfoToJsCredential(info.value())
                                   : std::string();
  ResolveOrRejectPromise(web_state, promise_id, /*resolve=*/true,
                         credential_str);
}

void ResolveCredentialPromiseWithUndefined(web::WebState* web_state,
                                           int promise_id) {
  DCHECK(web_state);
  ResolveOrRejectPromise(web_state, promise_id, /*resolve=*/true,
                         std::string());
}

void RejectCredentialPromiseWithTypeError(web::WebState* web_state,
                                          int promise_id,
                                          const base::StringPiece16& message) {
  DCHECK(web_state);
  std::string type_error_str = base::StringPrintf(
      "new TypeError(%s)", base::GetQuotedJSONString(message).c_str());
  ResolveOrRejectPromise(web_state, promise_id, /*resolve=*/false,
                         type_error_str);
}

void RejectCredentialPromiseWithInvalidStateError(
    web::WebState* web_state,
    int promise_id,
    const base::StringPiece16& message) {
  DCHECK(web_state);
  std::string invalid_state_err_str = base::StringPrintf(
      "Object.create(DOMException.prototype, "
      "{name:{value:DOMException.INVALID_STATE_ERR}, message:{value:%s}})",
      base::GetQuotedJSONString(message).c_str());
  ResolveOrRejectPromise(web_state, promise_id, /*resolve=*/false,
                         invalid_state_err_str);
}

void RejectCredentialPromiseWithNotSupportedError(
    web::WebState* web_state,
    int promise_id,
    const base::StringPiece16& message) {
  DCHECK(web_state);
  std::string not_supported_err_str = base::StringPrintf(
      "Object.create(DOMException.prototype, "
      "{name:{value:DOMException.NOT_SUPPORTED_ERR}, message:{value:%s}})",
      base::GetQuotedJSONString(message).c_str());
  ResolveOrRejectPromise(web_state, promise_id, /*resolve=*/false,
                         not_supported_err_str);
}
