// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.le.ScanFilter;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.ArrayList;

/** Allows for the creation of a Java ArrayList of the ScanFilter object. */
@JNINamespace("device")
final class ChromeBluetoothScanFilterList {
    ArrayList<ScanFilter> mFilters;

    /** Constructs a ChromeBluetoothScanFilterList */
    public ChromeBluetoothScanFilterList() {
        mFilters = new ArrayList<>();
    }

    @CalledByNative
    private static ChromeBluetoothScanFilterList create() {
        return new ChromeBluetoothScanFilterList();
    }

    @CalledByNative
    private void addFilter(ScanFilter filter) {
        mFilters.add(filter);
    }

    @CalledByNative
    public ArrayList<ScanFilter> getList() {
        return mFilters;
    }
}
