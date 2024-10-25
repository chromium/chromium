// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/** Wraps android.bluetooth.BluetoothLeScanner. */
public class BluetoothLeScannerWrapper {
    protected final BluetoothLeScanner mScanner;
    private final HashMap<ScanCallbackWrapper, ForwardScanCallbackToWrapper> mCallbacks;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothLeScannerWrapper(BluetoothLeScanner scanner) {
        mScanner = scanner;
        mCallbacks = new HashMap<>();
    }

    public void startScan(
            List<ScanFilter> filters, int scanSettingsScanMode, ScanCallbackWrapper callback) {
        ScanSettings settings =
                new ScanSettings.Builder().setScanMode(scanSettingsScanMode).build();

        ForwardScanCallbackToWrapper callbackForwarder = new ForwardScanCallbackToWrapper(callback);
        mCallbacks.put(callback, callbackForwarder);

        mScanner.startScan(filters, settings, callbackForwarder);
    }

    public void stopScan(ScanCallbackWrapper callback) {
        ForwardScanCallbackToWrapper callbackForwarder = mCallbacks.remove(callback);
        mScanner.stopScan(callbackForwarder);
    }

    /**
     * Implements android.bluetooth.le.ScanCallback and forwards calls through to a
     * provided ScanCallbackWrapper instance.
     *
     * This class is required so that Fakes can use ScanCallbackWrapper without
     * it extending from ScanCallback. Fakes must function even on Android
     * versions where ScanCallback class is not defined.
     */
    private static class ForwardScanCallbackToWrapper extends ScanCallback {
        final ScanCallbackWrapper mWrapperCallback;

        ForwardScanCallbackToWrapper(ScanCallbackWrapper wrapperCallback) {
            mWrapperCallback = wrapperCallback;
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            ArrayList<ScanResultWrapper> resultsWrapped =
                    new ArrayList<ScanResultWrapper>(results.size());
            for (ScanResult result : results) {
                resultsWrapped.add(new ScanResultWrapper(result));
            }
            mWrapperCallback.onBatchScanResult(resultsWrapped);
        }

        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            mWrapperCallback.onScanResult(callbackType, new ScanResultWrapper(result));
        }

        @Override
        public void onScanFailed(int errorCode) {
            mWrapperCallback.onScanFailed(errorCode);
        }
    }

}
