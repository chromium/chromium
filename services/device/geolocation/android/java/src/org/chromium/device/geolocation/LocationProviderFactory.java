// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/** Factory to create a LocationProvider to allow us to inject a mock for tests. */
@JNINamespace("device")
public class LocationProviderFactory {
    private static LocationProvider sProviderImpl;
    private static boolean sUseGmsCoreLocationProvider;

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
        } else {
            sProviderImpl = new LocationProviderAndroid();
        }
        return sProviderImpl;
    }
}
