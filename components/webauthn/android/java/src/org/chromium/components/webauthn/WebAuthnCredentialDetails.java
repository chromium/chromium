// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.base.annotations.CalledByNative;

/**
 * Describes a WebAuthn credential available on the authenticator.
 */
public class WebAuthnCredentialDetails {
    public String mUserName;
    public String mUserDisplayName;
    public byte[] mUserId;
    public byte[] mCredentialId;

    public WebAuthnCredentialDetails() {}

    @CalledByNative
    public String getUserName() {
        return mUserName;
    }

    @CalledByNative
    public String getUserDisplayName() {
        return mUserDisplayName;
    }

    @CalledByNative
    public byte[] getUserId() {
        return mUserId;
    }

    @CalledByNative
    public byte[] getCredentialId() {
        return mCredentialId;
    }
}
