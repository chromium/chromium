// Copyright 2016 The Chromium Authors
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
import org.chromium.device.bluetooth.wrapper.BluetoothGattDescriptorWrapper;

/**
 * Exposes android.bluetooth.BluetoothGattDescriptor as necessary for C++
 * device::BluetoothRemoteGattDescriptorAndroid.
 *
 * <p>Lifetime is controlled by device::BluetoothRemoteGattDescriptorAndroid.
 */
@JNINamespace("device")
@NullMarked
final class ChromeBluetoothRemoteGattDescriptor {
    @JniType("::device::BluetoothRemoteGattDescriptorAndroid")
    interface NativeBluetoothRemoteGattDescriptorAndroid extends JniTypeToken {}

    private static final String TAG = "Bluetooth";

    private @Nullable JniRawPtr<NativeBluetoothRemoteGattDescriptorAndroid>
            mNativeBluetoothRemoteGattDescriptorAndroid;
    final BluetoothGattDescriptorWrapper mDescriptor;
    final ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattDescriptor(
            JniRawPtr<NativeBluetoothRemoteGattDescriptorAndroid>
                    nativeBluetoothRemoteGattDescriptorAndroid,
            BluetoothGattDescriptorWrapper descriptorWrapper,
            ChromeBluetoothDevice chromeDevice) {
        mNativeBluetoothRemoteGattDescriptorAndroid = nativeBluetoothRemoteGattDescriptorAndroid;
        mDescriptor = descriptorWrapper;
        mChromeDevice = chromeDevice;

        mChromeDevice.mWrapperToChromeDescriptorsMap.put(descriptorWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattDescriptor created.");
    }

    /** Handles C++ object being destroyed. */
    @CalledByNative
    private void onBluetoothRemoteGattDescriptorAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattDescriptor Destroyed.");
        if (mNativeBluetoothRemoteGattDescriptorAndroid != null) {
            mNativeBluetoothRemoteGattDescriptorAndroid.release();
            mNativeBluetoothRemoteGattDescriptorAndroid = null;
        }
        mChromeDevice.mWrapperToChromeDescriptorsMap.remove(mDescriptor);
    }

    void onDescriptorRead(int status) {
        Log.i(
                TAG,
                "onDescriptorRead status:%d==%s",
                status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattDescriptorAndroid != null) {
            ChromeBluetoothRemoteGattDescriptorJni.get()
                    .onRead(
                            mNativeBluetoothRemoteGattDescriptorAndroid,
                            status,
                            mDescriptor.getValue());
        }
    }

    void onDescriptorWrite(int status) {
        Log.i(
                TAG,
                "onDescriptorWrite status:%d==%s",
                status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattDescriptorAndroid != null) {
            ChromeBluetoothRemoteGattDescriptorJni.get()
                    .onWrite(mNativeBluetoothRemoteGattDescriptorAndroid, status);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattDescriptorAndroid methods implemented in java:

    // Implements BluetoothRemoteGattDescriptorAndroid::Create.
    @CalledByNative
    static ChromeBluetoothRemoteGattDescriptor create(
            JniRawPtr<NativeBluetoothRemoteGattDescriptorAndroid>
                    nativeBluetoothRemoteGattDescriptorAndroid,
            BluetoothGattDescriptorWrapper descriptorWrapper,
            ChromeBluetoothDevice chromeDevice) {
        return new ChromeBluetoothRemoteGattDescriptor(
                nativeBluetoothRemoteGattDescriptorAndroid, descriptorWrapper, chromeDevice);
    }

    // Implements BluetoothRemoteGattDescriptorAndroid::GetUUID.
    @CalledByNative
    @JniType("std::string")
    private String getUUID() {
        return mDescriptor.getUuid().toString();
    }

    // Implements BluetoothRemoteGattDescriptorAndroid::ReadRemoteDescriptor.
    @CalledByNative
    private boolean readRemoteDescriptor() {
        if (!assumeNonNull(mChromeDevice.mBluetoothGatt).readDescriptor(mDescriptor)) {
            Log.i(TAG, "readRemoteDescriptor readDescriptor failed.");
            return false;
        }
        return true;
    }

    // Implements BluetoothRemoteGattDescriptorAndroid::WriteRemoteDescriptor.
    @CalledByNative
    private boolean writeRemoteDescriptor(byte[] value) {
        if (!mDescriptor.setValue(value)) {
            Log.i(TAG, "writeRemoteDescriptor setValue failed.");
            return false;
        }
        if (!assumeNonNull(mChromeDevice.mBluetoothGatt).writeDescriptor(mDescriptor)) {
            Log.i(TAG, "writeRemoteDescriptor writeDescriptor failed.");
            return false;
        }
        return true;
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothRemoteGattDescriptorAndroid::OnRead.
        void onRead(
                JniPtr<NativeBluetoothRemoteGattDescriptorAndroid> self, int status, byte[] value);

        // Binds to BluetoothRemoteGattDescriptorAndroid::OnWrite.
        void onWrite(JniPtr<NativeBluetoothRemoteGattDescriptorAndroid> self, int status);
    }
}
