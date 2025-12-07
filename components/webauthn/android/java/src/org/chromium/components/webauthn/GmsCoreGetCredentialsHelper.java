// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.components.webauthn.WebauthnLogger.log;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.OnFailureListener;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * This class is responsible for getting credentials from GMS Core. It will either use the passkey
 * cache service or the FIDO2 APIs.
 */
@NullMarked
public class GmsCoreGetCredentialsHelper {
    private static final String TAG = "GmsCoreGetCredentialsHelper";
    private static @Nullable GmsCoreGetCredentialsHelper sInstance;

    private static final String GET_CREDENTIALS_RESULT_HISTOGRAM =
            "WebAuthentication.Android.GmsCoreGetCredentialsResult";
    private static final String CREDENTIAL_FETCH_DURATION_HISTOGRAM =
            "WebAuthentication.CredentialFetchDuration.GmsCore";

    // LINT.IfChange
    //
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    public @interface GmsCoreGetCredentialsResult {
        int CACHE_SUCCESS = 0;
        int CACHE_FAILURE_FALLBACK_SUCCESS = 1;
        int CACHE_FAILURE_FALLBACK_FAILURE = 2;
        int FIDO2_SUCCESS = 3;
        int FIDO2_FAILURE = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/webauthn/enums.xml)

    /** The reason for a get credentials request. */
    public enum Reason {
        // A regular get assertion request for a non-Google relying party.
        GET_ASSERTION_NON_GOOGLE,
        // A request where the relying party is Google. For Google RP the cache should
        // not be used.
        GET_ASSERTION_GOOGLE_RP,
        // A payment request. For payment requests the cache should not be used.
        PAYMENT,
        // A request for payment requests with an allowlist
        // and should not use the cache.
        GET_MATCHING_CREDENTIAL_IDS,
        // This is for checking for matching credentials before calling CredMan. These requests are
        // for non-payments, non-conditional requests with an allowlist and should not use the
        // cache.
        CHECK_FOR_MATCHING_CREDENTIALS,
    }

    /** Callback for receiving credentials from GMS Core. */
    public interface GetCredentialsCallback {
        void onCredentialsReceived(List<WebauthnCredentialDetails> credentials);
    }

