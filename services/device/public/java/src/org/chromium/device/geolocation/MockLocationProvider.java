// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

import android.location.Location;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;

/**
 * A mock location provider. When started, runs a background thread that periodically
 * posts location updates. This does not involve any system Location APIs and thus
 * does not require any special permissions in the test app or on the device.
 */
public class MockLocationProvider implements LocationProvider {
    private boolean mIsRunning;
    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private final Object mLock = new Object();

    private static final int UPDATE_LOCATION_MSG = 100;

    public MockLocationProvider() {}

    public void stopUpdates() {
        if (mHandlerThread != null) {
            mHandlerThread.quit();
        }
    }

    @Override
    public void start(boolean enableHighAccuracy) {
        if (mIsRunning) return;

        if (mHandlerThread == null) {
            startMockLocationProviderThread();
        }

        mIsRunning = true;
        synchronized (mLock) {
            mHandler.sendEmptyMessage(UPDATE_LOCATION_MSG);
        }
    }

    @Override
    public void stop() {
        if (!mIsRunning) return;
        mIsRunning = false;
        synchronized (mLock) {
            mHandler.removeMessages(UPDATE_LOCATION_MSG);
        }
    }

    @Override
    public boolean isRunning() {
        return mIsRunning;
    }

    private void startMockLocationProviderThread() {
        assert mHandlerThread == null;
        assert mHandler == null;

        mHandlerThread = new HandlerThread("MockLocationProviderImpl");
        mHandlerThread.start();
        mHandler =
                new Handler(mHandlerThread.getLooper()) {
                    @Override
                    public void handleMessage(Message msg) {
                        synchronized (mLock) {
                            if (msg.what == UPDATE_LOCATION_MSG) {
                                newLocation();
                                sendEmptyMessageDelayed(UPDATE_LOCATION_MSG, 250);
                            }
                        }
                    }
                };
    }

    private void newLocation() {
        Location location = new Location("MockLocationProvider");
        location.setTime(System.currentTimeMillis());
        location.setAccuracy(0.5f);
        LocationProviderAdapter.onNewLocationAvailable(location);
    }
}
