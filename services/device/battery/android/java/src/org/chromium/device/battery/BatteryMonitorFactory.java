// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.battery;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.device.battery.BatteryStatusManager.BatteryStatusCallback;
import org.chromium.device.mojom.BatteryMonitor;
import org.chromium.device.mojom.BatteryStatus;
import org.chromium.services.service_manager.InterfaceFactory;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Factory that creates instances of BatteryMonitor implementations and notifies them about battery
 * status changes.
 */
public class BatteryMonitorFactory implements InterfaceFactory<BatteryMonitor> {
    private static final String TAG = "BattMonitorFactory";

    // Backing source of battery information.
    private final BatteryStatusManager mManager;
    // Monitors currently interested in the battery status notifications.
    private final HashSet<BatteryMonitorImpl> mSubscribedMonitors =
            new HashSet<BatteryMonitorImpl>();
    // Tracks the latest battery status update for newly added observers.
    private boolean mHasStatusUpdate;
    private BatteryStatus mBatteryStatus;

    private final BatteryStatusCallback mCallback =
            new BatteryStatusCallback() {
                @Override
                public void onBatteryStatusChanged(BatteryStatus batteryStatus) {
                    ThreadUtils.assertOnUiThread();

                    mHasStatusUpdate = true;
                    mBatteryStatus = batteryStatus;

                    List<BatteryMonitorImpl> monitors = new ArrayList<>(mSubscribedMonitors);
                    for (BatteryMonitorImpl monitor : monitors) {
                        monitor.didChange(batteryStatus);
                    }
                }
            };

    public BatteryMonitorFactory() {
        mHasStatusUpdate = false;
        mManager = new BatteryStatusManager(mCallback);
    }

    @Override
    public BatteryMonitor createImpl() {
        ThreadUtils.assertOnUiThread();

        if (mSubscribedMonitors.isEmpty() && !mManager.start()) {
            Log.e(TAG, "BatteryStatusManager failed to start.");
        }

        BatteryMonitorImpl monitor = new BatteryMonitorImpl(this);
        if (mHasStatusUpdate) {
            monitor.didChange(mBatteryStatus);
        }

        mSubscribedMonitors.add(monitor);
        return monitor;
    }

    void unsubscribe(BatteryMonitorImpl monitor) {
        ThreadUtils.assertOnUiThread();

        assert mSubscribedMonitors.contains(monitor);
        mSubscribedMonitors.remove(monitor);
        if (mSubscribedMonitors.isEmpty()) {
            mManager.stop();
            mHasStatusUpdate = false;
        }
    }
}
