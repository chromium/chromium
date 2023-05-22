// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate.h"

#include <memory>
#include <utility>
#include "base/android/build_info.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

WebAuthnCredManDelegate::WebAuthnCredManDelegate(
    content::WebContents* web_contents) {}

WebAuthnCredManDelegate::~WebAuthnCredManDelegate() = default;

void WebAuthnCredManDelegate::OnCredManConditionalRequestPending(
    content::RenderFrameHost* render_frame_host,
    bool has_results,
    base::RepeatingClosure full_assertion_request) {
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
  full_assertion_request_.Run();
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

// static
bool WebAuthnCredManDelegate::IsCredManEnabled() {
  return base::android::BuildInfo::GetInstance()->is_at_least_u() &&
         base::FeatureList::IsEnabled(device::kWebAuthnAndroidCredMan);
}

// static
WebAuthnCredManDelegate* WebAuthnCredManDelegate::GetRequestDelegate(
    content::WebContents* web_contents) {
  static constexpr char kWebAuthnCredManDelegateKey[] = "WebAuthnCredManKey";
  auto* delegate = static_cast<WebAuthnCredManDelegate*>(
      web_contents->GetUserData(kWebAuthnCredManDelegateKey));
  if (!delegate) {
    auto new_user_data =
        std::make_unique<WebAuthnCredManDelegate>(web_contents);
    delegate = new_user_data.get();
    web_contents->SetUserData(kWebAuthnCredManDelegateKey,
                              std::move(new_user_data));
  }

  return delegate;
}
