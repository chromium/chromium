// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate.h"

#include <utility>

#include "base/android/build_info.h"
#include "base/functional/callback.h"
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
    bool has_passkeys,
    base::RepeatingCallback<void(bool)> full_assertion_request) {
  has_passkeys_ = has_passkeys ? kHasPasskeys : kNoPasskeys;
  show_cred_man_ui_callback_ = std::move(full_assertion_request);
}

void WebAuthnCredManDelegate::OnCredManUiClosed(bool success) {
  if (!request_completion_callback_.is_null()) {
    request_completion_callback_.Run(success);
  }
}

void WebAuthnCredManDelegate::TriggerCredManUi() {
  if (show_cred_man_ui_callback_.is_null()) {
    return;
  }
  show_cred_man_ui_callback_.Run(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordsInCredMan));
}

WebAuthnCredManDelegate::State WebAuthnCredManDelegate::HasPasskeys() {
  return has_passkeys_;
}

void WebAuthnCredManDelegate::CleanUpConditionalRequest() {
  show_cred_man_ui_callback_.Reset();
  has_passkeys_ = kNotReady;
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
