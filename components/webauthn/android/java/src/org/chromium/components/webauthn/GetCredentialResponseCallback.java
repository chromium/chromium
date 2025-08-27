// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.CredentialInfo;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Callback interface for receiving a response from a request to produce a signed assertion from an
 * authenticator, or a username/password `CredentialInfo` in some cases.
 */
@NullMarked
public interface GetCredentialResponseCallback {
    void onCredentialResponse(
            @Nullable GetAssertionAuthenticatorResponse assertionResponse,
            @Nullable CredentialInfo passwordCredential);
}
