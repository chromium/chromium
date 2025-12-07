// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Location;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.Granularity;
import com.google.android.gms.location.LocationCallback;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationResult;
import com.google.android.gms.location.LocationServices;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.device.DeviceFeatureList;
import org.chromium.gms.ChromiumPlayServicesAvailability;

/**
 * This is a LocationProvider using Google Play Services.
 *
 * <p>https://developers.google.com/android/reference/com/google/android/gms/location/package-summary
 */
@NullMarked
public class LocationProviderGmsCore implements LocationProvider {
    private static final String TAG = "LocationProvider";

    // Values for the LocationRequest's setInterval for normal and high accuracy, respectively.
    private static final long UPDATE_INTERVAL_MS = 1000;
    private static final long UPDATE_INTERVAL_FAST_MS = 500;

    private final Context mContext;
    private final FusedLocationProviderClient mClient;
    private boolean mEffectiveHighAccuracy;
    private boolean mRequestedHighAccuracy;

    private @Nullable LocationCallback mLocationCallback;

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
        mRequestedHighAccuracy = enableHighAccuracy;
        mEffectiveHighAccuracy = mRequestedHighAccuracy;
        if (mContext.checkCallingOrSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            // Workaround for a bug in Google Play Services where, if an app only has
            // ACCESS_COARSE_LOCATION, trying to request PRIORITY_HIGH_ACCURACY will throw a
            // SecurityException even on Android S. See: b/184924939.
            mEffectiveHighAccuracy = false;
        }
        LocationRequest locationRequest;
        final long interval = mEffectiveHighAccuracy ? UPDATE_INTERVAL_FAST_MS : UPDATE_INTERVAL_MS;
        final int priority =
                mEffectiveHighAccuracy
                        ? LocationRequest.PRIORITY_HIGH_ACCURACY
                        : LocationRequest.PRIORITY_BALANCED_POWER_ACCURACY;
        // When the `APPROXIMATE_GEOLOCATION_PERMISSION` feature is enabled,
        // `mEffectiveHighAccuracy`
        // explicitly controls the location granularity. Otherwise, it only acts as a hint for the
        // location provider.
        if (PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)) {
            LocationRequest.Builder builder =
                    new LocationRequest.Builder(
                            /* priority= */ priority, /* intervalMillis= */ interval);
            if (mEffectiveHighAccuracy) {
                builder.setGranularity(Granularity.GRANULARITY_FINE);
            } else {
                builder.setGranularity(Granularity.GRANULARITY_COARSE);
            }
            locationRequest = builder.build();
        } else {
            locationRequest = LocationRequest.create();
            locationRequest.setPriority(priority).setInterval(interval);
        }

        if (DeviceFeatureList.sGmsCoreLocationRequestParamOverride.isEnabled()) {
            locationRequest =
                    new LocationRequest.Builder(locationRequest)
                            .setIntervalMillis(
                                    DeviceFeatureList.sGmsCoreLocationRequestUpdateInterval
                                            .getValue())
                            .setMaxUpdateAgeMillis(
                                    DeviceFeatureList.sGmsCoreLocationRequestMaxLocationAge
                                            .getValue())
                            .build();
        }

        stop();
        mLocationCallback =
                new LocationCallback() {
                    @Override
                    public void onLocationResult(LocationResult locationResult) {
                        if (locationResult == null) {
                            return;
                        }
                        Location location = locationResult.getLastLocation();
                        if (location != null) {
                            if (location.hasAccuracy()) {
                                final String histogramName =
                                        "Geolocation.GMSCoreLocationProvider"
                                                + (mEffectiveHighAccuracy
                                                        ? ".HighAccuracyHint"
                                                        : ".LowAccuracyHint")
                                                + ".Accuracy";
                                RecordHistogram.recordCount100000Histogram(
                                        histogramName, (int) location.getAccuracy());
                            }
                            // Using `mRequestedHighAccuracy` for location update cause
                            // `mEffectiveHighAccuracy` can be override by app-level permission
                            // check.
                            LocationProviderAdapter.onNewLocationAvailable(
                                    location, mRequestedHighAccuracy);
                        }
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

        if (mLocationCallback != null) {
            mClient.removeLocationUpdates(mLocationCallback);
            mLocationCallback = null;
        }
    }

    @Override
    public boolean isRunning() {
        assert ThreadUtils.runningOnUiThread();
        return mLocationCallback != null;
    }
}
