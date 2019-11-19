// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import android.content.Context;
import android.location.Location;
import android.os.Bundle;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.GoogleApiClient.ConnectionCallbacks;
import com.google.android.gms.common.api.GoogleApiClient.OnConnectionFailedListener;
import com.google.android.gms.location.FusedLocationProviderApi;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationServices;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.location.LocationUtils;

/**
 * This is a LocationProvider using Google Play Services.
 *
 * https://developers.google.com/android/reference/com/google/android/gms/location/package-summary
 */
public class LocationProviderGmsCore implements ConnectionCallbacks, OnConnectionFailedListener,
                                                LocationListener, LocationProvider {
    private static final String TAG = "LocationProvider";

    // Values for the LocationRequest's setInterval for normal and high accuracy, respectively.
    private static final long UPDATE_INTERVAL_MS = 1000;
    private static final long UPDATE_INTERVAL_FAST_MS = 500;

    private final GoogleApiClient mGoogleApiClient;
    private FusedLocationProviderApi mLocationProviderApi = LocationServices.FusedLocationApi;

    private boolean mEnablehighAccuracy;
    private LocationRequest mLocationRequest;

    public static boolean isGooglePlayServicesAvailable(Context context) {
        return GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context)
                == ConnectionResult.SUCCESS;
    }

    LocationProviderGmsCore(Context context) {
        Log.i(TAG, "Google Play Services");
        mGoogleApiClient = new GoogleApiClient.Builder(context)
                                   .addApi(LocationServices.API)
                                   .addConnectionCallbacks(this)
                                   .addOnConnectionFailedListener(this)
                                   .build();
        assert mGoogleApiClient != null;
    }

    LocationProviderGmsCore(GoogleApiClient client, FusedLocationProviderApi locationApi) {
        mGoogleApiClient = client;
        mLocationProviderApi = locationApi;
    }

    // ConnectionCallbacks implementation
    @Override
    public void onConnected(Bundle connectionHint) {
        ThreadUtils.assertOnUiThread();

        mLocationRequest = LocationRequest.create();
        if (mEnablehighAccuracy) {
            // With enableHighAccuracy, request a faster update interval and configure the provider
            // for high accuracy mode.
            mLocationRequest.setPriority(LocationRequest.PRIORITY_HIGH_ACCURACY)
                    .setInterval(UPDATE_INTERVAL_FAST_MS);
        } else {
            // Use balanced mode by default. In this mode, the API will prefer the network provider
            // but may use sensor data (for instance, GPS) if high accuracy is requested by another
            // app.
            //
            // If location is configured for sensors-only then elevate the priority to ensure GPS
            // and other sensors are used.
            if (LocationUtils.getInstance().isSystemLocationSettingSensorsOnly()) {
                mLocationRequest.setPriority(LocationRequest.PRIORITY_HIGH_ACCURACY);
            } else {
                mLocationRequest.setPriority(LocationRequest.PRIORITY_BALANCED_POWER_ACCURACY);
            }
            mLocationRequest.setInterval(UPDATE_INTERVAL_MS);
        }

        final Location location = mLocationProviderApi.getLastLocation(mGoogleApiClient);
        if (location != null) {
            LocationProviderAdapter.onNewLocationAvailable(location);
        }

        try {
            // Request updates on UI Thread replicating LocationProviderAndroid's behaviour.
            mLocationProviderApi.requestLocationUpdates(
                    mGoogleApiClient, mLocationRequest, this, ThreadUtils.getUiThreadLooper());
        } catch (IllegalStateException | SecurityException e) {
            // IllegalStateException is thrown "If this method is executed in a thread that has not
            // called Looper.prepare()". SecurityException is thrown if there is no permission, see
            // https://crbug.com/731271.
            Log.e(TAG, " mLocationProviderApi.requestLocationUpdates() " + e);
            LocationProviderAdapter.newErrorAvailable(
                    "Failed to request location updates: " + e.toString());
            assert false;
        }
    }

    @Override
    public void onConnectionSuspended(int cause) {}

    // OnConnectionFailedListener implementation
    @Override
    public void onConnectionFailed(ConnectionResult result) {
        LocationProviderAdapter.newErrorAvailable(
                "Failed to connect to Google Play Services: " + result.toString());
    }

    // LocationProvider implementation
    @Override
    public void start(boolean enableHighAccuracy) {
        ThreadUtils.assertOnUiThread();
        if (mGoogleApiClient.isConnected()) mGoogleApiClient.disconnect();

        mEnablehighAccuracy = enableHighAccuracy;
        mGoogleApiClient.connect(); // Should return via onConnected().
    }

    @Override
    public void stop() {
        ThreadUtils.assertOnUiThread();
        if (!mGoogleApiClient.isConnected()) return;

        mLocationProviderApi.removeLocationUpdates(mGoogleApiClient, this);

        mGoogleApiClient.disconnect();
    }

    @Override
    public boolean isRunning() {
        assert ThreadUtils.runningOnUiThread();
        if (mGoogleApiClient == null) return false;
        return mGoogleApiClient.isConnecting() || mGoogleApiClient.isConnected();
    }

    // LocationListener implementation
    @Override
    public void onLocationChanged(Location location) {
        LocationProviderAdapter.onNewLocationAvailable(location);
    }
}
