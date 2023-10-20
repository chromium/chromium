// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.location.LocationManager;

import com.google.android.gms.location.FusedLocationProviderClient;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLocationManager;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Feature;

import java.util.Arrays;
import java.util.Collection;

/** Test suite for Java Geolocation. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocationProviderTest {
    public static enum LocationProviderType {
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
    private ShadowLocationManager mShadowLocationManager;

    private LocationProviderAdapter mLocationProviderAdapter;

    private final LocationProviderType mApi;

    public LocationProviderTest(LocationProviderType api) {
        mApi = api;
    }

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        mLocationManager = RuntimeEnvironment.application.getSystemService(LocationManager.class);
    }

    /** Verify a normal start/stop call pair with the given LocationProvider. */
    @Test
    @Feature({"Location"})
    public void testStartStop() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        setLocationProvider();

        createLocationProviderAdapter();
        startLocationProviderAdapter(false);
        stopLocationProviderAdapter();
    }

    /** Verify a start/upgrade/stop call sequencewith the given LocationProvider. */
    @Test
    @Feature({"Location"})
    public void testStartUpgradeStop() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        setLocationProvider();

        createLocationProviderAdapter();
        startLocationProviderAdapter(false);
        startLocationProviderAdapter(true);
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
            assert false;
        }
    }

    private void setLocationProviderMock() {
        LocationProviderFactory.setLocationProviderImpl(new MockLocationProvider());
    }

    private void setLocationProviderAndroid() {
        LocationProviderAndroid locationProviderAndroid = new LocationProviderAndroid();

        // Robolectric has a ShadowLocationManager class that mocks the behaviour of the real
        // class very closely. Use it here.
        mShadowLocationManager = Shadows.shadowOf(mLocationManager);
        locationProviderAndroid.setLocationManagerForTesting(mLocationManager);
        LocationProviderFactory.setLocationProviderImpl(locationProviderAndroid);
    }

    private void setLocationProviderGmsCore() {
        Context context = Mockito.mock(Context.class);
        FusedLocationProviderClient fusedLocationProviderClient =
                Mockito.mock(FusedLocationProviderClient.class);

        LocationProviderGmsCore locationProviderGmsCore =
                new LocationProviderGmsCore(context, fusedLocationProviderClient);

        LocationProviderFactory.setLocationProviderImpl(locationProviderGmsCore);
    }

    private void startLocationProviderAdapter(boolean highResolution) {
        mLocationProviderAdapter.start(highResolution);
        assertTrue("Should be running", mLocationProviderAdapter.isRunning());
    }

    private void stopLocationProviderAdapter() {
        mLocationProviderAdapter.stop();
        assertFalse("Should have stopped", mLocationProviderAdapter.isRunning());
    }
}
