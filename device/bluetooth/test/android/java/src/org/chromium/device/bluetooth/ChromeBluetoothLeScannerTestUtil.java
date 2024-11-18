// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.device.bluetooth.wrapper.BluetoothAdapterWrapper;

import java.util.ArrayList;

/** Exposes {@link ChromeBluetoothLeScanner} for C++ testing. */
@JNINamespace("device")
public class ChromeBluetoothLeScannerTestUtil {
    private final ChromeBluetoothLeScanner mScanner;

    @CalledByNative
    private static ChromeBluetoothLeScannerTestUtil create(
            BluetoothAdapterWrapper adapterWrapper, long nativeScannerCallback) {
        return new ChromeBluetoothLeScannerTestUtil(adapterWrapper, nativeScannerCallback);
    }

    private ChromeBluetoothLeScannerTestUtil(
            BluetoothAdapterWrapper adapterWrapper, long nativeScannerCallback) {
        mScanner =
                new ChromeBluetoothLeScanner(
                        adapterWrapper::getBluetoothLeScanner,
                        new ScannerCallback(nativeScannerCallback));
    }

    @CalledByNative
    private boolean startScan(long durationMillis) {
        // Use an empty list instead of null as the device filter list because the scanner only pass
        // it to the Android scanner, but it is important to stop the scan with the correct
        // list instance.
        return mScanner.startScan(durationMillis, new ArrayList<>());
    }

    @CalledByNative
    private boolean resumeScan(long durationMillis) {
        return mScanner.resumeScan(durationMillis);
    }

    @CalledByNative
    private boolean stopScan() {
        return mScanner.stopScan();
    }

    @CalledByNative
    private boolean isScanning() {
        return mScanner.isScanning();
    }

    private static class ScannerCallback implements ChromeBluetoothScanCallback {
        private final long mNativeScannerCallback;

        private ScannerCallback(long nativeScannerCallback) {
            mNativeScannerCallback = nativeScannerCallback;
        }

        @Override
        public void onScanFailed(int errorCode) {
            ChromeBluetoothLeScannerTestUtilJni.get()
                    .onScanFailed(mNativeScannerCallback, errorCode);
        }

        @Override
        public void onScanFinished() {
            ChromeBluetoothLeScannerTestUtilJni.get().onScanFinished(mNativeScannerCallback);
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothScannerCallback::OnScanFailed
        void onScanFailed(long nativeBluetoothScannerCallback, int errorCode);

        // Binds to BluetoothScannerCallback::OnScanFinished
        void onScanFinished(long nativeBluetoothScannerCallback);
    }
}
