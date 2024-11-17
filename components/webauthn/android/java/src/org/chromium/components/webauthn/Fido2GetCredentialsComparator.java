// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Helper class to compare the results from the passkey cache service and FIDO2 module. Should not
 * be used if WebAuthenticationAndroidUsePasskeyCache is disabled. Note that the cache results are
 * not used to enumerate passkeys. They are used only for comparison.
 */
public class Fido2GetCredentialsComparator {
    public static class Factory {
        private static Fido2GetCredentialsComparator sInstanceForTesting;

        public static Fido2GetCredentialsComparator get() {
            return sInstanceForTesting == null
                    ? new Fido2GetCredentialsComparator()
                    : sInstanceForTesting;
        }

        public static void setInstanceForTesting(Fido2GetCredentialsComparator instance) {
            sInstanceForTesting = instance;
        }
    }

    @IntDef({
        SuccessState.FIDO2_FAILED_CACHE_FAILED,
        SuccessState.FIDO2_FAILED_CACHE_SUCCESSFUL,
        SuccessState.FIDO2_SUCCESSFUL_CACHE_FAILED,
        SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL,
        SuccessState.COUNT
    })
    public @interface SuccessState {
        int FIDO2_FAILED_CACHE_FAILED = 0;
        int FIDO2_FAILED_CACHE_SUCCESSFUL = 1;
        int FIDO2_SUCCESSFUL_CACHE_FAILED = 2;
        int FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL = 3;
        int COUNT = 4;
    }

    private static class State {
        public final boolean successful;
        public final long completionTime;
        public final int credentialCount;

        public State(boolean successful, long completionTime, int credentialCount) {
            this.successful = successful;
            this.completionTime = completionTime;
            this.credentialCount = credentialCount;
        }
    }

    private static final String HISTOGRAM_PREFIX = "WebAuthentication.Android.Fido2VsPasskeyCache.";

    private State mPasskeysCacheResultState;
    private State mFido2ResultState;

    void onGetCredentialsSuccessful(int credentialCount) {
        if (mFido2ResultState != null) {
            return;
        }
        long realtimeNow = SystemClock.elapsedRealtime();
        if (mPasskeysCacheResultState == null) {
            mFido2ResultState = new State(true, realtimeNow, credentialCount);
            return;
        }
        if (mPasskeysCacheResultState.successful) {
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_PREFIX + "PasskeyCacheFasterMs",
                    realtimeNow - mPasskeysCacheResultState.completionTime);
            RecordHistogram.recordCount100Histogram(
                    HISTOGRAM_PREFIX + "CredentialCountDifference",
                    Math.abs(credentialCount - mPasskeysCacheResultState.credentialCount));
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_PREFIX + "SuccessState",
                    SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL,
                    SuccessState.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_PREFIX + "SuccessState",
                    SuccessState.FIDO2_SUCCESSFUL_CACHE_FAILED,
                    SuccessState.COUNT);
        }
    }

    void onGetCredentialsFailed() {
        if (mFido2ResultState != null) {
            return;
        }
        long realtimeNow = SystemClock.elapsedRealtime();
        if (mPasskeysCacheResultState == null) {
            mFido2ResultState = new State(false, realtimeNow, -1);
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_PREFIX + "SuccessState",
                mPasskeysCacheResultState.successful
                        ? SuccessState.FIDO2_FAILED_CACHE_SUCCESSFUL
                        : SuccessState.FIDO2_FAILED_CACHE_FAILED,
                SuccessState.COUNT);
    }

    void onCachedGetCredentialsSuccessful(int credentialCount) {
        if (mPasskeysCacheResultState != null) {
            return;
        }
        long realtimeNow = SystemClock.elapsedRealtime();
        if (mFido2ResultState == null) {
            mPasskeysCacheResultState = new State(true, realtimeNow, credentialCount);
            return;
        }
        if (mFido2ResultState.successful) {
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_PREFIX + "Fido2FasterMs",
                    realtimeNow - mFido2ResultState.completionTime);
            RecordHistogram.recordCount100Histogram(
                    HISTOGRAM_PREFIX + "CredentialCountDifference",
                    Math.abs(credentialCount - mFido2ResultState.credentialCount));
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_PREFIX + "SuccessState",
                    SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL,
                    SuccessState.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_PREFIX + "SuccessState",
                    SuccessState.FIDO2_FAILED_CACHE_SUCCESSFUL,
                    SuccessState.COUNT);
        }
    }

    void onCachedGetCredentialsFailed() {
        if (mPasskeysCacheResultState != null) {
            return;
        }
        long realtimeNow = SystemClock.elapsedRealtime();
        if (mFido2ResultState == null) {
            mPasskeysCacheResultState = new State(false, realtimeNow, -1);
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_PREFIX + "SuccessState",
                mFido2ResultState.successful
                        ? SuccessState.FIDO2_SUCCESSFUL_CACHE_FAILED
                        : SuccessState.FIDO2_FAILED_CACHE_FAILED,
                SuccessState.COUNT);
    }

    // Use the factory to create an instance.
    private Fido2GetCredentialsComparator() {}
}
