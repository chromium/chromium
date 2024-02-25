// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.SparseArray;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.location.LocationUtils;

import java.util.List;
import java.util.Map;

/**
 * Exposes android.bluetooth.BluetoothAdapter as necessary for C++
 * device::BluetoothAdapterAndroid, which implements the cross platform
 * device::BluetoothAdapter.
 *
 * Lifetime is controlled by device::BluetoothAdapterAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothAdapter extends BroadcastReceiver {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothAdapterAndroid;
    // mAdapter is final to ensure registerReceiver is followed by unregisterReceiver.
    private final Wrappers.BluetoothAdapterWrapper mAdapter;
    private ScanCallback mScanCallback;

    // ---------------------------------------------------------------------------------------------
    // Construction and handler for C++ object destruction.

    /**
     * Constructs a ChromeBluetoothAdapter.
     * @param nativeBluetoothAdapterAndroid Is the associated C++
     *                                      BluetoothAdapterAndroid pointer value.
     * @param adapterWrapper Wraps the default android.bluetooth.BluetoothAdapter,
     *                       but may be either null if an adapter is not available
     *                       or a fake for testing.
     */
    public ChromeBluetoothAdapter(
            long nativeBluetoothAdapterAndroid, Wrappers.BluetoothAdapterWrapper adapterWrapper) {
        mNativeBluetoothAdapterAndroid = nativeBluetoothAdapterAndroid;
        mAdapter = adapterWrapper;
        registerBroadcastReceiver();
        if (adapterWrapper == null) {
            Log.i(TAG, "ChromeBluetoothAdapter created with no adapterWrapper.");
        } else {
            Log.i(TAG, "ChromeBluetoothAdapter created with provided adapterWrapper.");
        }
    }

    /** Handles C++ object being destroyed. */
    @CalledByNative
    private void onBluetoothAdapterAndroidDestruction() {
        stopScan();
        mNativeBluetoothAdapterAndroid = 0;
        unregisterBroadcastReceiver();
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterAndroid methods implemented in java:

    // Implements BluetoothAdapterAndroid::Create.
    @CalledByNative
    private static ChromeBluetoothAdapter create(
            long nativeBluetoothAdapterAndroid, Wrappers.BluetoothAdapterWrapper adapterWrapper) {
        return new ChromeBluetoothAdapter(nativeBluetoothAdapterAndroid, adapterWrapper);
    }

    // Implements BluetoothAdapterAndroid::GetAddress.
    @CalledByNative
    private String getAddress() {
        if (isPresent()) {
            return mAdapter.getAddress();
        } else {
            return "";
        }
    }

    // Implements BluetoothAdapterAndroid::GetName.
    @CalledByNative
    private String getName() {
        if (isPresent()) {
            return mAdapter.getName();
        } else {
            return "";
        }
    }

    // Implements BluetoothAdapterAndroid::IsPresent.
    @CalledByNative
    private boolean isPresent() {
        return mAdapter != null;
    }

    // Implements BluetoothAdapterAndroid::IsPowered.
    @CalledByNative
    private boolean isPowered() {
        return isPresent() && mAdapter.isEnabled();
    }

    // Implements BluetoothAdapterAndroid::SetPowered.
    @CalledByNative
    private boolean setPowered(boolean powered) {
        if (powered) {
            return isPresent() && mAdapter.enable();
        } else {
            return isPresent() && mAdapter.disable();
        }
    }

    // Implements BluetoothAdapterAndroid::IsDiscoverable.
    @CalledByNative
    private boolean isDiscoverable() {
        return isPresent()
                && mAdapter.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
    }

    // Implements BluetoothAdapterAndroid::IsDiscovering.
    @CalledByNative
    private boolean isDiscovering() {
        return isPresent() && (mAdapter.isDiscovering() || mScanCallback != null);
    }

    /**
     * Starts a Low Energy scan.
     * @param filters List of filters used to minimize number of devices returned
     * @return True on success.
     */
    @CalledByNative
    private boolean startScan(List<ScanFilter> filters) {
        Wrappers.BluetoothLeScannerWrapper scanner = mAdapter.getBluetoothLeScanner();

        if (scanner == null) {
            return false;
        }

        if (!canScan()) {
            return false;
        }

        // scanMode note: SCAN_FAILED_FEATURE_UNSUPPORTED is caused (at least on some devices) if
        // setReportDelay() is used or if SCAN_MODE_LOW_LATENCY isn't used.
        int scanMode = ScanSettings.SCAN_MODE_LOW_LATENCY;

        assert mScanCallback == null;
        mScanCallback = new ScanCallback();

        try {
            scanner.startScan(filters, scanMode, mScanCallback);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot start scan: " + e);
            mScanCallback = null;
            return false;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Adapter is off. Cannot start scan: " + e);
            mScanCallback = null;
            return false;
        }
        return true;
    }

    /**
     * Stops the Low Energy scan.
     * @return True if a scan was in progress.
     */
    @CalledByNative
    private boolean stopScan() {
        if (mScanCallback == null) {
            return false;
        }

        try {
            Wrappers.BluetoothLeScannerWrapper scanner = mAdapter.getBluetoothLeScanner();
            if (scanner != null) {
                scanner.stopScan(mScanCallback);
            }
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot stop scan: " + e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Adapter is off. Cannot stop scan: " + e);
        }
        mScanCallback = null;
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // Implementation details:

    /**
     * @return true if Chromium has permission to scan for Bluetooth devices and location services
     *         are on.
     */
    private boolean canScan() {
        if (mAdapter == null) {
            return false;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            Context context = mAdapter.getContext();
            return context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH_SCAN)
                            == PackageManager.PERMISSION_GRANTED
                    && context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                            == PackageManager.PERMISSION_GRANTED;
        }

        LocationUtils locationUtils = LocationUtils.getInstance();
        if (!locationUtils.isSystemLocationSettingEnabled()) return false;

        Context context = mAdapter.getContext();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return context.checkCallingOrSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                    == PackageManager.PERMISSION_GRANTED;
        }

        return (context.checkCallingOrSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                        == PackageManager.PERMISSION_GRANTED)
                || (context.checkCallingOrSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                        == PackageManager.PERMISSION_GRANTED);
    }

    private void registerBroadcastReceiver() {
        if (mAdapter != null) {
            ContextUtils.registerProtectedBroadcastReceiver(
                    mAdapter.getContext(),
                    this,
                    new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));
        }
    }

    private void unregisterBroadcastReceiver() {
        if (mAdapter != null) {
            mAdapter.getContext().unregisterReceiver(this);
        }
    }

    /**
     * Implements callbacks used during a Low Energy scan by notifying upon
     * devices discovered or detecting a scan failure.
     */
    private class ScanCallback extends Wrappers.ScanCallbackWrapper {
        @Override
        public void onBatchScanResult(List<Wrappers.ScanResultWrapper> results) {
            Log.v(TAG, "onBatchScanResults");
        }

        @Override
        public void onScanResult(int callbackType, Wrappers.ScanResultWrapper result) {
            Log.v(
                    TAG,
                    "onScanResult %d %s %s",
                    callbackType,
                    result.getDevice().getAddress(),
                    result.getDevice().getName());

            String[] uuid_strings;
            List<ParcelUuid> uuids = result.getScanRecord_getServiceUuids();

            if (uuids == null) {
                uuid_strings = new String[] {};
            } else {
                uuid_strings = new String[uuids.size()];
                for (int i = 0; i < uuids.size(); i++) {
                    uuid_strings[i] = uuids.get(i).toString();
                }
            }

            String[] serviceDataKeys;
            byte[][] serviceDataValues;
            Map<ParcelUuid, byte[]> serviceData = result.getScanRecord_getServiceData();
            if (serviceData == null) {
                serviceDataKeys = new String[] {};
                serviceDataValues = new byte[][] {};
            } else {
                serviceDataKeys = new String[serviceData.size()];
                serviceDataValues = new byte[serviceData.size()][];
                int i = 0;
                for (Map.Entry<ParcelUuid, byte[]> serviceDataItem : serviceData.entrySet()) {
                    serviceDataKeys[i] = serviceDataItem.getKey().toString();
                    serviceDataValues[i++] = serviceDataItem.getValue();
                }
            }

            int[] manufacturerDataKeys;
            byte[][] manufacturerDataValues;
            SparseArray<byte[]> manufacturerData =
                    result.getScanRecord_getManufacturerSpecificData();
            if (manufacturerData == null) {
                manufacturerDataKeys = new int[] {};
                manufacturerDataValues = new byte[][] {};
            } else {
                manufacturerDataKeys = new int[manufacturerData.size()];
                manufacturerDataValues = new byte[manufacturerData.size()][];
                for (int i = 0; i < manufacturerData.size(); i++) {
                    manufacturerDataKeys[i] = manufacturerData.keyAt(i);
                    manufacturerDataValues[i] = manufacturerData.valueAt(i);
                }
            }

            // Object can be destroyed, but Android keeps calling onScanResult.
            if (mNativeBluetoothAdapterAndroid != 0) {
                ChromeBluetoothAdapterJni.get()
                        .createOrUpdateDeviceOnScan(
                                mNativeBluetoothAdapterAndroid,
                                ChromeBluetoothAdapter.this,
                                result.getDevice().getAddress(),
                                result.getDevice(),
                                result.getScanRecord_getDeviceName(),
                                result.getRssi(),
                                uuid_strings,
                                result.getScanRecord_getTxPowerLevel(),
                                serviceDataKeys,
                                serviceDataValues,
                                manufacturerDataKeys,
                                manufacturerDataValues,
                                result.getScanRecord_getAdvertiseFlags());
            }
        }

        @Override
        public void onScanFailed(int errorCode) {
            Log.w(TAG, "onScanFailed: %d", errorCode);
            ChromeBluetoothAdapterJni.get()
                    .onScanFailed(mNativeBluetoothAdapterAndroid, ChromeBluetoothAdapter.this);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();

        if (isPresent() && BluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);

            Log.w(
                    TAG,
                    "onReceive: BluetoothAdapter.ACTION_STATE_CHANGED: %s",
                    getBluetoothStateString(state));

            switch (state) {
                case BluetoothAdapter.STATE_ON:
                    ChromeBluetoothAdapterJni.get()
                            .onAdapterStateChanged(
                                    mNativeBluetoothAdapterAndroid,
                                    ChromeBluetoothAdapter.this,
                                    true);
                    break;
                case BluetoothAdapter.STATE_OFF:
                    ChromeBluetoothAdapterJni.get()
                            .onAdapterStateChanged(
                                    mNativeBluetoothAdapterAndroid,
                                    ChromeBluetoothAdapter.this,
                                    false);
                    break;
                default:
                    // do nothing
            }
        }
    }

    private String getBluetoothStateString(int state) {
        switch (state) {
            case BluetoothAdapter.STATE_OFF:
                return "STATE_OFF";
            case BluetoothAdapter.STATE_ON:
                return "STATE_ON";
            case BluetoothAdapter.STATE_TURNING_OFF:
                return "STATE_TURNING_OFF";
            case BluetoothAdapter.STATE_TURNING_ON:
                return "STATE_TURNING_ON";
            default:
                assert false;
                return "illegal state: " + state;
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothAdapterAndroid::OnScanFailed.
        void onScanFailed(long nativeBluetoothAdapterAndroid, ChromeBluetoothAdapter caller);

        // Binds to BluetoothAdapterAndroid::CreateOrUpdateDeviceOnScan.
        void createOrUpdateDeviceOnScan(
                long nativeBluetoothAdapterAndroid,
                ChromeBluetoothAdapter caller,
                String address,
                Wrappers.BluetoothDeviceWrapper deviceWrapper,
                String localName,
                int rssi,
                String[] advertisedUuids,
                int txPower,
                String[] serviceDataKeys,
                Object[] serviceDataValues,
                int[] manufacturerDataKeys,
                Object[] manufacturerDataValues,
                int advertiseFlags);

        // Binds to BluetoothAdapterAndroid::nativeOnAdapterStateChanged
        void onAdapterStateChanged(
                long nativeBluetoothAdapterAndroid, ChromeBluetoothAdapter caller, boolean powered);
    }
}
