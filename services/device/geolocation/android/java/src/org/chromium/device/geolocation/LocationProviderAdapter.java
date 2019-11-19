// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import android.location.Location;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.util.concurrent.FutureTask;

/**
 * Implements the Java side of LocationProviderAndroid.
 * Delegates all real functionality to the implementation
 * returned from LocationProviderFactory.
 * See detailed documentation on
 * content/browser/geolocation/location_api_adapter_android.h.
 * Based on android.webkit.GeolocationService.java
 */
@VisibleForTesting
public class LocationProviderAdapter {
    private static final String TAG = "LocationProvider";

    // Delegate handling the real work in the main thread.
    private LocationProvider mImpl;

    private LocationProviderAdapter() {
        mImpl = LocationProviderFactory.create();
    }

    @CalledByNative
    public static LocationProviderAdapter create() {
        return new LocationProviderAdapter();
    }

    /**
     * Start listening for location updates until we're told to quit. May be called in any thread.
     * @param enableHighAccuracy Whether or not to enable high accuracy location providers.
     */
    @CalledByNative
    public void start(final boolean enableHighAccuracy) {
        FutureTask<Void> task = new FutureTask<Void>(new Runnable() {
            @Override
            public void run() {
                mImpl.start(enableHighAccuracy);
            }
        }, null);
        ThreadUtils.runOnUiThread(task);
    }

    /**
     * Stop listening for location updates. May be called in any thread.
     */
    @CalledByNative
    public void stop() {
        FutureTask<Void> task = new FutureTask<Void>(new Runnable() {
            @Override
            public void run() {
                mImpl.stop();
            }
        }, null);
        ThreadUtils.runOnUiThread(task);
    }

    /**
     * Returns true if we are currently listening for location updates, false if not.
     * Must be called only in the UI thread.
     */
    public boolean isRunning() {
        assert ThreadUtils.runningOnUiThread();
        return mImpl.isRunning();
    }

    public static void onNewLocationAvailable(Location location) {
        LocationProviderAdapterJni.get().newLocationAvailable(location.getLatitude(),
                location.getLongitude(), location.getTime() / 1000.0, location.hasAltitude(),
                location.getAltitude(), location.hasAccuracy(), location.getAccuracy(),
                location.hasBearing(), location.getBearing(), location.hasSpeed(),
                location.getSpeed());
    }

    public static void newErrorAvailable(String message) {
        Log.e(TAG, "newErrorAvailable %s", message);
        LocationProviderAdapterJni.get().newErrorAvailable(message);
    }

    @NativeMethods
    interface Natives {
        void newLocationAvailable(double latitude, double longitude, double timeStamp,
                boolean hasAltitude, double altitude, boolean hasAccuracy, double accuracy,
                boolean hasHeading, double heading, boolean hasSpeed, double speed);

        void newErrorAvailable(String message);
    }
}
