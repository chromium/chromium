// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Factory to create a LocationProvider to allow us to inject a mock for tests. */
@JNINamespace("device")
@NullMarked
public class LocationProviderFactory {
    private static @Nullable LocationProvider sProviderImpl;
    private static boolean sUseGmsCoreLocationProvider;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. This enum is tied to the
    // AndroidLocationProviderType enum in tools/metrics/histograms/metadata/geolocation/enums.xml.
    @IntDef({
        LocationProviderType.ANDROID,
        LocationProviderType.GMS_CORE,
        LocationProviderType.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LocationProviderType {
        int ANDROID = 0;
        int GMS_CORE = 1;

        /** Total count of entries. */
        int COUNT = 2;
    }

    private LocationProviderFactory() {}

    @VisibleForTesting
    public static void setLocationProviderImpl(LocationProvider provider) {
        sProviderImpl = provider;
    }

    @CalledByNative
    public static void useGmsCoreLocationProvider() {
        sUseGmsCoreLocationProvider = true;
    }

    public static LocationProvider create() {
        if (sProviderImpl != null) return sProviderImpl;

        if (sUseGmsCoreLocationProvider
                && LocationProviderGmsCore.isGooglePlayServicesAvailable(
                        ContextUtils.getApplicationContext())) {
            sProviderImpl = new LocationProviderGmsCore(ContextUtils.getApplicationContext());
            RecordHistogram.recordEnumeratedHistogram(
                    "Geolocation.AndroidLocationProvider.ProviderType",
                    LocationProviderType.GMS_CORE,
                    LocationProviderType.COUNT);

        } else {
            sProviderImpl = new LocationProviderAndroid(ContextUtils.getApplicationContext());
            RecordHistogram.recordEnumeratedHistogram(
                    "Geolocation.AndroidLocationProvider.ProviderType",
                    LocationProviderType.ANDROID,
                    LocationProviderType.COUNT);
        }
        return sProviderImpl;
    }
}
