// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.os.ParcelUuid;
import android.util.SparseArray;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.Map;

/** Wraps android.bluetooth.le.ScanResult. */
@NullMarked
public class ScanResultWrapperImpl implements ScanResultWrapper {
    private final ScanResult mScanResult;

    public ScanResultWrapperImpl(ScanResult scanResult) {
        mScanResult = scanResult;
        assert mScanResult.getScanRecord() != null;
    }

    private ScanRecord getScanRecord() {
        return assumeNonNull(mScanResult.getScanRecord());
    }

    @Override
    public BluetoothDeviceWrapper getDevice() {
        return new BluetoothDeviceWrapper(mScanResult.getDevice());
    }

    @Override
    public int getRssi() {
        return mScanResult.getRssi();
    }

    @Override
    public @Nullable List<ParcelUuid> getScanRecord_getServiceUuids() {
        return getScanRecord().getServiceUuids();
    }

    @Override
    public @Nullable Map<ParcelUuid, byte[]> getScanRecord_getServiceData() {
        return getScanRecord().getServiceData();
    }

    @Override
    public @Nullable SparseArray<byte[]> getScanRecord_getManufacturerSpecificData() {
        return getScanRecord().getManufacturerSpecificData();
    }

    @Override
    public int getScanRecord_getTxPowerLevel() {
        return getScanRecord().getTxPowerLevel();
    }

    @Override
    public @Nullable String getScanRecord_getDeviceName() {
        return getScanRecord().getDeviceName();
    }

    @Override
    public int getScanRecord_getAdvertiseFlags() {
        return getScanRecord().getAdvertiseFlags();
    }
}
