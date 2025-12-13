// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"

#import <memory>

#import "base/containers/flat_map.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
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
IOSWebAuthnCredentialsDelegateFactory::GetDelegateForFrame(
    const std::string& frame_id) {
  auto it = delegate_map_.find(frame_id);
  if (it == delegate_map_.end()) {
    auto [new_it, inserted] = delegate_map_.try_emplace(
        frame_id, std::make_unique<IOSWebAuthnCredentialsDelegate>());
    it = new_it;
  }
  return it->second.get();
}

void IOSWebAuthnCredentialsDelegateFactory::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  delegate_map_.erase(frame_id);
}

}  // namespace webauthn
