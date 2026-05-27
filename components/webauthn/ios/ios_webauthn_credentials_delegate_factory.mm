// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"

#import <memory>

#import "base/containers/flat_map.h"
#import "base/functional/bind.h"
#import "base/strings/string_util.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/web/public/web_state.h"

namespace webauthn {

IOSWebAuthnCredentialsDelegateFactory::IOSWebAuthnCredentialsDelegateFactory(
    web::WebState* web_state)
    : web_state_(web_state) {
  autofill::AutofillJavaScriptFeature::GetInstance()
      ->GetWebFramesManager(web_state_)
      ->AddObserver(this);
}

IOSWebAuthnCredentialsDelegateFactory::
    ~IOSWebAuthnCredentialsDelegateFactory() {
  autofill::AutofillJavaScriptFeature::GetInstance()
      ->GetWebFramesManager(web_state_)
      ->RemoveObserver(this);
}

// static
IOSWebAuthnCredentialsDelegateFactory*
IOSWebAuthnCredentialsDelegateFactory::GetFactory(web::WebState* web_state) {
  // This does nothing if it already exists.
  CreateForWebState(web_state);
  return FromWebState(web_state);
}

IOSWebAuthnCredentialsDelegate*
IOSWebAuthnCredentialsDelegateFactory::GetDelegateForFrameId(
    const std::string& frame_id) {
  if (frame_id.empty()) {
    return nullptr;
  }
  std::string lower_frame_id = base::ToLowerASCII(frame_id);
  auto it = delegate_map_.find(lower_frame_id);
  if (it == delegate_map_.end()) {
    auto [new_it, inserted] = delegate_map_.try_emplace(
        lower_frame_id,
        std::make_unique<IOSWebAuthnCredentialsDelegate>(web_state_));
    it = new_it;
  }
  return it->second.get();
}

void IOSWebAuthnCredentialsDelegateFactory::GetDelegateForRemoteFrameToken(
    autofill::RemoteFrameToken remote_frame_token,
    base::OnceCallback<void(IOSWebAuthnCredentialsDelegate*)> callback) {
  if (!remote_frame_token) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto* registrar = autofill::ChildFrameRegistrar::FromWebState(web_state_);
  if (!registrar) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto delegate_resolver = base::BindOnce(
      [](base::WeakPtr<IOSWebAuthnCredentialsDelegateFactory> factory,
         base::OnceCallback<void(IOSWebAuthnCredentialsDelegate*)> callback,
         autofill::LocalFrameToken local_token) {
        if (!factory) {
          std::move(callback).Run(nullptr);
          return;
        }
        std::move(callback).Run(
            factory->GetDelegateForFrameId(local_token.ToString()));
      },
      weak_factory_.GetWeakPtr(), std::move(callback));

  // Note that DeclareNewRemoteToken initiallly calls LookupChildFrame, so
  // DeclareNewRemoteToken is called directly here to avoid calling
  // LookupChildFrame twice in a row.
  registrar->DeclareNewRemoteToken(remote_frame_token,
                                   std::move(delegate_resolver));
}

void IOSWebAuthnCredentialsDelegateFactory::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  delegate_map_.erase(frame_id);
}

}  // namespace webauthn
