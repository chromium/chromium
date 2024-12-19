// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.bluetooth.le.ScanResult;
import android.os.ParcelUuid;
import android.util.SparseArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.Map;

/** Wraps android.bluetooth.le.ScanResult. */
@NullMarked
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
        return assumeNonNull(mScanResult.getScanRecord()).getServiceUuids();
    }

    public Map<ParcelUuid, byte[]> getScanRecord_getServiceData() {
        return assumeNonNull(mScanResult.getScanRecord()).getServiceData();
    }

    public SparseArray<byte[]> getScanRecord_getManufacturerSpecificData() {
        return assumeNonNull(mScanResult.getScanRecord()).getManufacturerSpecificData();
    }

    public int getScanRecord_getTxPowerLevel() {
        return assumeNonNull(mScanResult.getScanRecord()).getTxPowerLevel();
    }

    public @Nullable String getScanRecord_getDeviceName() {
        return assumeNonNull(mScanResult.getScanRecord()).getDeviceName();
    }

    public int getScanRecord_getAdvertiseFlags() {
        return assumeNonNull(mScanResult.getScanRecord()).getAdvertiseFlags();
    }
}
