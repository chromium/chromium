// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Helper class to compare the results from the passkey cache service and FIDO2 module. Should not
 * be used if WebAuthenticationAndroidUsePasskeyCache is disabled. Note that the cache results are
 * not used to enumerate passkeys. They are used only for comparison.
 */
@NullMarked
public class Fido2GetCredentialsComparator {
    public static class Factory {
        private static @Nullable Fido2GetCredentialsComparator sInstanceForTesting;

        public static Fido2GetCredentialsComparator get(boolean isGoogleRp) {
            return sInstanceForTesting == null
                    ? new Fido2GetCredentialsComparator(isGoogleRp)
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

        public static State failed() {
            return new State(
                    /* successful= */ false, /* completionTime= */ 0, /* credentialCount= */ -1);
        }

        public static State successful(long completionTime, int credentialCount) {
            return new State(/* successful= */ true, completionTime, credentialCount);
        }

        private State(boolean successful, long completionTime, int credentialCount) {
            this.successful = successful;
            this.completionTime = completionTime;
            this.credentialCount = credentialCount;
        }
    }

    private static final String HISTOGRAM_PREFIX = "WebAuthentication.Android.Fido2VsPasskeyCache";

    private @Nullable State mPasskeysCacheResultState;
    private @Nullable State mFido2ResultState;
    private final boolean mIsGoogleRp;

    void onGetCredentialsSuccessful(int credentialCount) {
        if (mFido2ResultState != null) {
            return;
        }
        mFido2ResultState = State.successful(SystemClock.elapsedRealtime(), credentialCount);
        performComparison();
    }

    void onGetCredentialsFailed() {
        if (mFido2ResultState != null) {
            return;
        }
        mFido2ResultState = State.failed();
        performComparison();
    }

    void onCachedGetCredentialsSuccessful(int credentialCount) {
        if (mPasskeysCacheResultState != null) {
            return;
        }
        mPasskeysCacheResultState =
                State.successful(SystemClock.elapsedRealtime(), credentialCount);
        performComparison();
    }

    void onCachedGetCredentialsFailed() {
        if (mPasskeysCacheResultState != null) {
            return;
        }
        mPasskeysCacheResultState = State.failed();
        performComparison();
    }

    private void performComparison() {
        if (mFido2ResultState == null || mPasskeysCacheResultState == null) {
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_PREFIX + ".SuccessState", getSuccessState(), SuccessState.COUNT);
        if (!mFido2ResultState.successful || !mPasskeysCacheResultState.successful) {
            return;
        }
        long timeDifference =
                Math.abs(
                        mFido2ResultState.completionTime
                                - mPasskeysCacheResultState.completionTime);
        String speedHistogramName =
                mFido2ResultState.completionTime < mPasskeysCacheResultState.completionTime
                        ? ".Fido2FasterMs"
                        : ".PasskeyCacheFasterMs";
        String rpSuffix = mIsGoogleRp ? ".GoogleRp" : ".NonGoogleRp";

        RecordHistogram.recordTimesHistogram(HISTOGRAM_PREFIX + speedHistogramName, timeDifference);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_PREFIX + speedHistogramName + rpSuffix, timeDifference);
        int credentialCountDifference =
                Math.abs(
                        mFido2ResultState.credentialCount
                                - mPasskeysCacheResultState.credentialCount);
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_PREFIX + ".CredentialCountDifference", credentialCountDifference);
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_PREFIX + ".CredentialCountDifference" + rpSuffix,
                credentialCountDifference);
        if (mFido2ResultState.credentialCount != mPasskeysCacheResultState.credentialCount) {
            // The difference we see between the two APIs are significant. Also emit the
            // credential counts.
            RecordHistogram.recordCount1000Histogram(
                    HISTOGRAM_PREFIX + ".Fido2CredentialCountWhenDifferent",
                    mFido2ResultState.credentialCount);
            RecordHistogram.recordCount1000Histogram(
                    HISTOGRAM_PREFIX + ".PasskeyCacheCredentialCountWhenDifferent",
                    mPasskeysCacheResultState.credentialCount);
        }
    }

    private @SuccessState int getSuccessState() {
        assumeNonNull(mFido2ResultState);
        assumeNonNull(mPasskeysCacheResultState);
        if (mFido2ResultState.successful) {
            return mPasskeysCacheResultState.successful
                    ? SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL
                    : SuccessState.FIDO2_SUCCESSFUL_CACHE_FAILED;
        }
        return mPasskeysCacheResultState.successful
                ? SuccessState.FIDO2_FAILED_CACHE_SUCCESSFUL
                : SuccessState.FIDO2_FAILED_CACHE_FAILED;
    }

    // Use the factory to create an instance.
    private Fido2GetCredentialsComparator(boolean isGoogleRp) {
        mIsGoogleRp = isGoogleRp;
    }
}
