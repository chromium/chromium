// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.SparseArray;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Wrapper classes around android.bluetooth.* classes that provide an
 * indirection layer enabling fake implementations when running tests.
 *
 * Each Wrapper base class accepts an Android API object and passes through
 * calls to it. When under test, Fake subclasses override all methods that
 * pass through to the Android object and instead provide fake implementations.
 */
@JNINamespace("device")
class Wrappers {
    private static final String TAG = "Bluetooth";

    public static final int DEVICE_CLASS_UNSPECIFIED = 0x1F00;

    /**
     * Wraps base.ThreadUtils.
     * base.ThreadUtils has a set of static method to interact with the
     * UI Thread. To be able to provide a set of test methods, ThreadUtilsWrapper
     * uses the factory pattern.
     */
    static class ThreadUtilsWrapper {
        private static Factory sFactory;

        private static ThreadUtilsWrapper sInstance;

        protected ThreadUtilsWrapper() {}

        /** Returns the singleton instance of ThreadUtilsWrapper, creating it if needed. */
        public static ThreadUtilsWrapper getInstance() {
            if (sInstance == null) {
                if (sFactory == null) {
                    sInstance = new ThreadUtilsWrapper();
                } else {
                    sInstance = sFactory.create();
                }
            }
            return sInstance;
        }

        public void runOnUiThread(Runnable r) {
            ThreadUtils.runOnUiThread(r);
        }

        /**
         * Instantiate this to explain how to create a ThreadUtilsWrapper instance in
         * ThreadUtilsWrapper.getInstance().
         */
        public interface Factory {
            public ThreadUtilsWrapper create();
        }

        /** Call this to use a different subclass of ThreadUtilsWrapper throughout the program. */
        public static void setFactory(Factory factory) {
            sFactory = factory;
            sInstance = null;
        }
    }

    /** Wraps android.bluetooth.BluetoothAdapter. */
    static class BluetoothAdapterWrapper {
        private final BluetoothAdapter mAdapter;
        protected final Context mContext;
        protected BluetoothLeScannerWrapper mScannerWrapper;

