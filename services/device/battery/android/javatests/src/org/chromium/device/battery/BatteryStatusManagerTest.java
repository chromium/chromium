// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.battery;

import android.content.Intent;
import android.os.BatteryManager;
import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.device.mojom.BatteryStatus;

/**
 * Test suite for BatteryStatusManager.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BatteryStatusManagerTest {
    // Values reported in the most recent callback from |mManager|.
    private boolean mCharging = false;
    private double mChargingTime = 0;
    private double mDischargingTime = 0;
    private double mLevel = 0;

    private BatteryStatusManager.BatteryStatusCallback mCallback =
            new BatteryStatusManager.BatteryStatusCallback() {
                @Override
                public void onBatteryStatusChanged(BatteryStatus batteryStatus) {
                    mCharging = batteryStatus.charging;
                    mChargingTime = batteryStatus.chargingTime;
                    mDischargingTime = batteryStatus.dischargingTime;
                    mLevel = batteryStatus.level;
                }
            };

    private BatteryStatusManager mManager;

    private void verifyValues(
            boolean charging, double chargingTime, double dischargingTime, double level) {
        Assert.assertEquals(charging, mCharging);
        Assert.assertEquals(chargingTime, mChargingTime);
        Assert.assertEquals(dischargingTime, mDischargingTime);
        Assert.assertEquals(level, mLevel);
    }

    private static class FakeAndroidBatteryManager
            extends BatteryStatusManager.AndroidBatteryManagerWrapper {
        private int mChargeCounter;
        private int mCapacity;
        private int mAverageCurrent;

        private FakeAndroidBatteryManager() {
            super(null);
        }

        @Override
        public int getIntProperty(int id) {
            switch (id) {
                case BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER:
                    return mChargeCounter;
                case BatteryManager.BATTERY_PROPERTY_CAPACITY:
                    return mCapacity;
                case BatteryManager.BATTERY_PROPERTY_CURRENT_AVERAGE:
                    return mAverageCurrent;
            }
            Assert.fail();
            return 0;
        }

        public FakeAndroidBatteryManager setIntProperty(int id, int value) {
            switch (id) {
                case BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER:
                    mChargeCounter = value;
                    return this;
                case BatteryManager.BATTERY_PROPERTY_CAPACITY:
                    mCapacity = value;
                    return this;
                case BatteryManager.BATTERY_PROPERTY_CURRENT_AVERAGE:
                    mAverageCurrent = value;
                    return this;
            }
            Assert.fail();
            return this;
        }
    }

    @Before
    public void setUp() throws Exception {
        initializeBatteryManager(null);
    }

    public void initializeBatteryManager(FakeAndroidBatteryManager managerForTesting) {
        mManager = BatteryStatusManager.createBatteryStatusManagerForTesting(
                InstrumentationRegistry.getContext(), mCallback, managerForTesting);
    }

    @Test
    @SmallTest
    public void testOnReceiveBatteryNotPluggedIn() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, 0);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 10);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        mManager.onReceive(intent);
        verifyValues(false, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, 0.1);
    }

    @Test
    @SmallTest
    public void testOnReceiveBatteryPluggedInACCharging() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_AC);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_CHARGING);

        mManager.onReceive(intent);
        verifyValues(true, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, 0.5);
    }

    @Test
    @SmallTest
    public void testOnReceiveBatteryPluggedInACNotCharging() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_AC);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        mManager.onReceive(intent);
        verifyValues(true, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, 0.5);
    }

    @Test
    @SmallTest
    public void testOnReceiveBatteryPluggedInUSBFull() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 100);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_FULL);

        mManager.onReceive(intent);
        verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @Test
    @SmallTest
    public void testOnReceiveNoBattery() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, false);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);

        mManager.onReceive(intent);
        verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @Test
    @SmallTest
    public void testOnReceiveNoPluggedStatus() {
        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);

        mManager.onReceive(intent);
        verifyValues(true, 0, Double.POSITIVE_INFINITY, 1);
    }

    @Test
    @SmallTest
    public void testStartStopSucceeds() {
        Assert.assertTrue(mManager.start());
        mManager.stop();
    }

    @Test
    @SmallTest
    public void testLollipopChargingTimeEstimate() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, BatteryManager.BATTERY_PLUGGED_USB);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);

        initializeBatteryManager(
                new FakeAndroidBatteryManager()
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER, 1000)
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY, 50)
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_AVERAGE, 100));

        mManager.onReceive(intent);
        verifyValues(true, 0.5 * 10 * 3600, Double.POSITIVE_INFINITY, 0.5);
    }

    @Test
    @SmallTest
    public void testLollipopDischargingTimeEstimate() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, 0);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 60);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        initializeBatteryManager(
                new FakeAndroidBatteryManager()
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER, 1000)
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY, 60)
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_AVERAGE, -100));

        mManager.onReceive(intent);
        verifyValues(false, Double.POSITIVE_INFINITY, 0.6 * 10 * 3600, 0.6);
    }

    @Test
    @SmallTest
    public void testLollipopDischargingTimeEstimateRounding() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        Intent intent = new Intent(Intent.ACTION_BATTERY_CHANGED);
        intent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        intent.putExtra(BatteryManager.EXTRA_PLUGGED, 0);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 90);
        intent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        intent.putExtra(BatteryManager.EXTRA_STATUS, BatteryManager.BATTERY_STATUS_NOT_CHARGING);

        initializeBatteryManager(
                new FakeAndroidBatteryManager()
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER, 1999)
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY, 90)
                        .setIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_AVERAGE, -1000));

        mManager.onReceive(intent);
        verifyValues(false, Double.POSITIVE_INFINITY, Math.floor(0.9 * 1.999 * 3600), 0.9);
    }
}
