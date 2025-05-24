// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;


import android.os.ParcelUuid;
import android.util.SparseArray;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.Map;

/** Wraps android.bluetooth.le.ScanResult. */
@NullMarked
public interface ScanResultWrapper {
    BluetoothDeviceWrapper getDevice();

    int getRssi();

    @Nullable
    List<ParcelUuid> getScanRecord_getServiceUuids();

    @Nullable
    Map<ParcelUuid, byte[]> getScanRecord_getServiceData();

    @Nullable
    SparseArray<byte[]> getScanRecord_getManufacturerSpecificData();

    int getScanRecord_getTxPowerLevel();

    @Nullable
    String getScanRecord_getDeviceName();

    int getScanRecord_getAdvertiseFlags();
}
