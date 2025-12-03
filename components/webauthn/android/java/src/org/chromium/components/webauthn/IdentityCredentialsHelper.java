// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.webauthn.WebauthnLogger.log;
import static org.chromium.components.webauthn.WebauthnLogger.logError;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.identitycredentials.CreateCredentialHandle;
import com.google.android.gms.identitycredentials.CreateCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.IdentityCredentialManager;
import com.google.android.gms.identitycredentials.SignalCredentialStateRequest;

import org.jni_zero.JNINamespace;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialReportOptions;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.webauthn.cred_man.CredManHelper;

/** Handles requests dispatched to the identity_credentials GmsCore module. */
@JNINamespace("webauthn")
@NullMarked
public class IdentityCredentialsHelper {
    private static final String TAG = "IdentityCredentialsHelper";
    private static final String CRED_MAN_PREFIX = "androidx.credentials.";

    private final AuthenticationContextProvider mAuthenticationContextProvider;

    // A callback that provides an AuthenticatorStatus error in the first argument, and optionally a
    // metrics recording outcome in the second.
    public interface ErrorCallback {
        void onResult(int error, @Nullable Integer metricsOutcome);
    }

    public IdentityCredentialsHelper(AuthenticationContextProvider authenticationContextProvider) {
        mAuthenticationContextProvider = authenticationContextProvider;
    }

    // Dispatches a WebAuthn conditionalCreate request.
    public void handleConditionalCreateRequest(
            PublicKeyCredentialCreationOptions options,
            String origin,
            byte @Nullable [] clientDataJson,
            byte @Nullable [] clientDataHash) {
        log(TAG, "handleConditionalCreateRequest");
        WebauthnRequestCallback callback =
                assertNonNull(mAuthenticationContextProvider.getRequestCallback());
        try {
            IdentityCredentialClient client =
                    IdentityCredentialManager.Companion.getClient(
                            assertNonNull(mAuthenticationContextProvider.getContext()));
            client.createCredential(buildConditionalCreateRequest(options, origin, clientDataHash))
                    .addOnSuccessListener(
                            GmsCoreUtils.wrapSuccessCallback(
                                    (handle) ->
                                            onConditionalCreateSuccess(
                                                    clientDataJson, options, handle)))
                    .addOnFailureListener(
                            GmsCoreUtils.wrapFailureCallback(this::onConditionalCreateFailure));
        } catch (Exception e) {
            logError(TAG, "CreateCredential failed ", e);
            callback.onComplete(
                    WebauthnRequestResponse.forFailedMakeCredential(
                            AuthenticatorStatus.NOT_ALLOWED_ERROR,
                            new RequestMetrics.Builder()
                                    .setMakeCredentialOutcome(MakeCredentialOutcome.OTHER_FAILURE)
                                    .setMakeCredentialResult(
                                            CredentialRequestResult
                                                    .ANDROID_IDENTITY_CREDENTIALS_ERROR)
                                    .build()));
            return;
        }
    }

    private void onConditionalCreateSuccess(
            byte @Nullable [] clientDataJson,
            PublicKeyCredentialCreationOptions options,
            CreateCredentialHandle handle) {
        log(TAG, "onConditionalCreateSuccess");
        Bundle data = assertNonNull(handle.getCreateCredentialResponse()).getData();
        MakeCredentialAuthenticatorResponse response =
                CredManHelper.parseCreateCredentialResponseData(data);
        if (response == null) {
            log(TAG, "parseCreateCredentialResponseData() failed");
            assumeNonNull(mAuthenticationContextProvider.getRequestCallback())
                    .onComplete(
                            WebauthnRequestResponse.forFailedMakeCredential(
                                    AuthenticatorStatus.NOT_ALLOWED_ERROR,
                                    new RequestMetrics.Builder()
                                            .setMakeCredentialOutcome(
                                                    MakeCredentialOutcome.OTHER_FAILURE)
                                            .setMakeCredentialResult(
                                                    CredentialRequestResult
                                                            .ANDROID_IDENTITY_CREDENTIALS_ERROR)
                                            .build()));
            return;
        }
        if (clientDataJson != null) {
            response.info.clientDataJson = clientDataJson;
        }
        response.echoCredProps = options.credProps;
        assumeNonNull(mAuthenticationContextProvider.getRequestCallback())
                .onComplete(
                        WebauthnRequestResponse.forSuccessfulMakeCredential(
                                response,
                                new RequestMetrics.Builder()
                                        .setMakeCredentialOutcome(MakeCredentialOutcome.SUCCESS)
                                        .setMakeCredentialResult(
                                                CredentialRequestResult
                                                        .ANDROID_IDENTITY_CREDENTIALS_SUCCESS)
                                        .build()));
    }

    private void onConditionalCreateFailure(Exception e) {
        log(TAG, "CreateCredential request failed ", e);
        WebauthnRequestCallback callback = mAuthenticationContextProvider.getRequestCallback();
        if (callback == null) {
            return;
        }
        callback.onComplete(
                WebauthnRequestResponse.forFailedMakeCredential(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                        new RequestMetrics.Builder()
                                .setMakeCredentialOutcome(
                                        MakeCredentialOutcome.CONDITIONAL_CREATE_FAILURE)
                                .setMakeCredentialResult(
                                        CredentialRequestResult.ANDROID_IDENTITY_CREDENTIALS_ERROR)
                                .build()));
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

    // Dispatches a Report request.
    public void handleReportRequest(PublicKeyCredentialReportOptions options, String origin) {
        log(TAG, "handleReportRequest");
        try {
            IdentityCredentialClient client =
                    IdentityCredentialManager.Companion.getClient(
                            assertNonNull(mAuthenticationContextProvider.getContext()));
            client.signalCredentialState(buildSignalCredentialStateRequest(options, origin))
                    .addOnSuccessListener(
                            GmsCoreUtils.wrapSuccessCallback(
                                    (handle) ->
                                            log(TAG, "Signal API request completed successfully")))
                    .addOnFailureListener(
                            GmsCoreUtils.wrapFailureCallback(
                                    (e) -> logError(TAG, "Signal API Report request failed ", e)));
        } catch (Exception e) {
            logError(TAG, "handleReportRequest failed ", e);
            return;
        }
    }

    @VisibleForTesting
    public SignalCredentialStateRequest buildSignalCredentialStateRequest(
            PublicKeyCredentialReportOptions options, String origin) {
        String type;
        if (options.unknownCredentialId != null) {
            type = CRED_MAN_PREFIX + "SIGNAL_UNKNOWN_CREDENTIAL_STATE_REQUEST_TYPE";
        } else if (options.allAcceptedCredentials != null) {
            type = CRED_MAN_PREFIX + "SIGNAL_ALL_ACCEPTED_CREDENTIALS_REQUEST_TYPE";
        } else {
            assert (options.currentUserDetails != null);
            type = CRED_MAN_PREFIX + "SIGNAL_CURRENT_USER_DETAILS_STATE_REQUEST_TYPE";
        }

        String requestJson =
                Fido2CredentialRequestJni.get().reportOptionsToJson(options.serialize());
        Bundle requestDataBundle = new Bundle();
        requestDataBundle.putCharSequence(CRED_MAN_PREFIX + "signal_request_json_key", requestJson);

        return new SignalCredentialStateRequest(type, origin, requestDataBundle);
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