        /**
         * Creates a BluetoothAdapterWrapper using the default
         * android.bluetooth.BluetoothAdapter. May fail if the default adapter
         * is not available or if the application does not have sufficient
         * permissions.
         */
        @CalledByNative("BluetoothAdapterWrapper")
        public static BluetoothAdapterWrapper createWithDefaultAdapter() {
            // In Android Q and earlier the BLUETOOTH and BLUETOOTH_ADMIN permissions must be
            // granted in the manifest. In Android S and later the BLUETOOTH_SCAN and
            // BLUETOOTH_CONNECT permissions can be requested at runtime after fetching the default
            // adapter.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                final boolean hasPermission =
                        ContextUtils.getApplicationContext()
                                                .checkCallingOrSelfPermission(
                                                        Manifest.permission.BLUETOOTH)
                                        == PackageManager.PERMISSION_GRANTED
                                && ContextUtils.getApplicationContext()
                                                .checkCallingOrSelfPermission(
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
            final boolean hasLowEnergyFeature =
                    ContextUtils.getApplicationContext()
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

    /** Wraps android.bluetooth.BluetoothLeScanner. */
    static class BluetoothLeScannerWrapper {
        protected final BluetoothLeScanner mScanner;
        private final HashMap<ScanCallbackWrapper, ForwardScanCallbackToWrapper> mCallbacks;

        public BluetoothLeScannerWrapper(BluetoothLeScanner scanner) {
            mScanner = scanner;
            mCallbacks = new HashMap<ScanCallbackWrapper, ForwardScanCallbackToWrapper>();
        }

        public void startScan(
                List<ScanFilter> filters, int scanSettingsScanMode, ScanCallbackWrapper callback) {
            ScanSettings settings =
                    new ScanSettings.Builder().setScanMode(scanSettingsScanMode).build();

            ForwardScanCallbackToWrapper callbackForwarder =
                    new ForwardScanCallbackToWrapper(callback);
            mCallbacks.put(callback, callbackForwarder);

            mScanner.startScan(filters, settings, callbackForwarder);
        }

        public void stopScan(ScanCallbackWrapper callback) {
            ForwardScanCallbackToWrapper callbackForwarder = mCallbacks.remove(callback);
            mScanner.stopScan(callbackForwarder);
        }
    }

    /**
     * Implements android.bluetooth.le.ScanCallback and forwards calls through to a
     * provided ScanCallbackWrapper instance.
     *
     * This class is required so that Fakes can use ScanCallbackWrapper without
     * it extending from ScanCallback. Fakes must function even on Android
     * versions where ScanCallback class is not defined.
     */
    static class ForwardScanCallbackToWrapper extends ScanCallback {
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

    /** Wraps android.bluetooth.le.ScanCallback, being called by ScanCallbackImpl. */
    abstract static class ScanCallbackWrapper {
        public abstract void onBatchScanResult(List<ScanResultWrapper> results);

        public abstract void onScanResult(int callbackType, ScanResultWrapper result);

        public abstract void onScanFailed(int errorCode);
    }

    /** Wraps android.bluetooth.le.ScanResult. */
    static class ScanResultWrapper {
        private final ScanResult mScanResult;

        public ScanResultWrapper(ScanResult scanResult) {
            mScanResult = scanResult;
        }

        public BluetoothDeviceWrapper getDevice() {
            return new BluetoothDeviceWrapper(mScanResult.getDevice());
        }

        public int getRssi() {
            return mScanResult.getRssi();
        }

        public List<ParcelUuid> getScanRecord_getServiceUuids() {
            return mScanResult.getScanRecord().getServiceUuids();
        }

        public Map<ParcelUuid, byte[]> getScanRecord_getServiceData() {
            return mScanResult.getScanRecord().getServiceData();
        }

        public SparseArray<byte[]> getScanRecord_getManufacturerSpecificData() {
            return mScanResult.getScanRecord().getManufacturerSpecificData();
        }

        public int getScanRecord_getTxPowerLevel() {
            return mScanResult.getScanRecord().getTxPowerLevel();
        }

        public String getScanRecord_getDeviceName() {
            return mScanResult.getScanRecord().getDeviceName();
        }

        public int getScanRecord_getAdvertiseFlags() {
            return mScanResult.getScanRecord().getAdvertiseFlags();
        }
    }

    /** Wraps android.bluetooth.BluetoothDevice. */
    static class BluetoothDeviceWrapper {
        private final BluetoothDevice mDevice;
        private final HashMap<BluetoothGattCharacteristic, BluetoothGattCharacteristicWrapper>
                mCharacteristicsToWrappers;
        private final HashMap<BluetoothGattDescriptor, BluetoothGattDescriptorWrapper>
                mDescriptorsToWrappers;

        public BluetoothDeviceWrapper(BluetoothDevice device) {
            mDevice = device;
            mCharacteristicsToWrappers =
                    new HashMap<BluetoothGattCharacteristic, BluetoothGattCharacteristicWrapper>();
            mDescriptorsToWrappers =
                    new HashMap<BluetoothGattDescriptor, BluetoothGattDescriptorWrapper>();
        }

        public BluetoothGattWrapper connectGatt(
                Context context,
                boolean autoConnect,
                BluetoothGattCallbackWrapper callback,
                int transport) {
            return new BluetoothGattWrapper(
                    mDevice.connectGatt(
                            context,
                            autoConnect,
                            new ForwardBluetoothGattCallbackToWrapper(callback, this),
                            transport),
                    this);
        }

        public String getAddress() {
            return mDevice.getAddress();
        }

        public int getBluetoothClass_getDeviceClass() {
            if (mDevice == null || mDevice.getBluetoothClass() == null) {
                // BluetoothDevice.getBluetoothClass() returns null if adapter has been powered off.
                // Return DEVICE_CLASS_UNSPECIFIED in these cases.
                return DEVICE_CLASS_UNSPECIFIED;
            }
            return mDevice.getBluetoothClass().getDeviceClass();
        }

        public int getBondState() {
            return mDevice.getBondState();
        }

        public String getName() {
            return mDevice.getName();
        }
    }

    /** Wraps android.bluetooth.BluetoothGatt. */
    static class BluetoothGattWrapper {
        private final BluetoothGatt mGatt;
        private final BluetoothDeviceWrapper mDeviceWrapper;

        BluetoothGattWrapper(BluetoothGatt gatt, BluetoothDeviceWrapper deviceWrapper) {
            mGatt = gatt;
            mDeviceWrapper = deviceWrapper;
        }

        public void disconnect() {
            mGatt.disconnect();
        }

        public void close() {
            mGatt.close();
        }

        public boolean requestMtu(int mtu) {
            return mGatt.requestMtu(mtu);
        }

        public void discoverServices() {
            mGatt.discoverServices();
        }

        public List<BluetoothGattServiceWrapper> getServices() {
            List<BluetoothGattService> services = mGatt.getServices();
            ArrayList<BluetoothGattServiceWrapper> servicesWrapped =
                    new ArrayList<BluetoothGattServiceWrapper>(services.size());
            for (BluetoothGattService service : services) {
                servicesWrapped.add(new BluetoothGattServiceWrapper(service, mDeviceWrapper));
            }
            return servicesWrapped;
        }

        boolean readCharacteristic(BluetoothGattCharacteristicWrapper characteristic) {
            return mGatt.readCharacteristic(characteristic.mCharacteristic);
        }

        boolean setCharacteristicNotification(
                BluetoothGattCharacteristicWrapper characteristic, boolean enable) {
            return mGatt.setCharacteristicNotification(characteristic.mCharacteristic, enable);
        }

        boolean writeCharacteristic(BluetoothGattCharacteristicWrapper characteristic) {
            return mGatt.writeCharacteristic(characteristic.mCharacteristic);
        }

        boolean readDescriptor(BluetoothGattDescriptorWrapper descriptor) {
            return mGatt.readDescriptor(descriptor.mDescriptor);
        }

        boolean writeDescriptor(BluetoothGattDescriptorWrapper descriptor) {
            return mGatt.writeDescriptor(descriptor.mDescriptor);
        }
    }

    /**
     * Implements android.bluetooth.BluetoothGattCallback and forwards calls through
     * to a provided BluetoothGattCallbackWrapper instance.
     *
     * This class is required so that Fakes can use BluetoothGattCallbackWrapper
     * without it extending from BluetoothGattCallback. Fakes must function even on
     * Android versions where BluetoothGattCallback class is not defined.
     */
    static class ForwardBluetoothGattCallbackToWrapper extends BluetoothGattCallback {
        final BluetoothGattCallbackWrapper mWrapperCallback;
        final BluetoothDeviceWrapper mDeviceWrapper;

        ForwardBluetoothGattCallbackToWrapper(
                BluetoothGattCallbackWrapper wrapperCallback,
                BluetoothDeviceWrapper deviceWrapper) {
            mWrapperCallback = wrapperCallback;
            mDeviceWrapper = deviceWrapper;
        }

        @Override
        public void onCharacteristicChanged(
                BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            Log.i(TAG, "wrapper onCharacteristicChanged.");
            mWrapperCallback.onCharacteristicChanged(
                    mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic));
        }

        @Override
        public void onCharacteristicRead(
                BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            mWrapperCallback.onCharacteristicRead(
                    mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic), status);
        }

        @Override
        public void onCharacteristicWrite(
                BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            mWrapperCallback.onCharacteristicWrite(
                    mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic), status);
        }

        @Override
        public void onDescriptorRead(
                BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            mWrapperCallback.onDescriptorRead(
                    mDeviceWrapper.mDescriptorsToWrappers.get(descriptor), status);
        }

        @Override
        public void onDescriptorWrite(
                BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            mWrapperCallback.onDescriptorWrite(
                    mDeviceWrapper.mDescriptorsToWrappers.get(descriptor), status);
        }

        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            mWrapperCallback.onConnectionStateChange(status, newState);
        }

