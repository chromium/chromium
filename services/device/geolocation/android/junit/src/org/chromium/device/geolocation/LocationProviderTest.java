// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;

import android.location.LocationManager;
import android.os.Build;

import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.location.FusedLocationProviderApi;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLocationManager;
import org.robolectric.shadows.ShadowLog; // remove me ?

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

import java.util.Arrays;
import java.util.Collection;

/**
 * Test suite for Java Geolocation.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.M, manifest = Config.NONE)
public class LocationProviderTest {
    static {
        // Setting robolectric.offline which tells Robolectric to look for runtime dependency
        // JARs from a local directory and to not download them from Maven.
        System.setProperty("robolectric.offline", "true");
    }

    public static enum LocationProviderType { MOCK, ANDROID, GMS_CORE }

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{LocationProviderType.MOCK},
                {LocationProviderType.ANDROID}, {LocationProviderType.GMS_CORE}});
    }

    // Member variables for LocationProviderType.GMS_CORE case.
    @Mock
    private GoogleApiClient mGoogleApiClient;
    private boolean mGoogleApiClientIsConnected;

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

    /**
     * Verify a normal start/stop call pair with the given LocationProvider.
     */
    @Test
    @Feature({"Location"})
    public void testStartStop() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        setLocationProvider();

        createLocationProviderAdapter();
        startLocationProviderAdapter(false);
        stopLocationProviderAdapter();
    }

    /**
     * Verify a start/upgrade/stop call sequencewith the given LocationProvider.
     */
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
        // Robolectric has a ShadowGoogleApiClientBuilder class that mocks the behaviour of the real
        // class very closely, but it's not available in our build
        mGoogleApiClient = Mockito.mock(GoogleApiClient.class);
        mGoogleApiClientIsConnected = false;
        doAnswer(new Answer<Boolean>() {
            @Override
            public Boolean answer(InvocationOnMock invocation) {
                return mGoogleApiClientIsConnected;
            }
        })
                .when(mGoogleApiClient)
                .isConnected();

        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mGoogleApiClientIsConnected = true;
                return null;
            }
        })
                .when(mGoogleApiClient)
                .connect();

        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mGoogleApiClientIsConnected = false;
                return null;
            }
        })
                .when(mGoogleApiClient)
                .disconnect();

        FusedLocationProviderApi fusedLocationProviderApi =
                Mockito.mock(FusedLocationProviderApi.class);

        LocationProviderGmsCore locationProviderGmsCore =
                new LocationProviderGmsCore(mGoogleApiClient, fusedLocationProviderApi);

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
