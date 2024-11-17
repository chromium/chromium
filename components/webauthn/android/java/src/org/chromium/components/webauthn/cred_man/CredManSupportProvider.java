// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.content.Context;
import android.os.Build;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.components.webauthn.CredManSupport;
import org.chromium.components.webauthn.GmsCoreUtils;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.device.DeviceFeatureList;
import org.chromium.device.DeviceFeatureMap;

public class CredManSupportProvider {
    private static final int GMSCORE_MIN_VERSION_CANARY_DEV = 241900000;
    private static final int GMSCORE_MIN_VERSION_BETA_STABLE = 242300000;

    private static @CredManSupport int sCredManSupport;

    private static boolean sOverrideVersionCheckForTesting;

    public static void setupForTesting(boolean override) {
        sOverrideVersionCheckForTesting = override;
        sCredManSupport = CredManSupport.NOT_EVALUATED;
    }

    @CalledByNative
    public static @CredManSupport int getCredManSupport() {
        if (sCredManSupport != CredManSupport.NOT_EVALUATED) {
            return sCredManSupport;
        }
        if (!sOverrideVersionCheckForTesting) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                sCredManSupport = CredManSupport.DISABLED;
                return sCredManSupport;
            }
            if (hasOldGmsVersion()) {
                sCredManSupport = CredManSupport.DISABLED;
                return sCredManSupport;
            }
        }

        if (!sOverrideVersionCheckForTesting
                && ContextUtils.getApplicationContext().getSystemService(Context.CREDENTIAL_SERVICE)
                        == null) {
            sCredManSupport = CredManSupport.DISABLED;
            recordCredManAvailability(/*available*/ false);
            return sCredManSupport;
        }
        recordCredManAvailability(/*available*/ true);

        if (DeviceFeatureMap.isEnabled(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN)) {
            CredManUiRecommender recommender =
                    ServiceLoaderUtil.maybeCreate(CredManUiRecommender.class);
            boolean customUiRecommended =
                    recommender == null ? false : recommender.recommendsCustomUi();
            boolean gpmInCredMan =
                    DeviceFeatureMap.getInstance()
                            .getFieldTrialParamByFeatureAsBoolean(
                                    DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN,
                                    "gpm_in_cred_man",
                                    customUiRecommended);
            boolean isChrome3pPwmMode =
                    WebauthnModeProvider.getInstance().getGlobalWebauthnMode()
                            == WebauthnMode.CHROME_3PP_ENABLED;
            // In CHROME_3PP_ENABLED mode Chrome does not use FIDO2 APIs parallel with CredMan. This
            // is because Chrome's Password Manager capabilities are disabled.
            sCredManSupport =
                    gpmInCredMan || isChrome3pPwmMode
                            ? CredManSupport.FULL_UNLESS_INAPPLICABLE
                            : CredManSupport.PARALLEL_WITH_FIDO_2;
            return sCredManSupport;
        }
        sCredManSupport = CredManSupport.IF_REQUIRED;

        return sCredManSupport;
    }

    public static @CredManSupport int getCredManSupportForWebView() {
        if (sCredManSupport != CredManSupport.NOT_EVALUATED) {
            return sCredManSupport;
        }
        if (!sOverrideVersionCheckForTesting) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                sCredManSupport = CredManSupport.DISABLED;
                return sCredManSupport;
            }
            if (ContextUtils.getApplicationContext().getSystemService(Context.CREDENTIAL_SERVICE)
                    == null) {
                sCredManSupport = CredManSupport.DISABLED;
                return sCredManSupport;
            }
        }
        sCredManSupport = CredManSupport.FULL_UNLESS_INAPPLICABLE;
        return sCredManSupport;
    }

    private static void recordCredManAvailability(boolean available) {
        RecordHistogram.recordBooleanHistogram(
                "WebAuthentication.Android.CredManAvailability", available);
    }

    private static boolean hasOldGmsVersion() {
        assert !sOverrideVersionCheckForTesting : "Don't use in testing!";
        int gmsVersion = GmsCoreUtils.getGmsCoreVersion();
        if (gmsVersion == -1) {
            return true; // Couldn't get a GMS version. Assume insufficient GMS availability.
        }

        final int requiredMinGmsVersion =
                DeviceFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsInt(
                                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN,
                                "min_gms_core_version_no_dots",
                                getMinGmsVersionForCurrentChannel());

        return gmsVersion < requiredMinGmsVersion;
    }

    private static int getMinGmsVersionForCurrentChannel() {
        return (VersionInfo.isBetaBuild() || VersionInfo.isStableBuild())
                ? GMSCORE_MIN_VERSION_BETA_STABLE
                : GMSCORE_MIN_VERSION_CANARY_DEV;
    }
}
