// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.MotionEvent;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/**
 * Handles passing registrations with Web Attribution Reporting API to the underlying native
 * library.
 */
@JNINamespace("content")
public class AttributionOsLevelManager {
    private long mNativePtr;

    @CalledByNative
    private AttributionOsLevelManager(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /**
     * Registers a web attribution source with native, see `registerWebSource()`:
     * https://developer.android.com/design-for-safety/privacy-sandbox/reference/adservices/measurement/MeasurementManager.
     */
    @CalledByNative
    private void registerAttributionSource(GURL registrationUrl, GURL topLevelOrigin,
            boolean isDebugKeyAllowed, MotionEvent event) {
        // TODO(johnidel): Register with the Android API, see
        // https://developer.android.com/design-for-safety/privacy-sandbox/guides/attribution.
        // This is dependent on support for the Tiramisu Privacy Sandbox SDK.
    }

    /**
     * Registers a web attribution trigger with native, see `registerWebTrigger()`:
     * https://developer.android.com/design-for-safety/privacy-sandbox/reference/adservices/measurement/MeasurementManager.
     */
    @CalledByNative
    private void registerAttributionTrigger(
            GURL registrationUrl, GURL topLevelOrigin, boolean isDebugKeyAllowed) {
        // TODO(johnidel): Register with the Android API, see
        // https://developer.android.com/design-for-safety/privacy-sandbox/guides/attribution.
        // This is dependent on support for the Tiramisu Privacy Sandbox SDK.
    }

    /**
     * Deletes attribution data with native, see `deleteRegistrations()`:
     * https://developer.android.com/design-for-safety/privacy-sandbox/reference/adservices/measurement/MeasurementManager.
     */
    @CalledByNative
    private void deleteRegistrations(int requestId, long startMs, long endMs, GURL[] origins,
            String[] domains, int deletionMode, int matchBehavior) {
        // TODO(linnan): Delete registrations with the Android API, see
        // https://developer.android.com/design-for-safety/privacy-sandbox/guides/attribution.
        // This is dependent on support for the Tiramisu Privacy Sandbox SDK.
        if (mNativePtr != 0) {
            AttributionOsLevelManagerJni.get().onDataDeletionCompleted(mNativePtr, requestId);
        }
    }

    /**
     * Gets Measurement API status with native, see `getMeasurementApiStatus()`:
     * https://developer.android.com/design-for-safety/privacy-sandbox/reference/adservices/measurement/MeasurementManager.
     */
    @CalledByNative
    private void getMeasurementApiStatus() {
        // TODO(linnan):  Get from Android API, see
        // https://developer.android.com/design-for-safety/privacy-sandbox/guides/attribution.
        // This is dependent on support for the Tiramisu Privacy Sandbox SDK.
        AttributionOsLevelManagerJni.get().onMeasurementStateReturned(0);
    }

    @CalledByNative
    private void nativeDestroyed() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void onDataDeletionCompleted(long nativeAttributionOsLevelManagerAndroid, int requestId);
        void onMeasurementStateReturned(int state);
    }
}
