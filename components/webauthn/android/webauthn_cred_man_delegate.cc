// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate.h"

#include <utility>
#include "base/android/build_info.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"

namespace content {
class WebContents;
}  // namespace content

namespace webauthn {

bool WebAuthnCredManDelegate::override_android_version_for_testing_ = false;

WebAuthnCredManDelegate::WebAuthnCredManDelegate(
    content::WebContents* web_contents) {}

WebAuthnCredManDelegate::~WebAuthnCredManDelegate() = default;

void WebAuthnCredManDelegate::OnCredManConditionalRequestPending(
    bool has_results,
    base::RepeatingCallback<void(bool)> full_assertion_request) {
  has_results_ = has_results;
  full_assertion_request_ = std::move(full_assertion_request);
}

void WebAuthnCredManDelegate::OnCredManUiClosed(bool success) {
  if (!request_completion_callback_.is_null()) {
    request_completion_callback_.Run(success);
  }
}

void WebAuthnCredManDelegate::TriggerFullRequest() {
  if (full_assertion_request_.is_null() || !HasResults()) {
    OnCredManUiClosed(false);
    return;
  }

  full_assertion_request_.Run(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordsInCredMan));
}

bool WebAuthnCredManDelegate::HasResults() {
  return has_results_;
}

void WebAuthnCredManDelegate::CleanUpConditionalRequest() {
  full_assertion_request_.Reset();
  has_results_ = false;
}

void WebAuthnCredManDelegate::SetRequestCompletionCallback(
    base::RepeatingCallback<void(bool)> callback) {
  request_completion_callback_ = std::move(callback);
}

void WebAuthnCredManDelegate::SetFillingCallback(
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        filling_callback) {
  filling_callback_ = std::move(filling_callback);
}

void WebAuthnCredManDelegate::FillUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password) {
  std::move(filling_callback_).Run(username, password);
}

// static
bool WebAuthnCredManDelegate::IsCredManEnabled() {
  return (override_android_version_for_testing_ ||
          base::android::BuildInfo::GetInstance()->is_at_least_u()) &&
         base::FeatureList::IsEnabled(device::kWebAuthnAndroidCredMan);
}

}  // namespace webauthn
