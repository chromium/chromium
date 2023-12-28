// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.content.ComponentName;
import android.credentials.CredentialOption;
import android.os.Bundle;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;

import java.util.Set;

/** Shadow of the CredentialOption object. */
@Implements(value = CredentialOption.class)
public class ShadowCredentialOption {
    private String mType;
    private Bundle mCredentialRetrievalData;
    private Bundle mCandidateQueryData;
    private Set<ComponentName> mAllowedProviders = Set.of();

    @Implementation
    protected void __constructor__() {}

    @Implementation
    protected String getType() {
        return mType;
    }

    @Implementation
    protected Bundle getCredentialRetrievalData() {
        return mCredentialRetrievalData;
    }

    @Implementation
    protected Bundle getCandidateQueryData() {
        return mCandidateQueryData;
    }

    @Implementation
    protected boolean isSystemProviderRequired() {
        return false;
    }

    /** Builder for ShadowCredentialOption. */
    @Implements(value = CredentialOption.Builder.class)
    public static class ShadowBuilder {
        @RealObject private CredentialOption.Builder mRealBuilder;

        private String mType;
        private Bundle mCredentialRetrievalData;
        private Bundle mCandidateQueryData;
        private Set<ComponentName> mAllowedProviders = Set.of();

        @Implementation
        protected void __constructor__(
                String type, Bundle credentialRetrievalData, Bundle candidateQueryData) {
            mType = type;
            mCredentialRetrievalData = credentialRetrievalData;
            mCandidateQueryData = candidateQueryData;
        }

        @Implementation
        protected CredentialOption.Builder setAllowedProviders(Set<ComponentName> providers) {
            return mRealBuilder;
        }

        @Implementation
        protected CredentialOption build() {
            CredentialOption realOption = Shadow.newInstanceOf(CredentialOption.class);
            ShadowCredentialOption shadow = Shadow.extract(realOption);
            shadow.mType = mType;
            shadow.mCredentialRetrievalData = mCredentialRetrievalData;
            shadow.mCandidateQueryData = mCandidateQueryData;
            shadow.mAllowedProviders = mAllowedProviders;
            return realOption;
        }
    }
}
