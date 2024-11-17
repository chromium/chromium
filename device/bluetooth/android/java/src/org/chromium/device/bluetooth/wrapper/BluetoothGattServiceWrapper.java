// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/** Wraps android.bluetooth.BluetoothGattService. */
public class BluetoothGattServiceWrapper {
    private final BluetoothGattService mService;
    private final BluetoothDeviceWrapper mDeviceWrapper;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothGattServiceWrapper(
            BluetoothGattService service, BluetoothDeviceWrapper deviceWrapper) {
        mService = service;
        mDeviceWrapper = deviceWrapper;
    }

    public List<BluetoothGattCharacteristicWrapper> getCharacteristics() {
        List<BluetoothGattCharacteristic> characteristics = mService.getCharacteristics();
        ArrayList<BluetoothGattCharacteristicWrapper> characteristicsWrapped = new ArrayList<>(
                characteristics.size());
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
