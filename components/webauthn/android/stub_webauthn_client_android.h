// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_STUB_WEBAUTHN_CLIENT_ANDROID_H_
#define COMPONENTS_WEBAUTHN_ANDROID_STUB_WEBAUTHN_CLIENT_ANDROID_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/webauthn/android/webauthn_client_android.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class DiscoverableCredentialMetadata;
}

namespace webauthn {

class StubWebAuthnClientAndroid : public WebAuthnClientAndroid {
 public:
  ~StubWebAuthnClientAndroid() override;

  // Resets the `WebAuthnClientAndroid` for testing.
  static void ClearClient();

  void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      std::vector<device::DiscoverableCredentialMetadata> credentials,
      AssertionMediationType mediation_type,
      base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
          passkey_callback,
      base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
          password_callback,
      base::RepeatingClosure hybrid_closure,
      base::RepeatingCallback<void(NonCredentialReturnReason)>
          non_credential_callback) override;
  void CleanupWebAuthnRequest(content::RenderFrameHost* frame_host) override;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_STUB_WEBAUTHN_CLIENT_ANDROID_H_
