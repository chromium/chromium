// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static org.chromium.components.webauthn.WebauthnLogger.log;

import android.content.Context;
import android.os.Build;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.webauthn.CredManSupport;
import org.chromium.components.webauthn.GmsCoreUtils;
import org.chromium.components.webauthn.WebauthnFeatureMap;
import org.chromium.components.webauthn.WebauthnFeatures;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;

@NullMarked
public class CredManSupportProvider {
    private static final String TAG = "CredManSupportProvider";
    private static final int GMSCORE_MIN_VERSION_CANARY_DEV = 241900000;
    private static final int GMSCORE_MIN_VERSION_BETA_STABLE = 242300000;

    private static @CredManSupport int sCredManSupport;

    private static @Nullable Integer sOverrideAndroidVersion;
    private static @Nullable Boolean sOverrideForcesGpm;

    public static void setupForTesting(
            @Nullable Integer overrideAndroidVersion, @Nullable Boolean overrideForcesGpm) {
        sOverrideAndroidVersion = overrideAndroidVersion;
        sOverrideForcesGpm = overrideForcesGpm;
        sCredManSupport = CredManSupport.NOT_EVALUATED;
        ResettersForTesting.register(
                () -> {
                    sOverrideAndroidVersion = null;
                    sOverrideForcesGpm = null;

                    // While this is not a test-specific value, the state shouldn't leak between
                    // tests.
                    sCredManSupport = CredManSupport.NOT_EVALUATED;
                });
    }

    @CalledByNative
    public static @CredManSupport int getCredManSupport() {
        if (sCredManSupport != CredManSupport.NOT_EVALUATED) {
            return sCredManSupport;
        }
        if (WebauthnFeatureMap.getInstance()
                .isEnabled(WebauthnFeatures.WEBAUTHN_ANDROID_CRED_MAN_FOR_DEV)) {
            String mode =
                    WebauthnFeatureMap.getInstance()
                            .getFieldTrialParamByFeature(
                                    WebauthnFeatures.WEBAUTHN_ANDROID_CRED_MAN_FOR_DEV, "mode");
            if (mode.equals("full")) {
                sCredManSupport = CredManSupport.FULL_UNLESS_INAPPLICABLE;
                log(TAG, "Support is %d due to dev flag", sCredManSupport);
                return sCredManSupport;
            } else if (mode.equals("parallel")) {
                sCredManSupport = CredManSupport.PARALLEL_WITH_FIDO_2;
                log(TAG, "Support is %d due to dev flag", sCredManSupport);
                return sCredManSupport;
            } else if (mode.equals("disabled")) {
                sCredManSupport = CredManSupport.DISABLED;
                log(TAG, "Support is %d due to dev flag", sCredManSupport);
                return sCredManSupport;
            }
        }
        if (getAndroidVersion() < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            sCredManSupport = CredManSupport.DISABLED;
            log(TAG, "Disabled because of Android version.");
            return sCredManSupport;
        }
        if (notSkippedBecauseInTests() && hasOldGmsVersion()) {
            sCredManSupport = CredManSupport.DISABLED;
            log(TAG, "Disabled because of old GMS version.");
            return sCredManSupport;
        }
        if (notSkippedBecauseInTests()
                && ContextUtils.getApplicationContext().getSystemService(Context.CREDENTIAL_SERVICE)
                        == null) {
            sCredManSupport = CredManSupport.DISABLED;
            recordCredManAvailability(/* available= */ false);
            log(TAG, "Disabled because CredentialManager is not available.");
            return sCredManSupport;
        }
        recordCredManAvailability(/* available= */ true);

        final CredManUiRecommender recommender =
                ServiceLoaderUtil.maybeCreate(CredManUiRecommender.class);
        boolean customUiRecommended = recommender != null && recommender.recommendsCustomUi();
        boolean gpmInCredMan =
                sOverrideForcesGpm != null ? sOverrideForcesGpm : customUiRecommended;
        boolean isChrome3pPwmMode =
                WebauthnModeProvider.getInstance().getGlobalWebauthnMode()
                        == WebauthnMode.CHROME_3PP_ENABLED;
        // In CHROME_3PP_ENABLED mode Chrome does not use FIDO2 APIs parallel with CredMan. This
        // is because Chrome's Password Manager capabilities are disabled.
        sCredManSupport =
                gpmInCredMan || isChrome3pPwmMode
                        ? CredManSupport.FULL_UNLESS_INAPPLICABLE
                        : CredManSupport.PARALLEL_WITH_FIDO_2;
        log(TAG, "Support is %d", sCredManSupport);
        return sCredManSupport;
    }

    public static @CredManSupport int getCredManSupportForWebView() {
        if (sCredManSupport != CredManSupport.NOT_EVALUATED) {
            return sCredManSupport;
        }
        if (getAndroidVersion() < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            sCredManSupport = CredManSupport.DISABLED;
            return sCredManSupport;
        }
        if (notSkippedBecauseInTests()
                && ContextUtils.getApplicationContext().getSystemService(Context.CREDENTIAL_SERVICE)
                        == null) {
            sCredManSupport = CredManSupport.DISABLED;
            return sCredManSupport;
        }
        sCredManSupport = CredManSupport.FULL_UNLESS_INAPPLICABLE;
        return sCredManSupport;
    }

    private static void recordCredManAvailability(boolean available) {
        RecordHistogram.recordBooleanHistogram(
                "WebAuthentication.Android.CredManAvailability", available);
    }

    private static boolean hasOldGmsVersion() {
        assert sOverrideAndroidVersion == null : "Don't use in testing!";
        // The check works for unavailable and low GMS versions. `getGmsCoreVersion()` is -1 if the
        // GMS version can't be retrieved. Chrome assumes an insufficient GMS availability then.
        return GmsCoreUtils.getGmsCoreVersion() < getMinGmsVersionForCurrentChannel();
    }

    private static int getAndroidVersion() {
        return sOverrideAndroidVersion == null ? Build.VERSION.SDK_INT : sOverrideAndroidVersion;
    }

    private static boolean notSkippedBecauseInTests() {
        return sOverrideForcesGpm == null && sOverrideAndroidVersion == null;
    }

    private static int getMinGmsVersionForCurrentChannel() {
        return (VersionInfo.isBetaBuild() || VersionInfo.isStableBuild())
                ? GMSCORE_MIN_VERSION_BETA_STABLE
                : GMSCORE_MIN_VERSION_CANARY_DEV;
    }
}
