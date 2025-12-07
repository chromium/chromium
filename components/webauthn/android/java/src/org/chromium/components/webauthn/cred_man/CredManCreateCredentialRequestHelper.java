// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static org.chromium.components.webauthn.cred_man.CredManHelper.CRED_MAN_PREFIX;
import static org.chromium.components.webauthn.cred_man.CredManHelper.TYPE_PASSKEY;

import android.credentials.CreateCredentialRequest;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This class is responsible for holding the arguments to create a valid {@link
 * CreateCredentialRequest}. The request can be formed using the `getCreateCredentialRequest`
 * method.
 */
@NullMarked
class CredManCreateCredentialRequestHelper {
    private static @Nullable CredManCreateCredentialRequestHelper sInstanceForTesting;

    private @Nullable String mRequestAsJson;
    private byte @Nullable [] mClientDataHash;
    private @Nullable String mOrigin;
    private byte @Nullable [] mUserId;

    static class Builder {
        private final CredManCreateCredentialRequestHelper mHelper;

        Builder(String requestAsJson, byte @Nullable [] clientDataHash) {
            mHelper = CredManCreateCredentialRequestHelper.getInstance();
            mHelper.mRequestAsJson = requestAsJson;
            mHelper.mClientDataHash = clientDataHash;
        }

        Builder setUserId(byte[] userId) {
            mHelper.mUserId = userId;
            return this;
        }

        Builder setOrigin(String origin) {
            mHelper.mOrigin = origin;
            return this;
        }

        CredManCreateCredentialRequestHelper build() {
            return mHelper;
        }
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    CreateCredentialRequest getCreateCredentialRequest(
            @Nullable CredManRequestDecorator decorator) {
        final Bundle requestBundle = getBundleForRequest(decorator);
        var builder =
                new CreateCredentialRequest.Builder(TYPE_PASSKEY, requestBundle, requestBundle)
                        .setAlwaysSendAppInfoToProvider(true);
        if (decorator != null) {
            decorator.updateCreateCredentialRequestBuilder(builder, this);
        }
        return builder.build();
    }

    @Nullable String getOrigin() {
        return mOrigin;
    }

    byte @Nullable [] getUserId() {
        return mUserId;
    }

    private static CredManCreateCredentialRequestHelper getInstance() {
        if (sInstanceForTesting == null) return new CredManCreateCredentialRequestHelper();
        return sInstanceForTesting;
    }

    public static void setInstanceForTesting(
            CredManCreateCredentialRequestHelper instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
    }

    private Bundle getBundleForRequest(@Nullable CredManRequestDecorator decorator) {
        Bundle bundle = getBaseCreateCredentialRequestBundle();
        if (decorator != null) {
            decorator.updateCreateCredentialRequestBundle(bundle, this);
        }
        return bundle;
    }

    private Bundle getBaseCreateCredentialRequestBundle() {
        Bundle createCredentialRequestBundle = new Bundle();
        // The CreateCredentialRequest is for a public key credential.
        createCredentialRequestBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_CREATE_PUBLIC_KEY_CREDENTIAL_REQUEST");
        // The PublicKeyCredentialCreationOptions JSON as string. @see
        // https://w3c.github.io/webauthn/#dictdef-publickeycredentialcreationoptionsjson
        createCredentialRequestBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", mRequestAsJson);
        // The SHA-256 of the ClientDataJSON as byte array.
        createCredentialRequestBundle.putByteArray(
                CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", mClientDataHash);
        return createCredentialRequestBundle;
    }

    private CredManCreateCredentialRequestHelper() {}
}
