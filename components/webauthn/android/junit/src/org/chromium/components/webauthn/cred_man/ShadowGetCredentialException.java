// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.GetCredentialException;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Shadow of the Android Credential Manager GetCredentialException exception. */
@Implements(value = GetCredentialException.class)
public class ShadowGetCredentialException {
    private String mType;
    private String mMessage;

    @Implementation
    protected void __constructor__(String type, String message) {
        mType = type;
        mMessage = message;
    }

    @Implementation
    protected String getType() {
        return mType;
    }
}
