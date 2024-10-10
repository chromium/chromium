// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.BluetoothDevice;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.HashMap;

/**
 * Exposes android.bluetooth.BluetoothDevice as necessary for C++
 * device::BluetoothDeviceAndroid.
 *
 * Lifetime is controlled by device::BluetoothDeviceAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothDevice {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothDeviceAndroid;
    final Wrappers.BluetoothDeviceWrapper mDevice;
    Wrappers.BluetoothGattWrapper mBluetoothGatt;
    private final BluetoothGattCallbackImpl mBluetoothGattCallbackImpl;
    final HashMap<
                    Wrappers.BluetoothGattCharacteristicWrapper,
                    ChromeBluetoothRemoteGattCharacteristic>
            mWrapperToChromeCharacteristicsMap;
    final HashMap<Wrappers.BluetoothGattDescriptorWrapper, ChromeBluetoothRemoteGattDescriptor>
            mWrapperToChromeDescriptorsMap;

    private ChromeBluetoothDevice(
            long nativeBluetoothDeviceAndroid, Wrappers.BluetoothDeviceWrapper deviceWrapper) {
        mNativeBluetoothDeviceAndroid = nativeBluetoothDeviceAndroid;
        mDevice = deviceWrapper;
        mBluetoothGattCallbackImpl = new BluetoothGattCallbackImpl();
        mWrapperToChromeCharacteristicsMap =
                new HashMap<
                        Wrappers.BluetoothGattCharacteristicWrapper,
                        ChromeBluetoothRemoteGattCharacteristic>();
        mWrapperToChromeDescriptorsMap =
                new HashMap<
                        Wrappers.BluetoothGattDescriptorWrapper,
                        ChromeBluetoothRemoteGattDescriptor>();
        Log.v(TAG, "ChromeBluetoothDevice created.");
    }

    /** Handles C++ object being destroyed. */
    @CalledByNative
    private void onBluetoothDeviceAndroidDestruction() {
        if (mBluetoothGatt != null) {
            mBluetoothGatt.close();
            mBluetoothGatt = null;
        }
        mNativeBluetoothDeviceAndroid = 0;
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothDeviceAndroid methods implemented in java:

    // Implements BluetoothDeviceAndroid::Create.
    @CalledByNative
    private static ChromeBluetoothDevice create(
            long nativeBluetoothDeviceAndroid, Wrappers.BluetoothDeviceWrapper deviceWrapper) {
        return new ChromeBluetoothDevice(nativeBluetoothDeviceAndroid, deviceWrapper);
    }

    // Implements BluetoothDeviceAndroid::GetBluetoothClass.
    @CalledByNative
    private int getBluetoothClass() {
        return mDevice.getBluetoothClass_getDeviceClass();
    }

    // Implements BluetoothDeviceAndroid::GetAddress.
    @CalledByNative
    private String getAddress() {
        return mDevice.getAddress();
    }

    // Implements BluetoothDeviceAndroid::GetName.
    @CalledByNative
    private String getName() {
        return mDevice.getName();
    }

    // Implements BluetoothDeviceAndroid::IsPaired.
    @CalledByNative
    private boolean isPaired() {
        return mDevice.getBondState() == BluetoothDevice.BOND_BONDED;
    }

    // Implements BluetoothDeviceAndroid::CreateGattConnectionImpl.
    @CalledByNative
    private void createGattConnectionImpl() {
        Log.i(TAG, "connectGatt");

        if (mBluetoothGatt != null) mBluetoothGatt.close();

        // autoConnect set to false as under experimentation using autoConnect failed to complete
        // connections.
        mBluetoothGatt =
                mDevice.connectGatt(
                        ContextUtils.getApplicationContext(),
                        /* autoConnect= */ false,
                        mBluetoothGattCallbackImpl,
                        // Prefer LE for dual-mode devices due to lower energy consumption.
                        BluetoothDevice.TRANSPORT_LE);
    }

    // Implements BluetoothDeviceAndroid::DisconnectGatt.
    @CalledByNative
    private void disconnectGatt() {
        Log.i(TAG, "BluetoothGatt.disconnect");
        if (mBluetoothGatt != null) mBluetoothGatt.disconnect();
    }

    // Implements callbacks related to a GATT connection.
    private class BluetoothGattCallbackImpl extends Wrappers.BluetoothGattCallbackWrapper {
        @Override
        public void onConnectionStateChange(int status, int newState) {
            Log.i(
                    TAG,
                    "onConnectionStateChange status:%d newState:%s",
                    status,
                    (newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED)
                            ? "Connected"
                            : "Disconnected");

            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(() -> onConnectionStateChangeUiThread(status, newState));
        }

        private void onConnectionStateChangeUiThread(int status, int newState) {
            if (newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED) {
                // Try requesting for a larger ATT MTU so that more information can be exchanged per
                // transmission.
                if (!mBluetoothGatt.requestMtu(517)) {
                    mBluetoothGatt.discoverServices();
                }
            } else if (newState == android.bluetooth.BluetoothProfile.STATE_DISCONNECTED) {
                if (mBluetoothGatt != null) {
                    mBluetoothGatt.close();
                    mBluetoothGatt = null;
                }
            }
            if (mNativeBluetoothDeviceAndroid != 0) {
                ChromeBluetoothDeviceJni.get()
                        .onConnectionStateChange(
                                mNativeBluetoothDeviceAndroid,
                                ChromeBluetoothDevice.this,
                                status,
                                newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED);
            }
        }

        @Override
        public void onMtuChanged(final int mtu, final int status) {
            Log.i(
                    TAG,
                    "onMtuChanged mtu:%d status:%d==%s",
                    mtu,
                    status,
                    status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(
                            () -> {
                                if (mNativeBluetoothDeviceAndroid == 0 || mBluetoothGatt == null) {
                                    return;
                                }
                                mBluetoothGatt.discoverServices();
                            });
        }

        @Override
        public void onServicesDiscovered(final int status) {
            Log.i(
                    TAG,
                    "onServicesDiscovered status:%d==%s",
                    status,
                    status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(() -> onServicesDiscoveredUiThread());
        }

        private void onServicesDiscoveredUiThread() {
            if (mNativeBluetoothDeviceAndroid != 0) {
                // When the device disconnects it deletes mBluetoothGatt, so we need to check it's
                // not null.
                if (mBluetoothGatt == null) {
                    return;
                }

                // TODO(crbug.com/40452041): Update or replace existing GATT objects if they change
                // after initial discovery.
                for (Wrappers.BluetoothGattServiceWrapper service : mBluetoothGatt.getServices()) {
                    // Create an adapter unique service ID. getInstanceId only differs between
                    // service instances with the same UUID on this device.
                    String serviceInstanceId =
                            getAddress()
                                    + "/"
                                    + service.getUuid().toString()
                                    + ","
                                    + service.getInstanceId();
                    ChromeBluetoothDeviceJni.get()
                            .createGattRemoteService(
                                    mNativeBluetoothDeviceAndroid,
                                    ChromeBluetoothDevice.this,
                                    serviceInstanceId,
                                    service);
                }
                ChromeBluetoothDeviceJni.get()
                        .onGattServicesDiscovered(
                                mNativeBluetoothDeviceAndroid, ChromeBluetoothDevice.this);
            }
        }

        @Override
        public void onCharacteristicChanged(
                final Wrappers.BluetoothGattCharacteristicWrapper characteristic) {
            Log.i(TAG, "device onCharacteristicChanged.");
            // Copy the characteristic's value for this event so that new notifications that
            // arrive before the posted task runs do not affect this event's value.
            byte[] value = characteristic.getValue();
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(
                            () -> {
                                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic =
                                        mWrapperToChromeCharacteristicsMap.get(characteristic);
                                if (chromeCharacteristic == null) {
                                    // Android events arriving with no Chrome object is expected
                                    // rarely only when the event races object destruction.
                                    Log.v(
                                            TAG,
                                            "onCharacteristicChanged when chromeCharacteristic"
                                                    + " == null.");
                                } else {
                                    chromeCharacteristic.onCharacteristicChanged(value);
                                }
                            });
        }

        @Override
        public void onCharacteristicRead(
                final Wrappers.BluetoothGattCharacteristicWrapper characteristic,
                final int status) {
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(
                            () -> {
                                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic =
                                        mWrapperToChromeCharacteristicsMap.get(characteristic);
                                if (chromeCharacteristic == null) {
                                    // Android events arriving with no Chrome object is expected
                                    // rarely: only when the event races object destruction.
                                    Log.v(
                                            TAG,
                                            "onCharacteristicRead when chromeCharacteristic =="
                                                    + " null.");
                                } else {
                                    chromeCharacteristic.onCharacteristicRead(status);
                                }
                            });
        }

        @Override
        public void onCharacteristicWrite(
                final Wrappers.BluetoothGattCharacteristicWrapper characteristic,
                final int status) {
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(
                            () -> {
                                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic =
                                        mWrapperToChromeCharacteristicsMap.get(characteristic);
                                if (chromeCharacteristic == null) {
                                    // Android events arriving with no Chrome object is expected
                                    // rarely: only when the event races object destruction.
                                    Log.v(
                                            TAG,
                                            "onCharacteristicWrite when chromeCharacteristic =="
                                                    + " null.");
                                } else {
                                    chromeCharacteristic.onCharacteristicWrite(status);
                                }
                            });
        }

        @Override
        public void onDescriptorRead(
                final Wrappers.BluetoothGattDescriptorWrapper descriptor, final int status) {
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(
                            () -> {
                                ChromeBluetoothRemoteGattDescriptor chromeDescriptor =
                                        mWrapperToChromeDescriptorsMap.get(descriptor);
                                if (chromeDescriptor == null) {
                                    // Android events arriving with no Chrome object is expected
                                    // rarely: only when the event races object destruction.
                                    Log.v(TAG, "onDescriptorRead when chromeDescriptor == null.");
                                } else {
                                    chromeDescriptor.onDescriptorRead(status);
                                }
                            });
        }

        @Override
        public void onDescriptorWrite(
                final Wrappers.BluetoothGattDescriptorWrapper descriptor, final int status) {
            Wrappers.ThreadUtilsWrapper.getInstance()
                    .runOnUiThread(
                            () -> {
                                ChromeBluetoothRemoteGattDescriptor chromeDescriptor =
                                        mWrapperToChromeDescriptorsMap.get(descriptor);
                                if (chromeDescriptor == null) {
                                    // Android events arriving with no Chrome object is expected
                                    // rarely: only when the event races object destruction.
                                    Log.v(TAG, "onDescriptorWrite when chromeDescriptor == null.");
                                } else {
                                    chromeDescriptor.onDescriptorWrite(status);
                                }
                            });
        }
    }

    @NativeMethods
    interface Natives {
        // Binds to BluetoothDeviceAndroid::OnConnectionStateChange.
        void onConnectionStateChange(
                long nativeBluetoothDeviceAndroid,
                ChromeBluetoothDevice caller,
                int status,
                boolean connected);

        // Binds to BluetoothDeviceAndroid::CreateGattRemoteService.
        void createGattRemoteService(
                long nativeBluetoothDeviceAndroid,
                ChromeBluetoothDevice caller,
                String instanceId,
                Wrappers.BluetoothGattServiceWrapper serviceWrapper);

        // Binds to BluetoothDeviceAndroid::GattServicesDiscovered.
        void onGattServicesDiscovered(
                long nativeBluetoothDeviceAndroid, ChromeBluetoothDevice caller);
    }
}
