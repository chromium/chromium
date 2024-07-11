// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_FACTORY_TEST_API_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_FACTORY_TEST_API_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace webauthn {

class WebAuthnCredManDelegate;

class WebAuthnCredManDelegateFactoryTestApi {
 public:
  explicit WebAuthnCredManDelegateFactoryTestApi(
      WebAuthnCredManDelegateFactory* factory)
      : factory_(*factory) {}

  void EmplaceDelegateForFrame(
      content::RenderFrameHost* rfh,
      std::unique_ptr<WebAuthnCredManDelegate> delegate) {
    factory_->delegate_map_.try_emplace(rfh, std::move(delegate));
  }

 private:
  const raw_ref<WebAuthnCredManDelegateFactory> factory_;
};

inline WebAuthnCredManDelegateFactoryTestApi test_api(
    WebAuthnCredManDelegateFactory* factory) {
  return WebAuthnCredManDelegateFactoryTestApi(factory);
}

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_FACTORY_TEST_API_H_
