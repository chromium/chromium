// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.device.bluetooth.wrapper.BluetoothGattCharacteristicWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothGattDescriptorWrapper;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothGattCharacteristic as necessary for C++
 * device::BluetoothRemoteGattCharacteristicAndroid.
 *
 * <p>Lifetime is controlled by device::BluetoothRemoteGattCharacteristicAndroid.
 */
@JNINamespace("device")
@NullMarked
final class ChromeBluetoothRemoteGattCharacteristic {
    @JniType("::device::BluetoothRemoteGattCharacteristicAndroid")
    interface NativeBluetoothRemoteGattCharacteristicAndroid extends JniTypeToken {}

    private static final String TAG = "Bluetooth";

    private @Nullable JniRawPtr<NativeBluetoothRemoteGattCharacteristicAndroid>
            mNativeBluetoothRemoteGattCharacteristicAndroid;
    final BluetoothGattCharacteristicWrapper mCharacteristic;
    final String mInstanceId;
    final ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattCharacteristic(
            JniRawPtr<NativeBluetoothRemoteGattCharacteristicAndroid>
                    nativeBluetoothRemoteGattCharacteristicAndroid,
            BluetoothGattCharacteristicWrapper characteristicWrapper,
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
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != null) {
            mNativeBluetoothRemoteGattCharacteristicAndroid.release();
            mNativeBluetoothRemoteGattCharacteristicAndroid = null;
        }
        mChromeDevice.mWrapperToChromeCharacteristicsMap.remove(mCharacteristic);
    }

    void onCharacteristicChanged(byte[] value) {
        Log.i(TAG, "onCharacteristicChanged");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != null) {
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .onChanged(mNativeBluetoothRemoteGattCharacteristicAndroid, value);
        }
    }

    void onCharacteristicRead(int status) {
        Log.i(
                TAG,
                "onCharacteristicRead status:%d==%s",
                status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != null) {
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .onRead(
                            mNativeBluetoothRemoteGattCharacteristicAndroid,
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
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != null) {
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .onWrite(mNativeBluetoothRemoteGattCharacteristicAndroid, status);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattCharacteristicAndroid methods implemented in java:

    // Implements BluetoothRemoteGattCharacteristicAndroid::Create.
    @CalledByNative
    static ChromeBluetoothRemoteGattCharacteristic create(
            JniRawPtr<NativeBluetoothRemoteGattCharacteristicAndroid>
                    nativeBluetoothRemoteGattCharacteristicAndroid,
            BluetoothGattCharacteristicWrapper characteristicWrapper,
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
    @JniType("std::string")
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
        if (!assumeNonNull(mChromeDevice.mBluetoothGatt).readCharacteristic(mCharacteristic)) {
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
        if (!assumeNonNull(mChromeDevice.mBluetoothGatt).writeCharacteristic(mCharacteristic)) {
            Log.i(TAG, "writeRemoteCharacteristic writeCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Enable or disable the notifications for this characteristic.
    @CalledByNative
    private boolean setCharacteristicNotification(boolean enabled) {
        return assumeNonNull(mChromeDevice.mBluetoothGatt)
                .setCharacteristicNotification(mCharacteristic, enabled);
    }

    // Creates objects for all descriptors. Designed only to be called by
    // BluetoothRemoteGattCharacteristicAndroid::EnsureDescriptorsCreated.
    @CalledByNative
    private void createDescriptors() {
        List<BluetoothGattDescriptorWrapper> descriptors =
                mCharacteristic.getDescriptors();
        // descriptorInstanceId ensures duplicate UUIDs have unique instance
        // IDs. BluetoothGattDescriptor does not offer getInstanceId the way
        // BluetoothGattCharacteristic does.
        //
        // TODO(crbug.com/40452041) Do not reuse IDs upon onServicesDiscovered.
        int instanceIdCounter = 0;
        assumeNonNull(mNativeBluetoothRemoteGattCharacteristicAndroid);
        for (BluetoothGattDescriptorWrapper descriptor : descriptors) {
            String descriptorInstanceId =
                    mInstanceId + "/" + descriptor.getUuid().toString() + ";" + instanceIdCounter++;
            ChromeBluetoothRemoteGattCharacteristicJni.get()
                    .createGattRemoteDescriptor(
                            mNativeBluetoothRemoteGattCharacteristicAndroid,
                            descriptorInstanceId,
                            descriptor,
                            mChromeDevice);
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothRemoteGattCharacteristicAndroid::OnChanged.
        void onChanged(JniPtr<NativeBluetoothRemoteGattCharacteristicAndroid> self, byte[] value);

        // Binds to BluetoothRemoteGattCharacteristicAndroid::OnRead.
        void onRead(
                JniPtr<NativeBluetoothRemoteGattCharacteristicAndroid> self,
                int status,
                byte[] value);

        // Binds to BluetoothRemoteGattCharacteristicAndroid::OnWrite.
        void onWrite(JniPtr<NativeBluetoothRemoteGattCharacteristicAndroid> self, int status);

        // Binds to BluetoothRemoteGattCharacteristicAndroid::CreateGattRemoteDescriptor.
        void createGattRemoteDescriptor(
                JniPtr<NativeBluetoothRemoteGattCharacteristicAndroid> self,
                String instanceId,
                BluetoothGattDescriptorWrapper descriptorWrapper,
                ChromeBluetoothDevice chromeBluetoothDevice);
    }
}
