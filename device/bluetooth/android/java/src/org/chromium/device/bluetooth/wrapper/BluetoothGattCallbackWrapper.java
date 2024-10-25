// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

/**
 * Wrapper alternative to android.bluetooth.BluetoothGattCallback allowing
 * clients and Fakes to
 * work on older SDK versions without having a dependency on the class not
 * defined there.
 *
 * BluetoothGatt gatt parameters are omitted from methods as each call would
 * need to look up the correct BluetoothGattWrapper instance.
 * Client code should cache the BluetoothGattWrapper provided if
 * necessary from the initial BluetoothDeviceWrapper.connectGatt
 * call.
 */
public interface BluetoothGattCallbackWrapper {
    void onCharacteristicChanged(BluetoothGattCharacteristicWrapper characteristic);

    void onCharacteristicRead(BluetoothGattCharacteristicWrapper characteristic, int status);

    void onCharacteristicWrite(BluetoothGattCharacteristicWrapper characteristic, int status);

    void onDescriptorRead(BluetoothGattDescriptorWrapper descriptor, int status);

    void onDescriptorWrite(BluetoothGattDescriptorWrapper descriptor, int status);

    void onConnectionStateChange(int status, int newState);

    void onMtuChanged(int mtu, int status);

    void onServicesDiscovered(int status);
}
