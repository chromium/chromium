// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import android.location.Location;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Implements the Java side of LocationProviderAndroid. Delegates all real functionality to the
 * implementation returned from LocationProviderFactory. See detailed documentation on
 * content/browser/geolocation/location_api_adapter_android.h. Based on
 * android.webkit.GeolocationService.java
 */
@NullMarked
public class LocationProviderAdapter {
    private static final String TAG = "LocationProvider";

    // Delegate handling the real work in the main thread.
    private final LocationProvider mImpl;

    private LocationProviderAdapter() {
        mImpl = LocationProviderFactory.create();
    }

    @CalledByNative
    public static LocationProviderAdapter create() {
        return new LocationProviderAdapter();
    }

    /**
     * Start listening for location updates until we're told to quit. May be called in any thread.
     *
     * @param enableHighAccuracy Whether or not to enable high accuracy location providers.
     */
    @CalledByNative
    public void start(final boolean enableHighAccuracy) {
        ThreadUtils.runOnUiThread(() -> mImpl.start(enableHighAccuracy));
    }

    /** Stop listening for location updates. May be called in any thread. */
    @CalledByNative
    public void stop() {
        ThreadUtils.runOnUiThread(mImpl::stop);
    }

    /**
     * Returns true if we are currently listening for location updates, false if not.
     * Must be called only in the UI thread.
     */
    public boolean isRunning() {
        assert ThreadUtils.runningOnUiThread();
        return mImpl.isRunning();
    }

    public static void onNewLocationAvailable(Location location, boolean isPrecise) {
        LocationProviderAdapterJni.get()
                .newLocationAvailable(
                        location.getLatitude(),
                        location.getLongitude(),
                        location.getTime() / 1000.0,
                        location.hasAltitude(),
                        location.getAltitude(),
                        location.hasAccuracy(),
                        location.getAccuracy(),
                        location.hasBearing(),
                        location.getBearing(),
                        location.hasSpeed(),
                        location.getSpeed(),
                        isPrecise);
    }

    public static void newErrorAvailable(String message) {
        Log.e(TAG, "newErrorAvailable %s", message);
        LocationProviderAdapterJni.get().newErrorAvailable(message);
    }

    @NativeMethods
    interface Natives {
        void newLocationAvailable(
                double latitude,
                double longitude,
                double timeStamp,
                boolean hasAltitude,
                double altitude,
                boolean hasAccuracy,
                double accuracy,
                boolean hasHeading,
                double heading,
                boolean hasSpeed,
                double speed,
                boolean isPrecise);

        void newErrorAvailable(String message);
    }
}
