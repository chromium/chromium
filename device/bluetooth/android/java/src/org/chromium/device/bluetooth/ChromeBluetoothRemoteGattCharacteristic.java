// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothGattCharacteristic as necessary
 * for C++ device::BluetoothRemoteGattCharacteristicAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattCharacteristicAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattCharacteristic {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattCharacteristicAndroid;
    final Wrappers.BluetoothGattCharacteristicWrapper mCharacteristic;
    final String mInstanceId;
    final ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattCharacteristic(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper,
            String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        mNativeBluetoothRemoteGattCharacteristicAndroid =
                nativeBluetoothRemoteGattCharacteristicAndroid;
        mCharacteristic = characteristicWrapper;
        mInstanceId = instanceId;
        mChromeDevice = chromeDevice;

        mChromeDevice.mWrapperToChromeCharacteristicsMap.put(characteristicWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic created.");
    }

    /** Handles C++ object being destroyed. */
    @CalledByNative
    private void onBluetoothRemoteGattCharacteristicAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic Destroyed.");
        if (mChromeDevice.mBluetoothGatt != null) {
            mChromeDevice.mBluetoothGatt.setCharacteristicNotification(mCharacteristic, false);
        }
        mNativeBluetoothRemoteGattCharacteristicAndroid = 0;
        mChromeDevice.mWrapperToChromeCharacteristicsMap.remove(mCharacteristic);
    }

    void onCharacteristicChanged(byte[] value) {
        Log.i(TAG, "onCharacteristicChanged");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .onChanged(
                            mNativeBluetoothRemoteGattCharacteristicAndroid,
                            ChromeBluetoothRemoteGattCharacteristic.this,
                            value);
        }
    }

    void onCharacteristicRead(int status) {
        Log.i(
                TAG,
                "onCharacteristicRead status:%d==%s",
                status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .onRead(
                            mNativeBluetoothRemoteGattCharacteristicAndroid,
                            ChromeBluetoothRemoteGattCharacteristic.this,
                            status,
                            mCharacteristic.getValue());
        }
    }

    void onCharacteristicWrite(int status) {
        Log.i(
                TAG,
                "onCharacteristicWrite status:%d==%s",
                status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .onWrite(
                            mNativeBluetoothRemoteGattCharacteristicAndroid,
                            ChromeBluetoothRemoteGattCharacteristic.this,
                            status);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattCharacteristicAndroid methods implemented in java:

    // Implements BluetoothRemoteGattCharacteristicAndroid::Create.
    @CalledByNative
    private static ChromeBluetoothRemoteGattCharacteristic create(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper,
            String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        return new ChromeBluetoothRemoteGattCharacteristic(
                nativeBluetoothRemoteGattCharacteristicAndroid,
                characteristicWrapper,
                instanceId,
                chromeDevice);
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::GetUUID.
    @CalledByNative
    private String getUUID() {
        return mCharacteristic.getUuid().toString();
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::GetProperties.
    @CalledByNative
    private int getProperties() {
        // TODO(scheib): Must read Extended Properties Descriptor. crbug.com/548449
        return mCharacteristic.getProperties();
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic.
    @CalledByNative
    private boolean readRemoteCharacteristic() {
        if (!mChromeDevice.mBluetoothGatt.readCharacteristic(mCharacteristic)) {
            Log.i(TAG, "readRemoteCharacteristic readCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic.
    @CalledByNative
    private boolean writeRemoteCharacteristic(byte[] value, int writeType) {
        if (!mCharacteristic.setValue(value)) {
            Log.i(TAG, "writeRemoteCharacteristic setValue failed.");
            return false;
        }
        if (writeType != 0) {
            mCharacteristic.setWriteType(writeType);
        }
        if (!mChromeDevice.mBluetoothGatt.writeCharacteristic(mCharacteristic)) {
            Log.i(TAG, "writeRemoteCharacteristic writeCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Enable or disable the notifications for this characteristic.
    @CalledByNative
    private boolean setCharacteristicNotification(boolean enabled) {
        return mChromeDevice.mBluetoothGatt.setCharacteristicNotification(mCharacteristic, enabled);
    }

    // Creates objects for all descriptors. Designed only to be called by
    // BluetoothRemoteGattCharacteristicAndroid::EnsureDescriptorsCreated.
    @CalledByNative
    private void createDescriptors() {
        List<Wrappers.BluetoothGattDescriptorWrapper> descriptors =
                mCharacteristic.getDescriptors();
        // descriptorInstanceId ensures duplicate UUIDs have unique instance
        // IDs. BluetoothGattDescriptor does not offer getInstanceId the way
        // BluetoothGattCharacteristic does.
        //
        // TODO(crbug.com/40452041) Do not reuse IDs upon onServicesDiscovered.
        int instanceIdCounter = 0;
        for (Wrappers.BluetoothGattDescriptorWrapper descriptor : descriptors) {
            String descriptorInstanceId =
                    mInstanceId + "/" + descriptor.getUuid().toString() + ";" + instanceIdCounter++;
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .createGattRemoteDescriptor(
                            mNativeBluetoothRemoteGattCharacteristicAndroid,
                            ChromeBluetoothRemoteGattCharacteristic.this,
                            descriptorInstanceId,
                            descriptor,
                            mChromeDevice);
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothRemoteGattCharacteristicAndroid::OnChanged.
        void onChanged(
                long nativeBluetoothRemoteGattCharacteristicAndroid,
                ChromeBluetoothRemoteGattCharacteristic caller,
                byte[] value);

        // Binds to BluetoothRemoteGattCharacteristicAndroid::OnRead.
        void onRead(
                long nativeBluetoothRemoteGattCharacteristicAndroid,
                ChromeBluetoothRemoteGattCharacteristic caller,
                int status,
                byte[] value);

        // Binds to BluetoothRemoteGattCharacteristicAndroid::OnWrite.
        void onWrite(
                long nativeBluetoothRemoteGattCharacteristicAndroid,
                ChromeBluetoothRemoteGattCharacteristic caller,
                int status);

        // Binds to BluetoothRemoteGattCharacteristicAndroid::CreateGattRemoteDescriptor.
        void createGattRemoteDescriptor(
                long nativeBluetoothRemoteGattCharacteristicAndroid,
                ChromeBluetoothRemoteGattCharacteristic caller,
                String instanceId,
                Wrappers.BluetoothGattDescriptorWrapper descriptorWrapper,
                ChromeBluetoothDevice chromeBluetoothDevice);
    }
}
