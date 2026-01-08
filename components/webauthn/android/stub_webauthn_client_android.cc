// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/stub_webauthn_client_android.h"

#include "base/functional/callback.h"
#include "content/public/browser/render_frame_host.h"

namespace webauthn {

StubWebAuthnClientAndroid::~StubWebAuthnClientAndroid() = default;

void StubWebAuthnClientAndroid::OnWebAuthnRequestPending(
    content::RenderFrameHost* frame_host,
    std::vector<device::DiscoverableCredentialMetadata> credentials,
    AssertionMediationType mediation_type,
    base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
        passkey_callback,
    base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
        password_callback,
    base::RepeatingClosure hybrid_closure,
    base::RepeatingCallback<void(NonCredentialReturnReason)>
        non_credential_callback) {}

void StubWebAuthnClientAndroid::CleanupWebAuthnRequest(
    content::RenderFrameHost* frame_host) {}

}  // namespace webauthn
