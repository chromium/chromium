// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_client_android.h"

#include <memory>

#include "base/check.h"

namespace components {

// The WebAuthnClientAndroid instance, which is set by the embedder.
WebAuthnClientAndroid* g_webauthn_client = nullptr;

WebAuthnClientAndroid::~WebAuthnClientAndroid() = default;

// static
void WebAuthnClientAndroid::SetClient(
    std::unique_ptr<WebAuthnClientAndroid> client) {
  DCHECK(client);
  DCHECK(!g_webauthn_client);
  g_webauthn_client = client.release();
}

// static
WebAuthnClientAndroid* WebAuthnClientAndroid::GetClient() {
  DCHECK(g_webauthn_client);
  return g_webauthn_client;
}

}  // namespace components
