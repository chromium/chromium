// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothDevice;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

import java.util.HashMap;

/** Wraps android.bluetooth.BluetoothDevice. */
public class BluetoothDeviceWrapper {
    private static final String TAG = "Bluetooth";
    public static final int DEVICE_CLASS_UNSPECIFIED = 0x1F00;

    private final BluetoothDevice mDevice;
    final HashMap<BluetoothGattCharacteristic, BluetoothGattCharacteristicWrapper>
            mCharacteristicsToWrappers;
    final HashMap<BluetoothGattDescriptor, BluetoothGattDescriptorWrapper> mDescriptorsToWrappers;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothDeviceWrapper(BluetoothDevice device) {
        mDevice = device;
        mCharacteristicsToWrappers = new HashMap<>();
        mDescriptorsToWrappers = new HashMap<>();
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
            // BluetoothDevice.getBluetoothClass() returns null if adapter has been powered
            // off.
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

    /**
     * Implements android.bluetooth.BluetoothGattCallback and forwards calls through
     * to a provided BluetoothGattCallbackWrapper instance.
     *
     * This class is required so that Fakes can use BluetoothGattCallbackWrapper
     * without it extending from BluetoothGattCallback. Fakes must function even on
     * Android versions where BluetoothGattCallback class is not defined.
     */
    private static class ForwardBluetoothGattCallbackToWrapper extends BluetoothGattCallback {
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

}
