// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.identitycredentials.CreateCredentialHandle;
import com.google.android.gms.identitycredentials.CreateCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.IdentityCredentialManager;

import org.jni_zero.JNINamespace;

import org.chromium.base.Log;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.webauthn.cred_man.CredManHelper;

/** Handles requests dispatched to the identity_credentials GmsCore module. */
@JNINamespace("webauthn")
@NullMarked
public class IdentityCredentialsHelper {
    private static final String TAG = "IdentityCredHelper";

    private static final String CRED_MAN_PREFIX = "androidx.credentials.";

    private final AuthenticationContextProvider mAuthenticationContextProvider;

    // A callback that provides an AuthenticatorStatus error in the first argument, and optionally a
    // metrics recording outcome in the second.
    public interface ErrorCallback {
        public void onResult(int error, @Nullable Integer metricsOutcome);
    }

    public IdentityCredentialsHelper(AuthenticationContextProvider authenticationContextProvider) {
        mAuthenticationContextProvider = authenticationContextProvider;
    }

    // Dispatches a WebAuthn conditionalCreate request.
    public void handleConditionalCreateRequest(
            PublicKeyCredentialCreationOptions options,
            String origin,
            byte @Nullable [] clientDataJson,
            byte @Nullable [] clientDataHash,
            MakeCredentialResponseCallback responseCallback,
            ErrorCallback errorCallback) {
        try {
            IdentityCredentialClient client =
                    IdentityCredentialManager.Companion.getClient(
                            assertNonNull(mAuthenticationContextProvider.getContext()));
            client.createCredential(buildConditionalCreateRequest(options, origin, clientDataHash))
                    .addOnSuccessListener(
                            (handle) ->
                                    onConditionalCreateSuccess(
                                            clientDataJson,
                                            options,
                                            responseCallback,
                                            errorCallback,
                                            handle))
                    .addOnFailureListener(
                            (exception) -> onConditionalCreateFailure(errorCallback, exception));
        } catch (Exception e) {
            Log.d(TAG, "CreateCredential failed ", e);
            errorCallback.onResult(
                    AuthenticatorStatus.NOT_ALLOWED_ERROR, MakeCredentialOutcome.OTHER_FAILURE);
            return;
        }
    }

    private void onConditionalCreateSuccess(
            byte @Nullable [] clientDataJson,
            PublicKeyCredentialCreationOptions options,
            MakeCredentialResponseCallback responseCallback,
            ErrorCallback errorCallback,
            CreateCredentialHandle handle) {
        Bundle data = assertNonNull(handle.getCreateCredentialResponse()).getData();
        MakeCredentialAuthenticatorResponse response =
                CredManHelper.parseCreateCredentialResponseData(data);
        if (response == null) {
            Log.d(TAG, "parseCreateCredentialResponseData() failed");
            errorCallback.onResult(
                    AuthenticatorStatus.NOT_ALLOWED_ERROR, MakeCredentialOutcome.OTHER_FAILURE);
            return;
        }
        if (clientDataJson != null) {
            response.info.clientDataJson = clientDataJson;
        }
        response.echoCredProps = options.credProps;
        responseCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS, response);
    }

    private void onConditionalCreateFailure(ErrorCallback errorCallback, Exception e) {
        Log.d(TAG, "CreateCredential request failed ", e);
        errorCallback.onResult(
                AuthenticatorStatus.NOT_ALLOWED_ERROR,
                MakeCredentialOutcome.CONDITIONAL_CREATE_FAILURE);
    }

    private Bundle requestBundle(String requestJson, byte @Nullable [] clientDataHash) {
        Bundle credentialData = new Bundle();
        credentialData.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_CREATE_PUBLIC_KEY_CREDENTIAL_REQUEST");
        credentialData.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", requestJson);
        credentialData.putByteArray(
                CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", clientDataHash);
        return credentialData;
    }

    @VisibleForTesting
    public CreateCredentialRequest buildConditionalCreateRequest(
            PublicKeyCredentialCreationOptions options,
            String origin,
            byte @Nullable [] clientDataHash) {
        String requestJson =
                Fido2CredentialRequestJni.get().createOptionsToJson(options.serialize());

        Bundle credentialData = requestBundle(requestJson, clientDataHash);
        Bundle displayInfo = new Bundle();
        displayInfo.putCharSequence(CRED_MAN_PREFIX + "BUNDLE_KEY_USER_ID", options.user.name);
        displayInfo.putCharSequence(
                CRED_MAN_PREFIX + "BUNDLE_KEY_USER_DISPLAY_NAME", options.user.displayName);
        credentialData.putBundle(CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_DISPLAY_INFO", displayInfo);

        Bundle candidateQueryData = requestBundle(requestJson, clientDataHash);
        candidateQueryData.putBoolean(CRED_MAN_PREFIX + "BUNDLE_KEY_IS_CONDITIONAL_REQUEST", true);

        return new CreateCredentialRequest(
                CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL",
                credentialData,
                candidateQueryData,
                origin,
                requestJson,
                null);
    }
}
