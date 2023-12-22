// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.Credential;
import android.credentials.GetCredentialResponse;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Shadow of the Android Credential Manager GetCredentialResponse object. */
@Implements(value = GetCredentialResponse.class)
public class ShadowGetCredentialResponse {
    Credential mCredential;

    @Implementation
    protected void __constructor__(Credential credential) {
        mCredential = credential;
    }

    @Implementation
    protected Credential getCredential() {
        return mCredential;
    }
}
