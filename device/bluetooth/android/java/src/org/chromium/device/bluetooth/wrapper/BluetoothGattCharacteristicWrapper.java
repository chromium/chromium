// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/** Wraps android.bluetooth.BluetoothGattCharacteristic. */
public class BluetoothGattCharacteristicWrapper {
    final BluetoothGattCharacteristic mCharacteristic;
    final BluetoothDeviceWrapper mDeviceWrapper;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothGattCharacteristicWrapper(
            BluetoothGattCharacteristic characteristic, BluetoothDeviceWrapper deviceWrapper) {
        mCharacteristic = characteristic;
        mDeviceWrapper = deviceWrapper;
    }

    public List<BluetoothGattDescriptorWrapper> getDescriptors() {
        List<BluetoothGattDescriptor> descriptors = mCharacteristic.getDescriptors();

        ArrayList<BluetoothGattDescriptorWrapper> descriptorsWrapped =
                new ArrayList<>(descriptors.size());

        for (BluetoothGattDescriptor descriptor : descriptors) {
            BluetoothGattDescriptorWrapper descriptorWrapper =
                    mDeviceWrapper.mDescriptorsToWrappers.get(descriptor);
            if (descriptorWrapper == null) {
                descriptorWrapper = new BluetoothGattDescriptorWrapper(descriptor, mDeviceWrapper);
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
