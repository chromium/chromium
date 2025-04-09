// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.ArraySet;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Set;

/** Wraps android.bluetooth.BluetoothAdapter. */
@JNINamespace("device")
@NullMarked
public class BluetoothAdapterWrapper {
    private static final String TAG = "Bluetooth";

    private final BluetoothAdapter mAdapter;
    protected final Context mContext;
    private final boolean mHasBluetoothFeature;
    private final boolean mHasLowEnergyFeature;
    protected @Nullable BluetoothLeScannerWrapper mScannerWrapper;

    /**
     * Creates a BluetoothAdapterWrapper using the default android.bluetooth.BluetoothAdapter. May
     * fail if the default adapter is not available or if the application does not have sufficient
     * permissions.
     */
    @CalledByNative
    public static @Nullable BluetoothAdapterWrapper createWithDefaultAdapter(
            boolean enableClassic) {
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

        final boolean hasLowEnergyFeature =
                ContextUtils.getApplicationContext()
                        .getPackageManager()
                        .hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE);
        final boolean hasBluetoothFeature =
                enableClassic
                        && ContextUtils.getApplicationContext()
                                .getPackageManager()
                                .hasSystemFeature(PackageManager.FEATURE_BLUETOOTH);

        // Fails out if neither Classic nor Low Energy are supported.
        if (!hasBluetoothFeature && !hasLowEnergyFeature) {
            Log.e(TAG, "BluetoothAdapterWrapper.create failed: No Bluetooth support.");
            return null;
        }

        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) {
            Log.i(TAG, "BluetoothAdapterWrapper.create failed: Default adapter not found.");
            return null;
        } else {
            return new BluetoothAdapterWrapper(
                    adapter,
                    ContextUtils.getApplicationContext(),
                    hasBluetoothFeature,
                    hasLowEnergyFeature);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public BluetoothAdapterWrapper(
            BluetoothAdapter adapter,
            Context context,
            boolean hasBluetoothFeature,
            boolean hasLowEnergyFeature) {
        mAdapter = adapter;
        mContext = context;
        mHasBluetoothFeature = hasBluetoothFeature;
        mHasLowEnergyFeature = hasLowEnergyFeature;
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

    public @Nullable BluetoothLeScannerWrapper getBluetoothLeScanner() {
        if (!mHasLowEnergyFeature) {
            return null;
        }
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

    public boolean hasBluetoothFeature() {
        return mHasBluetoothFeature;
    }

    public @Nullable Set<BluetoothDeviceWrapper> getBondedDevices() {
        Set<BluetoothDevice> bondedDevices = mAdapter.getBondedDevices();
        if (bondedDevices == null) {
            return null;
        }

        ArraySet<BluetoothDeviceWrapper> set = new ArraySet<>(bondedDevices.size());
        for (BluetoothDevice device : bondedDevices) {
            set.add(new BluetoothDeviceWrapper(device));
        }

        return set;
    }

    public DeviceBondStateReceiverWrapper createDeviceBondStateReceiver(
            DeviceBondStateReceiverWrapper.Callback callback) {
        return new DeviceBondStateReceiverWrapper(callback);
    }

    public DeviceConnectStateReceiverWrapper createDeviceConnectStateReceiver(
            DeviceConnectStateReceiverWrapper.Callback callback) {
        return new DeviceConnectStateReceiverWrapper(callback);
    }
}
