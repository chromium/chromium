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
 * Exposes android.bluetooth.BluetoothGattService as necessary
 * for C++ device::BluetoothRemoteGattServiceAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattServiceAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattService {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattServiceAndroid;
    final Wrappers.BluetoothGattServiceWrapper mService;
    final String mInstanceId;
    ChromeBluetoothDevice mChromeDevice;

    private ChromeBluetoothRemoteGattService(
            long nativeBluetoothRemoteGattServiceAndroid,
            Wrappers.BluetoothGattServiceWrapper serviceWrapper,
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
        mNativeBluetoothRemoteGattServiceAndroid = 0;
    }

    // Implements BluetoothRemoteGattServiceAndroid::Create.
    @CalledByNative
    private static ChromeBluetoothRemoteGattService create(
            long nativeBluetoothRemoteGattServiceAndroid,
            Wrappers.BluetoothGattServiceWrapper serviceWrapper,
            String instanceId,
            ChromeBluetoothDevice chromeDevice) {
        return new ChromeBluetoothRemoteGattService(
                nativeBluetoothRemoteGattServiceAndroid, serviceWrapper, instanceId, chromeDevice);
    }

    // Implements BluetoothRemoteGattServiceAndroid::GetUUID.
    @CalledByNative
    private String getUUID() {
        return mService.getUuid().toString();
    }

    // Creates objects for all characteristics. Designed only to be called by
    // BluetoothRemoteGattServiceAndroid::EnsureCharacteristicsCreated.
    @CalledByNative
    private void createCharacteristics() {
        List<Wrappers.BluetoothGattCharacteristicWrapper> characteristics =
                mService.getCharacteristics();
        for (Wrappers.BluetoothGattCharacteristicWrapper characteristic : characteristics) {
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
                            ChromeBluetoothRemoteGattService.this,
                            characteristicInstanceId,
                            characteristic,
                            mChromeDevice);
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothRemoteGattServiceAndroid::CreateGattRemoteCharacteristic.
        void createGattRemoteCharacteristic(
                long nativeBluetoothRemoteGattServiceAndroid,
                ChromeBluetoothRemoteGattService caller,
                String instanceId,
                Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper,
                ChromeBluetoothDevice chromeBluetoothDevice);
    }
}
