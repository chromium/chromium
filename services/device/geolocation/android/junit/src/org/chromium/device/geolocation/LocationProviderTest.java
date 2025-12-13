// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Criteria;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Looper;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.Granularity;
import com.google.android.gms.location.LocationCallback;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.tasks.Tasks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;

import java.util.Arrays;
import java.util.Collection;

/** Test suite for Java Geolocation. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocationProviderTest {
    public enum LocationProviderType {
        MOCK,
        ANDROID,
        GMS_CORE
    }

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(
                new Object[][] {
                    {LocationProviderType.MOCK},
                    {LocationProviderType.ANDROID},
                    {LocationProviderType.GMS_CORE}
                });
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private LocationManager mLocationManager;

    private LocationProviderAdapter mLocationProviderAdapter;

    private FusedLocationProviderClient mFusedLocationProviderClient;

    private final LocationProviderType mApi;

    public LocationProviderTest(LocationProviderType api) {
        mApi = api;
    }

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
    }

    /** Verify a normal start/stop call pair with the given LocationProvider. */
    @Test
    @Feature({"Location"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testStartStop() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        setLocationProvider();

        createLocationProviderAdapter();
        startLocationProviderAdapter(/* enableHighAccuracy= */ false);
        stopLocationProviderAdapter();
    }

    /** Verify a start/upgrade/stop call sequencewith the given LocationProvider. */
    @Test
    @Feature({"Location"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testStartUpgradeStop() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        setLocationProvider();

        createLocationProviderAdapter();
        startLocationProviderAdapter(/* enableHighAccuracy= */ false);
        startLocationProviderAdapter(/* enableHighAccuracy= */ true);
        stopLocationProviderAdapter();
    }

    private void createLocationProviderAdapter() {
        mLocationProviderAdapter = LocationProviderAdapter.create();
        assertNotNull("LocationProvider", mLocationProviderAdapter);
    }

    private void setLocationProvider() {
        if (mApi == LocationProviderType.MOCK) {
            setLocationProviderMock();
        } else if (mApi == LocationProviderType.ANDROID) {
            setLocationProviderAndroid();
        } else if (mApi == LocationProviderType.GMS_CORE) {
            setLocationProviderGmsCore();
        } else {
            throw new AssertionError();
        }
    }

    private void setLocationProviderMock() {
        LocationProviderFactory.setLocationProviderImpl(new MockLocationProvider());
    }

    private void setLocationProviderAndroid() {
        Context context = Mockito.mock(Context.class);
        when(context.checkCallingOrSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        LocationProviderAndroid locationProviderAndroid = new LocationProviderAndroid(context);
        mLocationManager = Mockito.mock(LocationManager.class);
        locationProviderAndroid.setLocationManagerForTesting(mLocationManager);
        LocationProviderFactory.setLocationProviderImpl(locationProviderAndroid);
    }

    private void setLocationProviderGmsCore() {
        Context context = Mockito.mock(Context.class);
        mFusedLocationProviderClient = Mockito.mock(FusedLocationProviderClient.class);
        when(mFusedLocationProviderClient.requestLocationUpdates(
                        (LocationRequest) any(), (LocationCallback) any(), any()))
                .thenReturn(Tasks.forCanceled());

        LocationProviderGmsCore locationProviderGmsCore =
                new LocationProviderGmsCore(context, mFusedLocationProviderClient);

        LocationProviderFactory.setLocationProviderImpl(locationProviderGmsCore);
    }

    private void startLocationProviderAdapter(boolean enableHighAccuracy) {
        mLocationProviderAdapter.start(enableHighAccuracy);
        assertTrue("Should be running", mLocationProviderAdapter.isRunning());
    }

    private void stopLocationProviderAdapter() {
        mLocationProviderAdapter.stop();
        assertFalse("Should have stopped", mLocationProviderAdapter.isRunning());
    }

    /** Verify that the correct accuracy is requested with the feature enabled. */
    @Test
    @Feature({"Location"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testAccuracyRequestWithFeatureEnabled() {
        if (mApi == LocationProviderType.MOCK) {
            return;
        }

        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        setLocationProvider();
        createLocationProviderAdapter();

        startLocationProviderAdapter(/* enableHighAccuracy= */ false);
        if (mApi == LocationProviderType.ANDROID) {
            ArgumentCaptor<Criteria> captor = ArgumentCaptor.forClass(Criteria.class);
            verify(mLocationManager)
                    .requestLocationUpdates(
                            anyLong(),
                            anyFloat(),
                            captor.capture(),
                            any(LocationListener.class),
                            any(Looper.class));
            assertEquals(Criteria.ACCURACY_COARSE, captor.getValue().getAccuracy());
        } else if (mApi == LocationProviderType.GMS_CORE) {
            ArgumentCaptor<LocationRequest> captor = ArgumentCaptor.forClass(LocationRequest.class);
            verify(mFusedLocationProviderClient)
                    .requestLocationUpdates(captor.capture(), any(LocationCallback.class), any());
            assertEquals(Granularity.GRANULARITY_COARSE, captor.getValue().getGranularity());
        }

        startLocationProviderAdapter(/* enableHighAccuracy= */ true);
        if (mApi == LocationProviderType.ANDROID) {
            ArgumentCaptor<Criteria> captor = ArgumentCaptor.forClass(Criteria.class);
            verify(mLocationManager, times(2))
                    .requestLocationUpdates(
                            anyLong(),
                            anyFloat(),
                            captor.capture(),
                            any(LocationListener.class),
                            any(Looper.class));
            assertEquals(Criteria.ACCURACY_FINE, captor.getValue().getAccuracy());
        } else if (mApi == LocationProviderType.GMS_CORE) {
            ArgumentCaptor<LocationRequest> captor = ArgumentCaptor.forClass(LocationRequest.class);
            verify(mFusedLocationProviderClient, times(2))
                    .requestLocationUpdates(captor.capture(), any(LocationCallback.class), any());
            assertEquals(Granularity.GRANULARITY_FINE, captor.getValue().getGranularity());
        }
        stopLocationProviderAdapter();
    }
}
