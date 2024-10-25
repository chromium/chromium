// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.BluetoothLeScanner;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/** Wraps android.bluetooth.BluetoothAdapter. */
@JNINamespace("device")
public class BluetoothAdapterWrapper {
    private static final String TAG = "Bluetooth";

    private final BluetoothAdapter mAdapter;
    protected final Context mContext;
    protected BluetoothLeScannerWrapper mScannerWrapper;

    /**
     * Creates a BluetoothAdapterWrapper using the default
     * android.bluetooth.BluetoothAdapter. May fail if the default adapter
     * is not available or if the application does not have sufficient
     * permissions.
     */
    @CalledByNative
    public static BluetoothAdapterWrapper createWithDefaultAdapter() {
        // In Android Q and earlier the BLUETOOTH and BLUETOOTH_ADMIN permissions must
        // be granted in the manifest. In Android S and later the BLUETOOTH_SCAN and
        // BLUETOOTH_CONNECT permissions can be requested at runtime after fetching the
        // default adapter.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            final boolean hasPermission =
                    ContextUtils.getApplicationContext().checkCallingOrSelfPermission(
                            Manifest.permission.BLUETOOTH)
                            == PackageManager.PERMISSION_GRANTED
                    && ContextUtils.getApplicationContext().checkCallingOrSelfPermission(
                               Manifest.permission.BLUETOOTH_ADMIN)
                            == PackageManager.PERMISSION_GRANTED;

            if (!hasPermission) {
                Log.w(
                        TAG,
                        "BluetoothAdapterWrapper.create failed: Lacking Bluetooth"
                                + " permissions.");
                return null;
            }
        }

        // Only Low Energy currently supported, see BluetoothAdapterAndroid class note.
        final boolean hasLowEnergyFeature = ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE);
        if (!hasLowEnergyFeature) {
            Log.i(TAG, "BluetoothAdapterWrapper.create failed: No Low Energy support.");
            return null;
        }

        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) {
            Log.i(TAG, "BluetoothAdapterWrapper.create failed: Default adapter not found.");
            return null;
        } else {
            return new BluetoothAdapterWrapper(adapter, ContextUtils.getApplicationContext());
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public BluetoothAdapterWrapper(BluetoothAdapter adapter, Context context) {
        mAdapter = adapter;
        mContext = context;
    }

    public boolean disable() {
        return mAdapter.disable();
    }

    public boolean enable() {
        return mAdapter.enable();
    }

    @SuppressLint("HardwareIds")
    public String getAddress() {
        return mAdapter.getAddress();
    }

    public BluetoothLeScannerWrapper getBluetoothLeScanner() {
        BluetoothLeScanner scanner = mAdapter.getBluetoothLeScanner();
        if (scanner == null) {
            return null;
        }
        if (mScannerWrapper == null) {
            mScannerWrapper = new BluetoothLeScannerWrapper(scanner);
        }
        return mScannerWrapper;
    }

    public Context getContext() {
        return mContext;
    }

    public String getName() {
        return mAdapter.getName();
    }

    public int getScanMode() {
        return mAdapter.getScanMode();
    }

    public boolean isDiscovering() {
        return mAdapter.isDiscovering();
    }

    public boolean isEnabled() {
        return mAdapter.isEnabled();
    }
}
