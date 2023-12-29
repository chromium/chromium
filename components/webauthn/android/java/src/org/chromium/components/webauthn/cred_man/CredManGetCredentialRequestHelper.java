// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static org.chromium.components.webauthn.cred_man.CredManHelper.CRED_MAN_PREFIX;
import static org.chromium.components.webauthn.cred_man.CredManHelper.TYPE_PASSKEY;

import android.credentials.CredentialOption;
import android.credentials.GetCredentialRequest;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.content_public.browser.RenderFrameHost;

/**
 * This class is responsible for holding the arguments to create a valid {@link
 * GetCredentialRequest}. The request can be formed using the `getGetCredentialRequest` method.
 */
class CredManGetCredentialRequestHelper {
    private static CredManGetCredentialRequestHelper sInstanceForTesting;

    // Auto-select means that, when an allowlist is present and one of the providers matches with
    // it, the account selector can be skipped. (However, if two or more providers match with the
    // allowlist then the selector will, sadly, still be shown.)
    private static final String IS_AUTO_SELECT_ALLOWED =
            CRED_MAN_PREFIX + "BUNDLE_KEY_IS_AUTO_SELECT_ALLOWED";
    private static final String TYPE_PASSWORD_CREDENTIAL =
            "android.credentials.TYPE_PASSWORD_CREDENTIAL";

    private String mRequestAsJson;
    private byte[] mClientDataHash;
    private boolean mPreferImmediatelyAvailable;
    private boolean mAllowAutoSelect;
    private boolean mRequestPasswords;

    @Nullable private String mOrigin;
    private boolean mPlayServicesAvailable;
    private boolean mIgnoreGpm;
    @Nullable private RenderFrameHost mRenderFrameHost;

    static class Builder {
        private CredManGetCredentialRequestHelper mHelper;

        Builder(
                String requestAsJson,
                byte[] clientDataHash,
                boolean preferImmediatelyAvailable,
                boolean allowAutoSelect,
                boolean requestPasswords) {
            mHelper = CredManGetCredentialRequestHelper.getInstance();
            mHelper.mRequestAsJson = requestAsJson;
            mHelper.mClientDataHash = clientDataHash;
            mHelper.mPreferImmediatelyAvailable = preferImmediatelyAvailable;
            mHelper.mAllowAutoSelect = allowAutoSelect;
            mHelper.mRequestPasswords = requestPasswords;
        }

        Builder setOrigin(String origin) {
            mHelper.mOrigin = origin;
            return this;
        }

        Builder setPlayServicesAvailable(boolean playServicesAvailable) {
            mHelper.mPlayServicesAvailable = playServicesAvailable;
            return this;
        }

        Builder setIgnoreGpm(boolean ignoreGpm) {
            mHelper.mIgnoreGpm = ignoreGpm;
            return this;
        }

        Builder setRenderFrameHost(RenderFrameHost renderFrameHost) {
            mHelper.mRenderFrameHost = renderFrameHost;
            return this;
        }

        CredManGetCredentialRequestHelper build() {
            if (sInstanceForTesting != null) return sInstanceForTesting;
            return mHelper;
        }
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    GetCredentialRequest getGetCredentialRequest(@Nullable CredManRequestDecorator decorator) {
        final Bundle requestBundle = getGetCredentialRequestBundle(decorator);
        var builder = new GetCredentialRequest.Builder(requestBundle);
        final CredentialOption publicKeyCredentialOption = getPublicKeyCredentialOption(decorator);
        final CredentialOption passwordCredentialOption = getPasswordCredentialOption(decorator);
        if (decorator != null) {
            decorator.updateGetCredentialRequestBuilder(builder, this);
        }
        builder.addCredentialOption(publicKeyCredentialOption);
        if (passwordCredentialOption != null) builder.addCredentialOption(passwordCredentialOption);
        return builder.build();
    }

    boolean getPreferImmediatelyAvailable() {
        return mPreferImmediatelyAvailable;
    }

    @Nullable
    String getOrigin() {
        return mOrigin;
    }

    boolean getPlayServicesAvailable() {
        return mPlayServicesAvailable;
    }

    boolean getIgnoreGpm() {
        return mIgnoreGpm;
    }

    @Nullable
    RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    private Bundle getGetCredentialRequestBundle(@Nullable CredManRequestDecorator decorator) {
        Bundle bundle = getBaseGetCredentialRequestBundle();
        if (decorator != null) {
            decorator.updateGetCredentialRequestBundle(bundle, this);
        }
        return bundle;
    }

    private Bundle getBaseGetCredentialRequestBundle() {
        Bundle getCredentialRequestBundle = new Bundle();
        getCredentialRequestBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS",
                mPreferImmediatelyAvailable);
        return getCredentialRequestBundle;
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private CredentialOption getPublicKeyCredentialOption(
            @Nullable CredManRequestDecorator decorator) {
        Bundle publicKeyCredentialOptionBundle = getBasePublicKeyCredentialOptionBundle();
        if (decorator != null) {
            decorator.updatePublicKeyCredentialOptionBundle(publicKeyCredentialOptionBundle, this);
        }
        CredentialOption publicKeyCredentialOption =
                new CredentialOption.Builder(
                                TYPE_PASSKEY,
                                publicKeyCredentialOptionBundle,
                                publicKeyCredentialOptionBundle)
                        .build();
        return publicKeyCredentialOption;
    }

    private Bundle getBasePublicKeyCredentialOptionBundle() {
        Bundle publicKeyCredentialOptionBundle = new Bundle();
        publicKeyCredentialOptionBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_GET_PUBLIC_KEY_CREDENTIAL_OPTION");
        publicKeyCredentialOptionBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", mRequestAsJson);
        publicKeyCredentialOptionBundle.putByteArray(
                CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", mClientDataHash);
        publicKeyCredentialOptionBundle.putBoolean(IS_AUTO_SELECT_ALLOWED, mAllowAutoSelect);
        return publicKeyCredentialOptionBundle;
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private @Nullable CredentialOption getPasswordCredentialOption(
            @Nullable CredManRequestDecorator decorator) {
        if (!mRequestPasswords) return null;
        Bundle passwordCredentialOptionBundle = new Bundle();
        if (decorator != null) {
            decorator.updatePasswordCredentialOptionBundle(passwordCredentialOptionBundle, this);
        }
        var builder =
                new CredentialOption.Builder(
                        TYPE_PASSWORD_CREDENTIAL,
                        passwordCredentialOptionBundle,
                        passwordCredentialOptionBundle);
        if (decorator != null) {
            decorator.updatePasswordCredentialOptionBuilder(builder, this);
        }
        return builder.build();
    }

    private static CredManGetCredentialRequestHelper getInstance() {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return new CredManGetCredentialRequestHelper();
    }

    public static void setInstanceForTesting(CredManGetCredentialRequestHelper instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
    }

    private CredManGetCredentialRequestHelper() {}
}
