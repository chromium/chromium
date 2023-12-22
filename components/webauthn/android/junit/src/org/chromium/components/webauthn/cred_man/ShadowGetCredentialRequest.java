// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CredentialOption;
import android.credentials.GetCredentialRequest;
import android.os.Bundle;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;

import java.util.ArrayList;
import java.util.List;

/** Shadow of the Android Credential Manager CreateCredentialRequest object. */
@Implements(value = GetCredentialRequest.class)
public class ShadowGetCredentialRequest {
    private Bundle mData;
    private String mOrigin;
    private List<CredentialOption> mCredentialOptions = new ArrayList<>();

    @Implementation
    protected void __constructor__() {}

    @Implementation
    protected Bundle getData() {
        return mData;
    }

    @Implementation
    protected String getOrigin() {
        return mOrigin;
    }

    @Implementation
    protected List<CredentialOption> getCredentialOptions() {
        return mCredentialOptions;
    }

    /** Builder for ShadowGetCredentialRequest. */
    @Implements(value = GetCredentialRequest.Builder.class)
    public static class ShadowBuilder {
        @RealObject private GetCredentialRequest.Builder mRealBuilder;

        private Bundle mData;
        private String mOrigin;
        private List<CredentialOption> mCredentialOptions = new ArrayList<>();

        @Implementation
        protected void __constructor__(Bundle data) {
            mData = data;
        }

        @Implementation
        protected GetCredentialRequest.Builder addCredentialOption(
                CredentialOption credentialOption) {
            mCredentialOptions.add(credentialOption);
            return mRealBuilder;
        }

        @Implementation
        protected GetCredentialRequest.Builder setOrigin(String origin) {
            mOrigin = origin;
            return mRealBuilder;
        }

        @Implementation
        protected GetCredentialRequest build() {
            GetCredentialRequest realRequest = Shadow.newInstanceOf(GetCredentialRequest.class);
            ShadowGetCredentialRequest shadow = Shadow.extract(realRequest);
            shadow.mOrigin = mOrigin;
            shadow.mData = mData;
            shadow.mCredentialOptions = mCredentialOptions;
            return realRequest;
        }
    }
}
