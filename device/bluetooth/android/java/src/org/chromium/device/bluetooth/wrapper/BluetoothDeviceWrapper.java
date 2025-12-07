// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.content.Context;
import android.os.ParcelUuid;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.util.HashMap;
import java.util.Objects;
import java.util.UUID;

/** Wraps android.bluetooth.BluetoothDevice. */
@NullMarked
public class BluetoothDeviceWrapper {
    private static final String TAG = "Bluetooth";
    public static final int DEVICE_CLASS_UNSPECIFIED = 0x1F00;

    private final @Nullable BluetoothDevice mDevice;
    final HashMap<BluetoothGattCharacteristic, BluetoothGattCharacteristicWrapper>
            mCharacteristicsToWrappers;
    final HashMap<BluetoothGattDescriptor, BluetoothGattDescriptorWrapper> mDescriptorsToWrappers;

    @NullUnmarked
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothDeviceWrapper(@Nullable BluetoothDevice device) {
        mDevice = device;
        mCharacteristicsToWrappers = new HashMap<>();
        mDescriptorsToWrappers = new HashMap<>();
    }

    public BluetoothGattWrapper connectGatt(
            Context context,
            boolean autoConnect,
            BluetoothGattCallbackWrapper callback,
            int transport) {
        assumeNonNull(mDevice);
        return new BluetoothGattWrapper(
                mDevice.connectGatt(
                        context,
                        autoConnect,
                        new ForwardBluetoothGattCallbackToWrapper(callback, this),
                        transport),
                this);
    }

    public String getAddress() {
        assumeNonNull(mDevice);
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
        assumeNonNull(mDevice);
        return mDevice.getBondState();
    }

    public String getName() {
        assumeNonNull(mDevice);
        return mDevice.getName();
    }

    public int getType() {
        assumeNonNull(mDevice);
        return mDevice.getType();
    }

    public ParcelUuid[] getUuids() {
        assumeNonNull(mDevice);
        return mDevice.getUuids();
    }

    public BluetoothSocketWrapper createRfcommSocketToServiceRecord(UUID uuid) throws IOException {
        assumeNonNull(mDevice);
        return new BluetoothSocketWrapper(mDevice.createRfcommSocketToServiceRecord(uuid));
    }

    public BluetoothSocketWrapper createInsecureRfcommSocketToServiceRecord(UUID uuid)
            throws IOException {
        assumeNonNull(mDevice);
        return new BluetoothSocketWrapper(mDevice.createInsecureRfcommSocketToServiceRecord(uuid));
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof BluetoothDeviceWrapper) {
            return Objects.equals(mDevice, ((BluetoothDeviceWrapper) o).mDevice);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return mDevice == null ? 0 : mDevice.hashCode();
    }

    /**
     * Implements android.bluetooth.BluetoothGattCallback and forwards calls through to a provided
     * BluetoothGattCallbackWrapper instance.
     *
     * <p>This class is required so that Fakes can use BluetoothGattCallbackWrapper without it
     * extending from BluetoothGattCallback. Fakes must function even on Android versions where
     * BluetoothGattCallback class is not defined.
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
            BluetoothGattCharacteristicWrapper wrapped =
                    mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic);
            assert wrapped != null;
            mWrapperCallback.onCharacteristicChanged(wrapped);
        }

        @Override
        public void onCharacteristicRead(
                BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            BluetoothGattCharacteristicWrapper wrapped =
                    mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic);
            assert wrapped != null;
            mWrapperCallback.onCharacteristicRead(wrapped, status);
        }

        @Override
        public void onCharacteristicWrite(
                BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            BluetoothGattCharacteristicWrapper wrapped =
                    mDeviceWrapper.mCharacteristicsToWrappers.get(characteristic);
            assert wrapped != null;
            mWrapperCallback.onCharacteristicWrite(wrapped, status);
        }

        @Override
        public void onDescriptorRead(
                BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            BluetoothGattDescriptorWrapper wrapped =
                    mDeviceWrapper.mDescriptorsToWrappers.get(descriptor);
            assert wrapped != null;
            mWrapperCallback.onDescriptorRead(wrapped, status);
        }

        @Override
        public void onDescriptorWrite(
                BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            BluetoothGattDescriptorWrapper wrapped =
                    mDeviceWrapper.mDescriptorsToWrappers.get(descriptor);
            assert wrapped != null;
            mWrapperCallback.onDescriptorWrite(wrapped, status);
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
