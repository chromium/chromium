// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.battery;

import org.chromium.base.Log;
import org.chromium.device.mojom.BatteryMonitor;
import org.chromium.device.mojom.BatteryStatus;
import org.chromium.mojo.system.MojoException;

/**
 * Android implementation of the battery monitor interface defined in
 * services/device/public/mojom/battery_monitor.mojom.
 */
public class BatteryMonitorImpl implements BatteryMonitor {
    private static final String TAG = "BatteryMonitorImpl";

    // Factory that created this instance and notifies it about battery status changes.
    private final BatteryMonitorFactory mFactory;
    private QueryNextStatus_Response mCallback;
    private BatteryStatus mStatus;
    private boolean mHasStatusToReport;
    private boolean mSubscribed;

    public BatteryMonitorImpl(BatteryMonitorFactory batteryMonitorFactory) {
        mFactory = batteryMonitorFactory;
        mHasStatusToReport = false;
        mSubscribed = true;
    }

    private void unsubscribe() {
        if (mSubscribed) {
            mFactory.unsubscribe(this);
            mSubscribed = false;
        }
    }

    @Override
    public void close() {
        unsubscribe();
    }

    @Override
    public void onConnectionError(MojoException e) {
        unsubscribe();
    }

    @Override
    public void queryNextStatus(QueryNextStatus_Response callback) {
        if (mCallback != null) {
            Log.e(TAG, "Overlapped call to queryNextStatus!");
            unsubscribe();
            return;
        }

        mCallback = callback;

        if (mHasStatusToReport) {
            reportStatus();
        }
    }

    void didChange(BatteryStatus batteryStatus) {
        mStatus = batteryStatus;
        mHasStatusToReport = true;

        if (mCallback != null) {
            reportStatus();
        }
    }

    void reportStatus() {
        mCallback.call(mStatus);
        mCallback = null;
        mHasStatusToReport = false;
    }
}
