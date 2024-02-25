// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CreateCredentialException;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Shadow of the Android Credential Manager CreateCredentialException exception. */
@Implements(value = CreateCredentialException.class)
public class ShadowCreateCredentialException {
    private String mType;

    @Implementation
    protected void __constructor__() {}

    protected void setType(String type) {
        mType = type;
    }

    @Implementation
    protected String getType() {
        return mType;
    }
}
