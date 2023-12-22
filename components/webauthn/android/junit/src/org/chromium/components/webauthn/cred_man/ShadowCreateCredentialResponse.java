// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CreateCredentialResponse;
import android.os.Bundle;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Shadow of the Android Credential Manager CreateCredentialResponse object. */
@Implements(value = CreateCredentialResponse.class)
public class ShadowCreateCredentialResponse {
    @Implementation
    protected Bundle getData() {
        Bundle data = new Bundle();
        data.putString("androidx.credentials.BUNDLE_KEY_REGISTRATION_RESPONSE_JSON", "json");
        return data;
    }
}
