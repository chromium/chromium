// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.url.GURL;

/**
 * Handles passing registrations with Web Attribution Reporting API to the underlying native
 * library.
 */
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
    private void registerAttributionSource(
            GURL registrationUrl, GURL topLevelOrigin, boolean isDebugKeyAllowed) {
        // TODO(johnidel): Register with the Android API, see
        // https://developer.android.com/design-for-safety/privacy-sandbox/guides/attribution.
        // This is dependent on support for the Tiramisu Privacy Sandbox SDK.
    }

    @CalledByNative
    private void nativeDestroyed() {
        mNativePtr = 0;
    }
}
