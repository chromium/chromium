// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;

import java.util.List;

/**
 * This is a LocationProvider using Android APIs [1]. It is a separate class for clarity
 * so that it can manage all processing completely on the UI thread. The container class
 * ensures that the start/stop calls into this class are done on the UI thread.
 *
 * [1] https://developer.android.com/reference/android/location/package-summary.html
 */
@NullMarked
public class LocationProviderAndroid implements LocationListener, LocationProvider {
    private static final String TAG = "LocationProvider";

    private @Nullable LocationManager mLocationManager;
    private boolean mIsRunning;
    private boolean mEffectiveHighAccuracy;
    private boolean mRequestedHighAccuracy;

    private final Context mContext;

    public LocationProviderAndroid(Context context) {
        mContext = context;
    }

    LocationProviderAndroid() {
        this(ContextUtils.getApplicationContext());
    }

    @Override
    public void start(boolean enableHighAccuracy) {
        ThreadUtils.assertOnUiThread();
        mRequestedHighAccuracy = enableHighAccuracy;
        mEffectiveHighAccuracy = mRequestedHighAccuracy;

        // Checking app-level permission here and override the `mEffectiveHighAccuracy`
        // so we can make sure `Geolocation.AndroidLocationProvider.` is logged with
        // correct name suffix.
        if (mContext.checkCallingOrSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            mEffectiveHighAccuracy = false;
        }

        unregisterFromLocationUpdates();
        registerForLocationUpdates();
    }

    @Override
    public void stop() {
        ThreadUtils.assertOnUiThread();
        unregisterFromLocationUpdates();
    }

    @Override
    public boolean isRunning() {
        ThreadUtils.assertOnUiThread();
        return mIsRunning;
    }

    @Override
    public void onLocationChanged(Location location) {
        // Callbacks from the system location service are queued to this thread, so it's
        // possible that we receive callbacks after unregistering. At this point, the
        // native object will no longer exist. Using `mRequestedHighAccuracy` for
        // location update because `mEffectiveHighAccuracy` can be overridden by app-level
        // permission check.
        if (mIsRunning) {
            if (location.hasAccuracy()) {
                final String histogramName =
                        "Geolocation.AndroidLocationProvider"
                                + (mEffectiveHighAccuracy
                                        ? ".HighAccuracyHint"
                                        : ".LowAccuracyHint")
                                + ".Accuracy";
                RecordHistogram.recordCount100000Histogram(
                        histogramName, (int) location.getAccuracy());
            }
            LocationProviderAdapter.onNewLocationAvailable(location, mRequestedHighAccuracy);
        }
    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras) {}

    @Override
    public void onProviderEnabled(String provider) {}

    @Override
    public void onProviderDisabled(String provider) {}

    public void setLocationManagerForTesting(LocationManager manager) {
        var oldValue = mLocationManager;
        mLocationManager = manager;
        ResettersForTesting.register(() -> mLocationManager = oldValue);
    }

    private void createLocationManagerIfNeeded() {
        if (mLocationManager != null) return;
        mLocationManager =
                (LocationManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.LOCATION_SERVICE);
    }

    /** Registers this object with the location service. */
    private void registerForLocationUpdates() {
        createLocationManagerIfNeeded();
        if (mLocationManager == null) {
            Log.e(TAG, "Could not get location manager.");
            return;
        }
        if (usePassiveOneShotLocation()) return;

        assert !mIsRunning;
        mIsRunning = true;

        // We're running on the main thread. The C++ side is responsible to
        // bounce notifications to the Geolocation thread as they arrive in the mainLooper.
        try {
            Criteria criteria = new Criteria();

            // When the `APPROXIMATE_GEOLOCATION_PERMISSION` feature is enabled,
            // `mEffectiveHighAccuracy` explicitly controls the location accuracy. Otherwise, it
            // only acts as a hint for the location provider.
            if (PermissionsAndroidFeatureMap.isEnabled(
                    PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)) {
                if (mEffectiveHighAccuracy) {
                    criteria.setAccuracy(Criteria.ACCURACY_FINE);
                } else {
                    criteria.setAccuracy(Criteria.ACCURACY_COARSE);
                }
            } else {
                if (mEffectiveHighAccuracy) {
                    criteria.setAccuracy(Criteria.ACCURACY_FINE);
                }
            }
            mLocationManager.requestLocationUpdates(
                    0, 0, criteria, this, ThreadUtils.getUiThreadLooper());
        } catch (SecurityException e) {
            Log.e(
                    TAG,
                    "Caught security exception while registering for location updates "
                            + "from the system. The application does not have sufficient "
                            + "geolocation permissions.");
            unregisterFromLocationUpdates();
            // Propagate an error to JavaScript, this can happen in case of WebView
            // when the embedding app does not have sufficient permissions.
            LocationProviderAdapter.newErrorAvailable(
                    "application does not have sufficient geolocation permissions.");
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Caught IllegalArgumentException registering for location updates.");
            unregisterFromLocationUpdates();
            assert false;
        }
    }

    /** Unregisters this object from the location service. */
    private void unregisterFromLocationUpdates() {
        if (!mIsRunning) return;
        mIsRunning = false;
        assumeNonNull(mLocationManager);
        mLocationManager.removeUpdates(this);
    }

    @RequiresNonNull("mLocationManager")
    private boolean usePassiveOneShotLocation() {
        if (!isOnlyPassiveLocationProviderEnabled()) {
            return false;
        }

        // Do not request a location update if the only available location provider is
        // the passive one. Make use of the last known location and call
        // onNewLocationAvailable directly.
        final Location location =
                mLocationManager.getLastKnownLocation(LocationManager.PASSIVE_PROVIDER);
        if (location != null) {
            ThreadUtils.assertOnUiThread();
            LocationProviderAdapter.onNewLocationAvailable(location, true);
        }
        return true;
    }

    /*
     * Checks if the passive location provider is the only provider available
     * in the system.
     */
    @RequiresNonNull("mLocationManager")
    private boolean isOnlyPassiveLocationProviderEnabled() {
        final List<String> providers = mLocationManager.getProviders(true);
        return providers != null
                && providers.size() == 1
                && providers.get(0).equals(LocationManager.PASSIVE_PROVIDER);
    }
}
