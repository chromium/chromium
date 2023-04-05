// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;

/**
 * Callback interface for receiving a response from a request to register a
 * credential with an authenticator.
 */
public interface MakeCredentialResponseCallback {
    public void onRegisterResponse(int status, MakeCredentialAuthenticatorResponse response);
}