    public static GmsCoreGetCredentialsHelper getInstance() {
        if (sInstance == null) {
            sInstance = new GmsCoreGetCredentialsHelper();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void overrideInstanceForTesting(GmsCoreGetCredentialsHelper helper) {
        sInstance = helper;
        ResettersForTesting.register(() -> sInstance = null);
    }

    /**
     * Gets credentials from GMS Core.
     *
     * @param authenticationContextProvider The provider for the context.
     * @param relyingPartyId The relying party ID for which to get the credentials.
     * @param reason The reason for the request.
     * @param successCallback The callback to be run on success.
     * @param failureCallback The callback to be run on failure.
     */
    public void getCredentials(
            AuthenticationContextProvider authenticationContextProvider,
            String relyingPartyId,
            Reason reason,
            GetCredentialsCallback successCallback,
            OnFailureListener failureCallback) {
        log(TAG, "getCredentials with reason: " + reason);
        final long startTimeMs = SystemClock.elapsedRealtime();
        if (reason == Reason.GET_ASSERTION_NON_GOOGLE
                && GmsCoreUtils.isPasskeyCacheSupported()
                && WebauthnFeatureMap.getInstance()
                        .isEnabled(WebauthnFeatures.WEBAUTHN_ANDROID_PASSKEY_CACHE_MIGRATION)) {
            Fido2ApiCallHelper.getInstance()
                    .invokePasskeyCacheGetCredentials(
                            authenticationContextProvider,
                            relyingPartyId,
                            (credentials) -> {
                                recordSuccessMetrics(
                                        credentials,
                                        reason,
                                        GmsCoreGetCredentialsResult.CACHE_SUCCESS,
                                        startTimeMs);
                                successCallback.onCredentialsReceived(credentials);
                            },
                            (e) -> {
                                log(
                                        TAG,
                                        "invokePasskeyCacheGetCredentials() failed. Falling back to"
                                                + " FIDO2. ",
                                        e);
                                getCredentialsFromFido2Api(
                                        authenticationContextProvider,
                                        relyingPartyId,
                                        reason,
                                        successCallback,
                                        failureCallback,
                                        GmsCoreGetCredentialsResult.CACHE_FAILURE_FALLBACK_SUCCESS,
                                        GmsCoreGetCredentialsResult.CACHE_FAILURE_FALLBACK_FAILURE,
                                        startTimeMs);
                            });
        } else {
            getCredentialsFromFido2Api(
                    authenticationContextProvider,
                    relyingPartyId,
                    reason,
                    successCallback,
                    failureCallback,
                    GmsCoreGetCredentialsResult.FIDO2_SUCCESS,
                    GmsCoreGetCredentialsResult.FIDO2_FAILURE,
                    startTimeMs);
        }
    }

    private void getCredentialsFromFido2Api(
            AuthenticationContextProvider authenticationContextProvider,
            String relyingPartyId,
            Reason reason,
            GetCredentialsCallback successCallback,
            OnFailureListener failureCallback,
            @GmsCoreGetCredentialsResult int successMetric,
            @GmsCoreGetCredentialsResult int failureMetric,
            long startTimeMs) {
        log(TAG, "getCredentialsFromFido2Api");
        Fido2ApiCallHelper.getInstance()
                .invokeFido2GetCredentials(
                        authenticationContextProvider,
                        relyingPartyId,
                        (credentials) -> {
                            recordSuccessMetrics(credentials, reason, successMetric, startTimeMs);
                            successCallback.onCredentialsReceived(credentials);
                        },
                        (e) -> {
                            log(TAG, "invokeFido2GetCredentials() failed. ", e);
                            RecordHistogram.recordEnumeratedHistogram(
                                    GET_CREDENTIALS_RESULT_HISTOGRAM,
                                    failureMetric,
                                    GmsCoreGetCredentialsResult.NUM_ENTRIES);
                            failureCallback.onFailure(e);
                        });
    }

    private void recordSuccessMetrics(
            List<WebauthnCredentialDetails> credentials,
            Reason reason,
            @GmsCoreGetCredentialsResult int result,
            long startTimeMs) {
        log(TAG, "recordSuccessMetrics with result: " + result);
        RecordHistogram.recordEnumeratedHistogram(
                GET_CREDENTIALS_RESULT_HISTOGRAM, result, GmsCoreGetCredentialsResult.NUM_ENTRIES);
        if ((reason == Reason.GET_ASSERTION_NON_GOOGLE || reason == Reason.GET_ASSERTION_GOOGLE_RP)
                && !credentials.isEmpty()) {
            final long durationMs = SystemClock.elapsedRealtime() - startTimeMs;
            RecordHistogram.recordTimesHistogram(CREDENTIAL_FETCH_DURATION_HISTOGRAM, durationMs);
            String metricName = getCredentialFetchDurationMetricName(result);
            if (!metricName.isEmpty()) {
                RecordHistogram.recordTimesHistogram(metricName, durationMs);
            }
        }
    }

    private String getCredentialFetchDurationMetricName(@GmsCoreGetCredentialsResult int result) {
        String suffix;
        switch (result) {
            case GmsCoreGetCredentialsResult.CACHE_SUCCESS:
                suffix = ".Cache";
                break;
            case GmsCoreGetCredentialsResult.CACHE_FAILURE_FALLBACK_SUCCESS:
                suffix = ".CacheFallback";
                break;
            case GmsCoreGetCredentialsResult.FIDO2_SUCCESS:
                suffix = ".Fido2";
                break;
            default:
                assert false
                        : "Unexpected GmsCoreGetCredentialsResult value for a successful"
                                + " getCredentials call: "
                                + result;
                return "";
        }
        return CREDENTIAL_FETCH_DURATION_HISTOGRAM + suffix;
    }
}
