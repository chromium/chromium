// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.device.bluetooth.wrapper.BluetoothLeScannerWrapper;
import org.chromium.device.bluetooth.wrapper.ScanCallbackWrapper;
import org.chromium.device.bluetooth.wrapper.ScanResultWrapper;
import org.chromium.device.bluetooth.wrapper.ThreadUtilsWrapper;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.function.Supplier;

/**
 * A helper class to perform BLE scans. It allows scanning BLE indefinitely as well as for a
 * specific duration.
 *
 * <p>Callers should start a scan with a call to {@link #startScan(long, List<ScanFilter>)}. When
 * {@link BluetoothScanCallback#onScanFinished()} is called, callers can resume the scan with the
 * same filters with a call to {@link #resumeScan(long)}. When callers are done with this scan, they
 * should call {@link #stopScan()} to end it, unless the scan ends unexpectedly with an error.
 */
@NullMarked
class ChromeBluetoothLeScanner {
    private static final String TAG = "Bluetooth";
    static final long INDEFINITE_SCAN_DURATION = Long.MIN_VALUE;

    private static final int SCAN_STATE_STOPPED = 0;
    private static final int SCAN_STATE_SCANNING = 1;
    private static final int SCAN_STATE_PAUSED = 2;

    @IntDef(value = {SCAN_STATE_STOPPED, SCAN_STATE_SCANNING, SCAN_STATE_PAUSED})
    @Retention(RetentionPolicy.SOURCE)
    @interface ScanState {}

    private final Supplier<@Nullable BluetoothLeScannerWrapper> mScannerSupplier;
    private final ChromeBluetoothScanCallback mChromeScanCallback;

    /**
     * Track the sequence number of scan lifecycles, so that we don't erroneously notify scan
     * finished if the next scan is started before the last timeout event happens.
     */
    private int mCurrentScanSequence;

    private @ScanState int mScanState = SCAN_STATE_STOPPED;
    private @Nullable List<ScanFilter> mScanFilters;
    private @Nullable ScanCallback mScanCallback;

    ChromeBluetoothLeScanner(
            Supplier<@Nullable BluetoothLeScannerWrapper> scannerSupplier,
            ChromeBluetoothScanCallback chromeScanCallback) {
        mScannerSupplier = scannerSupplier;
        mChromeScanCallback = chromeScanCallback;
    }

    // ---------------------------------------------------------------------------------------------
    // Scan lifecycle interfaces to ChromeBluetoothAdapter

    /**
     * Starts a BLE scan for the specific duration or {@link INDEFINITE_SCAN_DURATION} to scan
     * indefinitely with the specific device filters.
     *
     * @param durationMillis scan duration in ms or {@link INDEFINITE_SCAN_DURATION} if the scan is
     *     indefinite.
     * @param scanFilters List of filters used to minimize number of devices returned
     * @return {@code true} on success.
     */
    boolean startScan(long durationMillis, List<ScanFilter> scanFilters) {
        assert mScanState == SCAN_STATE_STOPPED;
        mScanCallback = new ScanCallback();
        mScanFilters = scanFilters;

        return startScanWindow(durationMillis);
    }

    /**
     * Resumes the BLE scan for the specific duration or {@link INDEFINITE_SCAN_DURATION} to scan
     * indefinitely.
     *
     * @param durationMillis scan duration in ms or {@link INDEFINITE_SCAN_DURATION} if the scan is
     *     indefinite.
     * @return {@code true} on success.
     */
    boolean resumeScan(long durationMillis) {
        assert mScanState == SCAN_STATE_PAUSED;
        assert mScanCallback != null;
        assert mScanFilters != null;

        if (startScanWindow(durationMillis)) {
            return true;
        }

        mScanState = SCAN_STATE_STOPPED;
        return false;
    }

    /**
     * Stops the BLE scan.
     *
     * @return {@code true} if a scan was in progress.
     */
    boolean stopScan() {
        if (mScanState == SCAN_STATE_STOPPED) {
            return false;
        }

        if (mScanState == SCAN_STATE_SCANNING) {
            stopScanWindow();
        }

        mScanState = SCAN_STATE_STOPPED;
        cleanScanParams();
        ++mCurrentScanSequence;
        return true;
    }

    /**
     * @return {@code true} if a scan is in progress and not paused.
     */
    boolean isScanning() {
        return mScanState == SCAN_STATE_SCANNING;
    }

    // ---------------------------------------------------------------------------------------------
    // Implementation details

    private void pauseScan(int pausingScanSequence) {
        if (mScanState == SCAN_STATE_STOPPED) {
            // It's possible that we received a request to stop scanning before the delayed callback
            // is invoked.
            return;
        }

        if (pausingScanSequence != mCurrentScanSequence) {
            // The next scan starts before the last scan window times out of the previous scan. We
            // don't want to pause the new scan here.
            return;
        }

        mScanState = SCAN_STATE_PAUSED;
        stopScanWindow();
        mChromeScanCallback.onScanFinished();
    }

    @RequiresNonNull({"mScanCallback", "mScanFilters"})
    private boolean startScanWindow(long durationMillis) {
        BluetoothLeScannerWrapper scanner = mScannerSupplier.get();
        if (scanner == null) {
            cleanScanParams();
            return false;
        }

        // scanMode note: SCAN_FAILED_FEATURE_UNSUPPORTED is caused (at least on some devices) if
        // setReportDelay() is used or if SCAN_MODE_LOW_LATENCY isn't used.
        int scanMode = ScanSettings.SCAN_MODE_LOW_LATENCY;

        try {
            scanner.startScan(mScanFilters, scanMode, mScanCallback);

            mScanState = SCAN_STATE_SCANNING;
            if (durationMillis > 0) {
                final int currentScanSequence = mCurrentScanSequence;
                ThreadUtilsWrapper.getInstance()
                        .postOnUiThreadDelayed(
                                () -> pauseScan(currentScanSequence), durationMillis);
            }

            return true;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot start scan: " + e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Adapter is off. Cannot start scan: " + e);
        }

        cleanScanParams();
        return false;
    }

    private void stopScanWindow() {
        try {
            BluetoothLeScannerWrapper scanner = mScannerSupplier.get();
            if (scanner != null) {
                scanner.stopScan(mScanCallback);
            }
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot stop scan: " + e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Adapter is off. Cannot stop scan: " + e);
        }
    }

    private void cleanScanParams() {
        mScanCallback = null;
        mScanFilters = null;
    }

    private class ScanCallback implements ScanCallbackWrapper {
        @Override
        public void onBatchScanResult(List<ScanResultWrapper> results) {
            Log.v(TAG, "onBatchScanResults");
        }

        @Override
        public void onScanResult(int callbackType, ScanResultWrapper result) {
            mChromeScanCallback.onLeScanResult(callbackType, result);
        }

        @Override
        public void onScanFailed(int errorCode) {
            if (mScanCallback != this) {
                return;
            }

            mScanState = SCAN_STATE_STOPPED;
            cleanScanParams();
            ++mCurrentScanSequence;

            mChromeScanCallback.onScanFailed(errorCode);
        }
    }
}
