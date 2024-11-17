// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.le.ScanResult;
import android.os.ParcelUuid;
import android.util.SparseArray;
import androidx.annotation.VisibleForTesting;

import java.util.List;
import java.util.Map;

/** Wraps android.bluetooth.le.ScanResult. */
public class ScanResultWrapper {
    private final ScanResult mScanResult;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public ScanResultWrapper(ScanResult scanResult) {
        mScanResult = scanResult;
    }

    public BluetoothDeviceWrapper getDevice() {
        return new BluetoothDeviceWrapper(mScanResult.getDevice());
    }

    public int getRssi() {
        return mScanResult.getRssi();
    }

    public List<ParcelUuid> getScanRecord_getServiceUuids() {
        return mScanResult.getScanRecord().getServiceUuids();
    }

    public Map<ParcelUuid, byte[]> getScanRecord_getServiceData() {
        return mScanResult.getScanRecord().getServiceData();
    }

    public SparseArray<byte[]> getScanRecord_getManufacturerSpecificData() {
        return mScanResult.getScanRecord().getManufacturerSpecificData();
    }

    public int getScanRecord_getTxPowerLevel() {
        return mScanResult.getScanRecord().getTxPowerLevel();
    }

    public String getScanRecord_getDeviceName() {
        return mScanResult.getScanRecord().getDeviceName();
    }

    public int getScanRecord_getAdvertiseFlags() {
        return mScanResult.getScanRecord().getAdvertiseFlags();
    }
}
