// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CLIENT_ANDROID_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CLIENT_ANDROID_H_

#include <memory>

#include "base/callback_forward.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class DiscoverableCredentialMetadata;
}

namespace components {

class WebAuthnClientAndroid {
 public:
  virtual ~WebAuthnClientAndroid();

  // Called by the embedder to set the static instance of this client.
  static void SetClient(std::unique_ptr<WebAuthnClientAndroid> client);

  // Accessor for the client that has been set by the embedder.
  static WebAuthnClientAndroid* GetClient();

  // Called when a Web Authentication Conditional UI request is received. This
  // provides the callback that will complete the request if and when a user
  // selects a credential from a form autofill dialog.
  virtual void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) = 0;
};

}  // namespace components

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CLIENT_ANDROID_H_
