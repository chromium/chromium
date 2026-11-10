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
import org.chromium.device.bluetooth.wrapper.BluetoothGattServiceWrapper;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothGattService as necessary for C++
 * device::BluetoothRemoteGattServiceAndroid.
 *
 * <p>Lifetime is controlled by device::BluetoothRemoteGattServiceAndroid.
 */
@JNINamespace("device")
@NullMarked
final class ChromeBluetoothRemoteGattService {
    @JniType("::device::BluetoothRemoteGattServiceAndroid")
    interface NativeBluetoothRemoteGattServiceAndroid extends JniTypeToken {}

    private static final String TAG = "Bluetooth";

    private @Nullable JniRawPtr<NativeBluetoothRemoteGattServiceAndroid>
            mNativeBluetoothRemoteGattServiceAndroid;
    final BluetoothGattServiceWrapper mService;
    final String mInstanceId;
    ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattService(
            JniRawPtr<NativeBluetoothRemoteGattServiceAndroid>
                    nativeBluetoothRemoteGattServiceAndroid,
            BluetoothGattServiceWrapper serviceWrapper,
            String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        mNativeBluetoothRemoteGattServiceAndroid = nativeBluetoothRemoteGattServiceAndroid;
        mService = serviceWrapper;
        mInstanceId = instanceId;
        mChromeDevice = chromeDevice;
        Log.v(TAG, "ChromeBluetoothRemoteGattService created.");
    }

    /** Handles C++ object being destroyed. */
    @CalledByNative
    private void onBluetoothRemoteGattServiceAndroidDestruction() {
        if (mNativeBluetoothRemoteGattServiceAndroid != null) {
            mNativeBluetoothRemoteGattServiceAndroid.release();
            mNativeBluetoothRemoteGattServiceAndroid = null;
        }
    }

    // Implements BluetoothRemoteGattServiceAndroid::Create.
    @CalledByNative
    static ChromeBluetoothRemoteGattService create(
            JniRawPtr<NativeBluetoothRemoteGattServiceAndroid>
                    nativeBluetoothRemoteGattServiceAndroid,
            BluetoothGattServiceWrapper serviceWrapper,
            String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        return new ChromeBluetoothRemoteGattService(
                nativeBluetoothRemoteGattServiceAndroid, serviceWrapper, instanceId, chromeDevice);
    }

    // Implements BluetoothRemoteGattServiceAndroid::GetUUID.
    @CalledByNative
    @JniType("std::string")
    private String getUUID() {
        return mService.getUuid().toString();
    }

    // Creates objects for all characteristics. Designed only to be called by
    // BluetoothRemoteGattServiceAndroid::EnsureCharacteristicsCreated.
    @CalledByNative
    private void createCharacteristics() {
        List<BluetoothGattCharacteristicWrapper> characteristics = mService.getCharacteristics();
        assumeNonNull(mNativeBluetoothRemoteGattServiceAndroid);
        for (BluetoothGattCharacteristicWrapper characteristic : characteristics) {
            // Create an adapter unique characteristic ID. getInstanceId only differs between
            // characteristic instances with the same UUID on this service.
            String characteristicInstanceId =
                    mInstanceId
                            + "/"
                            + characteristic.getUuid().toString()
                            + ","
                            + characteristic.getInstanceId();
            ChromeBluetoothRemoteGattServiceJni.get()
                    .createGattRemoteCharacteristic(
                            mNativeBluetoothRemoteGattServiceAndroid,
                            characteristicInstanceId,
                            characteristic,
                            mChromeDevice);
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothRemoteGattServiceAndroid::CreateGattRemoteCharacteristic.
        void createGattRemoteCharacteristic(
                JniPtr<NativeBluetoothRemoteGattServiceAndroid> self,
                String instanceId,
                BluetoothGattCharacteristicWrapper characteristicWrapper,
                ChromeBluetoothDevice chromeBluetoothDevice);
    }
}
