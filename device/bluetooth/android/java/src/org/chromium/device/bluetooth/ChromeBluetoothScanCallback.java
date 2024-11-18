// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.chromium.device.bluetooth.wrapper.ScanResultWrapper;

/**
 * An interface called by {@link ChromeBluetoothLeScanner} when a device is found, an error occurs
 * and a scan finishes.
 */
interface ChromeBluetoothScanCallback {
    default void onLeScanResult(int callbackType, ScanResultWrapper scanResult) {}

    void onScanFailed(int errorCode);

    void onScanFinished();
}
