// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

/** Constants related to the Authenticator. */
public final class AuthenticatorConstants {
    /**
     * https://w3c.github.io/webauthn/#enumdef-clientcapability
     *
     * <p>This is the subset of client capabilities computed by the browser. See also
     * //third_party/blink/renderer/modules/credentialmanagement/public_key_credential.cc.
     */
    public static final String CAPABILITY_RELATED_ORIGINS = "relatedOrigins";
    public static final String CAPABILITY_HYBRID_TRANSPORT = "hybridTransport";
    public static final String CAPABILITY_PPAA = "passkeyPlatformAuthenticator";
    public static final String CAPABILITY_UVPAA = "userVerifyingPlatformAuthenticator";
    public static final String CAPABILITY_CONDITIONAL_GET = "conditionalGet";
}
