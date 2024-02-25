// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CLIENT_ANDROID_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CLIENT_ANDROID_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class DiscoverableCredentialMetadata;
}

namespace webauthn {

class WebAuthnClientAndroid {
 public:
  virtual ~WebAuthnClientAndroid();

  // Called by the embedder to set the static instance of this client.
  static void SetClient(std::unique_ptr<WebAuthnClientAndroid> client);

  // Accessor for the client that has been set by the embedder.
  static WebAuthnClientAndroid* GetClient();

  // Called when a Web Authentication request is received that can be handled
  // by the browser. This provides the callback that will complete the request
  // if and when a user selects a credential from a selection dialog.
  virtual void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      bool is_conditional_request,
      base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
          getAssertionCallback,
      base::RepeatingCallback<void()> hybridCallback) = 0;

  // Closes an outstanding conditional UI request, so passkeys will no longer be
  // displayed through autofill.
  virtual void CleanupWebAuthnRequest(content::RenderFrameHost* frame_host) = 0;

  // Called when a pendingGetCredential call is completed. The provided closure
  // can be used to trigger CredMan UI flows. Android U+ only.
  void OnCredManConditionalRequestPending(
      content::RenderFrameHost* render_frame_host,
      bool has_results,
      base::RepeatingCallback<void(bool)> full_assertion_request);

  // Called when a CredMan sheet is closed. This can happen if the user
  // dismissed the UI, selected a credential, or if there are errors. Android U+
  // only.
  void OnCredManUiClosed(content::RenderFrameHost* render_frame_host,
                         bool success);

  // Called when a conditional request that is stored in CredMan should be
  // cleaned. Android U+ only.
  void CleanupCredManRequest(content::RenderFrameHost* render_frame_host);

  // Called when a user selects a password from the CredMan UI. The provided
  // `username` and `password` can be filled in the password form in the
  // `render_frame_host`.
  void OnPasswordCredentialReceived(content::RenderFrameHost* render_frame_host,
                                    std::u16string username,
                                    std::u16string password);
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CLIENT_ANDROID_H_
