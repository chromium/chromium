// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.chromium.base.PackageUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

@NullMarked
public class GmsCoreUtils {
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    private static final int GMSCORE_MIN_VERSION_GET_MATCHING_CRED_IDS = 223300000;
    private static final int GMSCORE_MIN_VERSION_HYBRID_API = 231206000;
    private static final int GMSCORE_MIN_VERSION_PASSKEY_CACHE = 244400000;
    private static final int GMSCORE_MIN_VERSION_RESULT_RECEIVER = 240700000;
    // This version is the minimum needed for dynamic lookup of services, which
    // the persistent API requires.
    static final int GMSCORE_MIN_VERSION_DYNAMIC_LOOKUP = 17895000;
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

    /** Returns whether the passkey cache is supported. */
    public static boolean isPasskeyCacheSupported() {
        return getGmsCoreVersion() >= GMSCORE_MIN_VERSION_PASSKEY_CACHE;
    }

    public static void setGmsCoreVersionForTesting(int version) {
        sGmsCorePackageVersion = version;
    }

    /**
     * Returns a SuccessListener callback that wraps another SuccessListener callback, but posts the
     * inner callback's invocation to the UI thread to avoid contention over state. Intent callbacks
     * can be run on a different thread from the one on which they were originally invoked.
     */
    public static <T> OnSuccessListener<T> wrapSuccessCallback(OnSuccessListener<T> callback) {
        return (result) -> {
            PostTask.runOrPostTask(
                    TaskTraits.UI_USER_VISIBLE,
                    () -> {
                        callback.onSuccess(result);
                    });
        };
    }

    /**
     * Returns a FailureListener callback that wraps another FailureListener callback, but posts the
     * inner callback's invocation to the UI thread to avoid contention over state. Intent callbacks
     * can be run on a different thread from the one on which they were originally invoked.
     */
    public static OnFailureListener wrapFailureCallback(OnFailureListener callback) {
        // Post callbacks to UI thread to avoid contention over state, since this can be called
        // on a different thread than was originally used to invoke the Intent.
        return (e) -> {
            PostTask.runOrPostTask(
                    TaskTraits.UI_USER_VISIBLE,
                    () -> {
                        callback.onFailure(e);
                    });
        };
    }
}
