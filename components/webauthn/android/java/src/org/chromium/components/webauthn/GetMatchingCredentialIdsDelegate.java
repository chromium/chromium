// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.components.webauthn.WebauthnLogger.log;
import static org.chromium.components.webauthn.WebauthnLogger.logError;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This class is responsible for getting matching credential ids from GMS Core. This is for payment
 * requests with an allowlist.
 */
@NullMarked
public class GetMatchingCredentialIdsDelegate {
    /**
     * Callback interface for receiving a response from a request to retrieve matching credential
     * ids from an authenticator. If the request is successful, the response will be a list of
     * credential ids that match the requested credential ids. If the request fails, the response
     * will be null.
     */
    @NullMarked
    public interface ResponseCallback {
        void onResponse(@Nullable List<byte[]> matchingCredentialIds);
    }

    private static final String TAG = "GetMatchingCredentialIdsDelegate";

    private static @Nullable GetMatchingCredentialIdsDelegate sInstance;
    private final boolean mIsGetMatchingCredentialIdsSupported;

    public static GetMatchingCredentialIdsDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new GetMatchingCredentialIdsDelegate();
        }
        return sInstance;
    }

    public void getMatchingCredentialIds(
            AuthenticationContextProvider authenticationContextProvider,
            String relyingPartyId,
            byte[][] allowCredentialIds,
            boolean requireThirdPartyPayment,
            ResponseCallback callback) {
        log(TAG, "getMatchingCredentialIds");

        if (!mIsGetMatchingCredentialIdsSupported) {
            logError(TAG, "GetMatchingCredentialIds is not supported.");
            callback.onResponse(null);
            return;
        }

        GmsCoreGetCredentialsHelper.getInstance()
                .getCredentials(
                        authenticationContextProvider,
                        relyingPartyId,
                        GmsCoreGetCredentialsHelper.Reason.GET_MATCHING_CREDENTIAL_IDS,
                        (credentials) ->
                                onGetMatchingCredentialIdsListReceived(
                                        credentials,
                                        allowCredentialIds,
                                        requireThirdPartyPayment,
                                        callback),
                        (exception) -> onException(exception, callback));
    }

    public static void setInstanceForTesting(GetMatchingCredentialIdsDelegate instance) {
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = null);
    }

    private GetMatchingCredentialIdsDelegate() {
        boolean isGooglePlayServicesAvailable =
                ExternalAuthUtils.getInstance()
                        .canUseGooglePlayServices(new UserRecoverableErrorHandler.Silent());
        if (!isGooglePlayServicesAvailable) {
            logError(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            mIsGetMatchingCredentialIdsSupported = false;
            return;
        }

        if (!GmsCoreUtils.isGetMatchingCredentialIdsSupported()) {
            logError(TAG, "GetMatchingCredentialIds is not supported in Google Play Services.");
            mIsGetMatchingCredentialIdsSupported = false;
            return;
        }

        mIsGetMatchingCredentialIdsSupported = true;
    }

    private void onGetMatchingCredentialIdsListReceived(
            List<WebauthnCredentialDetails> retrievedCredentials,
            byte[][] allowCredentialIds,
            boolean requireThirdPartyPayment,
            ResponseCallback callback) {
        log(TAG, "onGetMatchingCredentialIdsListReceived");
        List<byte[]> matchingCredentialIds = new ArrayList<>();
        for (WebauthnCredentialDetails credential : retrievedCredentials) {
            if (requireThirdPartyPayment && !credential.mIsPayment) continue;

            for (byte[] allowedId : allowCredentialIds) {
                if (Arrays.equals(allowedId, credential.mCredentialId)) {
                    matchingCredentialIds.add(credential.mCredentialId);
                    break;
                }
            }
        }
        callback.onResponse(matchingCredentialIds);
    }

    private void onException(Exception e, ResponseCallback callback) {
        logError(TAG, "FIDO2 API call failed", e);
        callback.onResponse(null);
    }
}
