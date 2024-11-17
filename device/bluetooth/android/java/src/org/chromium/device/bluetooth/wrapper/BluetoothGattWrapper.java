// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattService;

import java.util.ArrayList;
import java.util.List;

/** Wraps android.bluetooth.BluetoothGatt. */
public class BluetoothGattWrapper {
    private final BluetoothGatt mGatt;
    private final BluetoothDeviceWrapper mDeviceWrapper;

    public BluetoothGattWrapper(BluetoothGatt gatt, BluetoothDeviceWrapper deviceWrapper) {
        mGatt = gatt;
        mDeviceWrapper = deviceWrapper;
    }

    public void disconnect() {
        mGatt.disconnect();
    }

    public void close() {
        mGatt.close();
    }

    public boolean requestMtu(int mtu) {
        return mGatt.requestMtu(mtu);
    }

    public void discoverServices() {
        mGatt.discoverServices();
    }

    public List<BluetoothGattServiceWrapper> getServices() {
        List<BluetoothGattService> services = mGatt.getServices();
        ArrayList<BluetoothGattServiceWrapper> servicesWrapped = new ArrayList<>(
                services.size());
        for (BluetoothGattService service : services) {
            servicesWrapped.add(new BluetoothGattServiceWrapper(service, mDeviceWrapper));
        }
        return servicesWrapped;
    }

    public boolean readCharacteristic(BluetoothGattCharacteristicWrapper characteristic) {
        return mGatt.readCharacteristic(characteristic.mCharacteristic);
    }

    public boolean setCharacteristicNotification(
            BluetoothGattCharacteristicWrapper characteristic, boolean enable) {
        return mGatt.setCharacteristicNotification(characteristic.mCharacteristic, enable);
    }

    public boolean writeCharacteristic(BluetoothGattCharacteristicWrapper characteristic) {
        return mGatt.writeCharacteristic(characteristic.mCharacteristic);
    }

    public boolean readDescriptor(BluetoothGattDescriptorWrapper descriptor) {
        return mGatt.readDescriptor(descriptor.mDescriptor);
    }

    public boolean writeDescriptor(BluetoothGattDescriptorWrapper descriptor) {
        return mGatt.writeDescriptor(descriptor.mDescriptor);
    }
}