        @Override
        public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            mWrapperCallback.onMtuChanged(mtu, status);
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            mWrapperCallback.onServicesDiscovered(status);
        }
    }

    /**
     * Wrapper alternative to android.bluetooth.BluetoothGattCallback allowing clients and Fakes to
     * work on older SDK versions without having a dependency on the class not defined there.
     *
     * BluetoothGatt gatt parameters are omitted from methods as each call would
     * need to look up the correct BluetoothGattWrapper instance.
     * Client code should cache the BluetoothGattWrapper provided if
     * necessary from the initial BluetoothDeviceWrapper.connectGatt
     * call.
     */
    abstract static class BluetoothGattCallbackWrapper {
        public abstract void onCharacteristicChanged(
                BluetoothGattCharacteristicWrapper characteristic);

        public abstract void onCharacteristicRead(
                BluetoothGattCharacteristicWrapper characteristic, int status);

        public abstract void onCharacteristicWrite(
                BluetoothGattCharacteristicWrapper characteristic, int status);

        public abstract void onDescriptorRead(
                BluetoothGattDescriptorWrapper descriptor, int status);

        public abstract void onDescriptorWrite(
                BluetoothGattDescriptorWrapper descriptor, int status);

        public abstract void onConnectionStateChange(int status, int newState);

        public abstract void onMtuChanged(int mtu, int status);

        public abstract void onServicesDiscovered(int status);
    }

    /** Wraps android.bluetooth.BluetoothGattService. */
    static class BluetoothGattServiceWrapper {
        private final BluetoothGattService mService;
        private final BluetoothDeviceWrapper mDeviceWrapper;

        public BluetoothGattServiceWrapper(
                BluetoothGattService service, BluetoothDeviceWrapper deviceWrapper) {
            mService = service;
            mDeviceWrapper = deviceWrapper;
        }

        public List<BluetoothGattCharacteristicWrapper> getCharacteristics() {
            List<BluetoothGattCharacteristic> characteristics = mService.getCharacteristics();
            ArrayList<BluetoothGattCharacteristicWrapper> characteristicsWrapped =
                    new ArrayList<BluetoothGattCharacteristicWrapper>(characteristics.size());
            for (BluetoothGattCharacteristic characteristic : characteristics) {
                BluetoothGattCharacteristicWrapper characteristicWrapper =
                        mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic);
                if (characteristicWrapper == null) {
                    characteristicWrapper =
                            new BluetoothGattCharacteristicWrapper(characteristic, mDeviceWrapper);
                    mDeviceWrapper.mCharacteristicsToWrappers.put(
                            characteristic, characteristicWrapper);
                }
                characteristicsWrapped.add(characteristicWrapper);
            }
            return characteristicsWrapped;
        }

        public int getInstanceId() {
            return mService.getInstanceId();
        }

        public UUID getUuid() {
            return mService.getUuid();
        }
    }

    /** Wraps android.bluetooth.BluetoothGattCharacteristic. */
    static class BluetoothGattCharacteristicWrapper {
        final BluetoothGattCharacteristic mCharacteristic;
        final BluetoothDeviceWrapper mDeviceWrapper;

        public BluetoothGattCharacteristicWrapper(
                BluetoothGattCharacteristic characteristic, BluetoothDeviceWrapper deviceWrapper) {
            mCharacteristic = characteristic;
            mDeviceWrapper = deviceWrapper;
        }

        public List<BluetoothGattDescriptorWrapper> getDescriptors() {
            List<BluetoothGattDescriptor> descriptors = mCharacteristic.getDescriptors();

            ArrayList<BluetoothGattDescriptorWrapper> descriptorsWrapped =
                    new ArrayList<BluetoothGattDescriptorWrapper>(descriptors.size());

            for (BluetoothGattDescriptor descriptor : descriptors) {
                BluetoothGattDescriptorWrapper descriptorWrapper =
                        mDeviceWrapper.mDescriptorsToWrappers.get(descriptor);
                if (descriptorWrapper == null) {
                    descriptorWrapper =
                            new BluetoothGattDescriptorWrapper(descriptor, mDeviceWrapper);
                    mDeviceWrapper.mDescriptorsToWrappers.put(descriptor, descriptorWrapper);
                }
                descriptorsWrapped.add(descriptorWrapper);
            }
            return descriptorsWrapped;
        }

        public int getInstanceId() {
            return mCharacteristic.getInstanceId();
        }

        public int getProperties() {
            return mCharacteristic.getProperties();
        }

        public UUID getUuid() {
            return mCharacteristic.getUuid();
        }

        public byte[] getValue() {
            return mCharacteristic.getValue();
        }

        public boolean setValue(byte[] value) {
            return mCharacteristic.setValue(value);
        }

        public void setWriteType(int writeType) {
            mCharacteristic.setWriteType(writeType);
        }
    }

    /** Wraps android.bluetooth.BluetoothGattDescriptor. */
    static class BluetoothGattDescriptorWrapper {
        private final BluetoothGattDescriptor mDescriptor;
        final BluetoothDeviceWrapper mDeviceWrapper;

        public BluetoothGattDescriptorWrapper(
                BluetoothGattDescriptor descriptor, BluetoothDeviceWrapper deviceWrapper) {
            mDescriptor = descriptor;
            mDeviceWrapper = deviceWrapper;
        }

        public BluetoothGattCharacteristicWrapper getCharacteristic() {
            return mDeviceWrapper.mCharacteristicsToWrappers.get(mDescriptor.getCharacteristic());
        }

        public UUID getUuid() {
            return mDescriptor.getUuid();
        }

        public byte[] getValue() {
            return mDescriptor.getValue();
        }

        public boolean setValue(byte[] value) {
            return mDescriptor.setValue(value);
        }
    }
}
