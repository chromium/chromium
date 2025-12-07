// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_FACTORY_H_
#define COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_FACTORY_H_

#import "base/memory/raw_ptr.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

namespace webauthn {

// This factory creates and manages the lifecycle of
// IOSWebAuthnCredentialsDelegate objects. There is one factory per WebState,
// and it creates one delegate per WebFrame.
class IOSWebAuthnCredentialsDelegateFactory
    : public web::WebFramesManager::Observer,
      public web::WebStateUserData<IOSWebAuthnCredentialsDelegateFactory> {
 public:
  ~IOSWebAuthnCredentialsDelegateFactory() override;

  IOSWebAuthnCredentialsDelegateFactory(
      const IOSWebAuthnCredentialsDelegateFactory&) = delete;
  IOSWebAuthnCredentialsDelegateFactory& operator=(
      const IOSWebAuthnCredentialsDelegateFactory&) = delete;

  // Returns the factory for the given `web_state`, creating one if it doesn't
  // already exist.
  static IOSWebAuthnCredentialsDelegateFactory* GetFactory(
      web::WebState* web_state);

  // Returns the delegate for the given `frame`, creating one if it doesn't
  // already exist.
  IOSWebAuthnCredentialsDelegate* GetDelegateForFrame(
      const std::string& frame_id);

 private:
  friend class web::WebStateUserData<IOSWebAuthnCredentialsDelegateFactory>;

  explicit IOSWebAuthnCredentialsDelegateFactory(web::WebState* web_state);

  // web::WebFramesManager::Observer:
  void WebFrameBecameUnavailable(web::WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override;

  // Maps a frame ID to an IOSWebAuthnCredentialsDelegate.
  base::flat_map<std::string, std::unique_ptr<IOSWebAuthnCredentialsDelegate>>
      delegate_map_;

  raw_ptr<web::WebState> web_state_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_IOS_WEBAUTHN_CREDENTIALS_DELEGATE_FACTORY_H_
