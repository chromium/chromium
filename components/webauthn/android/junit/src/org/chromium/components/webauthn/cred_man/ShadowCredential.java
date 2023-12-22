// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.Credential;
import android.os.Bundle;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;

/** Shadow of the Android Credential Manager Credential object. */
@Implements(value = Credential.class)
public class ShadowCredential {
    private static final String TYPE_PASSKEY = "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL";
    private static final String USERNAME = "coolUserName";
    private static final String PASSWORD = "38kay5er1sp0r38";

    @RealObject private Credential mRealObject;
    private Bundle mData;
    private String mType;

    @Implementation
    protected void __constructor__(String type, Bundle data) {
        ShadowCredential shadow = Shadow.extract(mRealObject);
        shadow.mData = data;
        shadow.mType = type;
    }

    @Implementation
    protected Bundle getData() {
        return mData;
    }

    @Implementation
    protected String getType() {
        return mType;
    }
}
