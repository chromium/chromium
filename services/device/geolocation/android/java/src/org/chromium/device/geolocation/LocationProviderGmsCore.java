// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.LocationCallback;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationResult;
import com.google.android.gms.location.LocationServices;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.gms.ChromiumPlayServicesAvailability;

/**
 * This is a LocationProvider using Google Play Services.
 *
 * https://developers.google.com/android/reference/com/google/android/gms/location/package-summary
 */
public class LocationProviderGmsCore implements LocationProvider {
    private static final String TAG = "LocationProvider";

    // Values for the LocationRequest's setInterval for normal and high accuracy, respectively.
    private static final long UPDATE_INTERVAL_MS = 1000;
    private static final long UPDATE_INTERVAL_FAST_MS = 500;

    private final Context mContext;
    private final FusedLocationProviderClient mClient;

    private LocationCallback mLocationCallback;

    public static boolean isGooglePlayServicesAvailable(Context context) {
        return ChromiumPlayServicesAvailability.isGooglePlayServicesAvailable(context);
    }

    LocationProviderGmsCore(Context context) {
        Log.i(TAG, "Google Play Services");
        mContext = context;
        mClient = LocationServices.getFusedLocationProviderClient(context);
        assert mClient != null;
    }

    LocationProviderGmsCore(Context context, FusedLocationProviderClient client) {
        mContext = context;
        mClient = client;
    }

    // LocationProvider implementation
    @Override
    public void start(boolean enableHighAccuracy) {
        ThreadUtils.assertOnUiThread();

        LocationRequest locationRequest = LocationRequest.create();
        if (mContext.checkCallingOrSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            // Workaround for a bug in Google Play Services where, if an app only has
            // ACCESS_COARSE_LOCATION, trying to request PRIORITY_HIGH_ACCURACY will throw a
            // SecurityException even on Android S. See: b/184924939.
            enableHighAccuracy = false;
        }

        if (enableHighAccuracy) {
            // With enableHighAccuracy, request a faster update interval and configure the provider
            // for high accuracy mode.
            locationRequest
                    .setPriority(LocationRequest.PRIORITY_HIGH_ACCURACY)
                    .setInterval(UPDATE_INTERVAL_FAST_MS);
        } else {
            // Use balanced mode by default. In this mode, the API will prefer the network provider
            // but may use sensor data (for instance, GPS) if high accuracy is requested by another
            // app.
            locationRequest
                    .setPriority(LocationRequest.PRIORITY_BALANCED_POWER_ACCURACY)
                    .setInterval(UPDATE_INTERVAL_MS);
        }

        if (mLocationCallback != null) {
            mClient.removeLocationUpdates(mLocationCallback);
        }

        mLocationCallback =
                new LocationCallback() {
                    @Override
                    public void onLocationResult(LocationResult locationResult) {
                        if (locationResult == null) {
                            return;
                        }
                        LocationProviderAdapter.onNewLocationAvailable(
                                locationResult.getLastLocation());
                    }
                };

        try {
            // Request updates on UI Thread replicating LocationProviderAndroid's behaviour.
            mClient.requestLocationUpdates(
                            locationRequest, mLocationCallback, ThreadUtils.getUiThreadLooper())
                    .addOnFailureListener(
                            (e) -> {
                                Log.e(TAG, "mClient.requestLocationUpdates() " + e);
                                LocationProviderAdapter.newErrorAvailable(
                                        "Failed to request location updates: " + e.toString());
                            });
        } catch (IllegalStateException e) {
            // IllegalStateException is thrown "If this method is executed in a thread that has not
            // called Looper.prepare()".
            Log.e(TAG, "mClient.requestLocationUpdates() " + e);
            LocationProviderAdapter.newErrorAvailable(
                    "Failed to request location updates: " + e.toString());
            assert false;
        } catch (SecurityException e) {
            // SecurityException is thrown when the app is missing location permissions. See
            // crbug.com/731271.
            Log.e(TAG, "mClient.requestLocationUpdates() missing permissions " + e);
            LocationProviderAdapter.newErrorAvailable(
                    "Failed to request location updates due to permissions: " + e.toString());
        }
    }

    @Override
    public void stop() {
        ThreadUtils.assertOnUiThread();

        mClient.removeLocationUpdates(mLocationCallback);
        mLocationCallback = null;
    }

    @Override
    public boolean isRunning() {
        assert ThreadUtils.runningOnUiThread();
        return mLocationCallback != null;
    }
}
