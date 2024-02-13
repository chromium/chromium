// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.base.PackageUtils;

public class GmsCoreUtils {
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    private static final int GMSCORE_MIN_VERSION_GET_MATCHING_CRED_IDS = 223300000;
    private static final int GMSCORE_MIN_VERSION_HYBRID_API = 231206000;
    private static final int GMSCORE_MIN_VERSION_RESULT_RECEIVER = 240700000;
    static final int GMSCORE_MIN_VERSION = 16890000;

    private static int sGmsCorePackageVersion;

    public static int getGmsCoreVersion() {
        if (sGmsCorePackageVersion == 0) {
            sGmsCorePackageVersion = PackageUtils.getPackageVersion(GMSCORE_PACKAGE_NAME);
        }
        return sGmsCorePackageVersion;
    }

    /** Returns whether WebAuthn APIs are supported in GMSCore. */
    public static boolean isWebauthnSupported() {
        return getGmsCoreVersion() >= GMSCORE_MIN_VERSION;
    }

    /**
     * Returns whether or not the getMatchingCredentialIds API is supported. As the API is
     * flag-guarded inside of GMSCore, we can only provide a best-effort guess based on the GMSCore
     * version.
     */
    public static boolean isGetMatchingCredentialIdsSupported() {
        return getGmsCoreVersion() >= GMSCORE_MIN_VERSION_GET_MATCHING_CRED_IDS;
    }

    /** Returns whether the hybrid sign in API is supported. */
    public static boolean isHybridClientApiSupported() {
        return getGmsCoreVersion() >= GMSCORE_MIN_VERSION_HYBRID_API;
    }

    /** Returns whether makeCredential / getAssertion APIs support responding via ResultReceiver. */
    static boolean isResultReceiverSupported() {
        return getGmsCoreVersion() >= GMSCORE_MIN_VERSION_RESULT_RECEIVER;
    }
}
