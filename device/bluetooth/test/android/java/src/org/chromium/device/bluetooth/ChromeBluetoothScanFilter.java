// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.le.ScanFilter;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.List;

/**
 * Exposes android.bluetooth.le.ScanFilter as necessary for C++.
 * This is currently only used for testing the ChromeBluetoothScanFilterBuilder.
 */
@JNINamespace("device")
final class ChromeBluetoothScanFilter {
    private ScanFilter mScanFilter;

    /** Constructs a ChromeBluetoothScanFilter */
    public ChromeBluetoothScanFilter(ScanFilter filter) {
        mScanFilter = filter;
    }

    // Creates a ChromeBluetoothScanFilter from the ScanFilter at the index specified in the list
    // given.
    @CalledByNative
    private static ChromeBluetoothScanFilter getFromList(List<ScanFilter> filters, int index) {
        return new ChromeBluetoothScanFilter(filters.get(index));
    }

    // Gets the Service UUID as a string from the ScanFilter
    @CalledByNative
    private String getServiceUuid() {
        return mScanFilter.getServiceUuid().toString();
    }

    // Gets the Device Name from the ScanFilter
    @CalledByNative
    private String getDeviceName() {
        return mScanFilter.getDeviceName();
    }
}
