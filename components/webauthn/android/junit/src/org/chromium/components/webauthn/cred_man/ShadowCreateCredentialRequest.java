// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CreateCredentialRequest;
import android.os.Bundle;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;

/** Shadow of the Android Credential Manager CreateCredentialRequest object. */
@Implements(value = CreateCredentialRequest.class)
public class ShadowCreateCredentialRequest {
    private String mType;
    private Bundle mCredentialData;
    private Bundle mCandidateQueryData;
    private boolean mAlwaysSendAppInfoToProvider;
    private String mOrigin;

    @Implementation
    protected void __constructor__() {}

    @Implementation
    protected String getType() {
        return mType;
    }

    @Implementation
    protected Bundle getCredentialData() {
        return mCredentialData;
    }

    @Implementation
    protected Bundle getCandidateQueryData() {
        return mCandidateQueryData;
    }

    @Implementation
    protected boolean alwaysSendAppInfoToProvider() {
        return mAlwaysSendAppInfoToProvider;
    }

    @Implementation
    protected String getOrigin() {
        return mOrigin;
    }

    /** Builder for ShadowCreateCredentialRequest. */
    @Implements(value = CreateCredentialRequest.Builder.class)
    public static class ShadowBuilder {
        @RealObject private CreateCredentialRequest.Builder mRealBuilder;

        private String mType;
        private Bundle mCredentialData;
        private Bundle mCandidateQueryData;
        private boolean mAlwaysSendAppInfoToProvider;
        private String mOrigin;

        @Implementation
        protected void __constructor__(
                String type, Bundle credentialData, Bundle candidateQueryData) {
            mType = type;
            mCredentialData = credentialData;
            mCandidateQueryData = candidateQueryData;
        }

        @Implementation
        protected CreateCredentialRequest.Builder setAlwaysSendAppInfoToProvider(boolean value) {
            mAlwaysSendAppInfoToProvider = value;
            return mRealBuilder;
        }

        @Implementation
        protected CreateCredentialRequest.Builder setOrigin(String origin) {
            mOrigin = origin;
            return mRealBuilder;
        }

        protected String getOrigin() {
            return mOrigin;
        }

        @Implementation
        protected CreateCredentialRequest build() {
            CreateCredentialRequest realRequest =
                    Shadow.newInstanceOf(CreateCredentialRequest.class);
            ShadowCreateCredentialRequest shadow = Shadow.extract(realRequest);
            shadow.mType = mType;
            shadow.mCredentialData = mCredentialData;
            shadow.mCandidateQueryData = mCandidateQueryData;
            shadow.mAlwaysSendAppInfoToProvider = mAlwaysSendAppInfoToProvider;
            shadow.mOrigin = mOrigin;
            return realRequest;
        }
    }
}
