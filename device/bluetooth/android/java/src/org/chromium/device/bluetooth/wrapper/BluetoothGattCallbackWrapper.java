// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
@NullMarked
public interface BluetoothGattCallbackWrapper {
    void onCharacteristicChanged(@Nullable BluetoothGattCharacteristicWrapper characteristic);

    void onCharacteristicRead(
            @Nullable BluetoothGattCharacteristicWrapper characteristic, int status);

    void onCharacteristicWrite(
            @Nullable BluetoothGattCharacteristicWrapper characteristic, int status);

    void onDescriptorRead(@Nullable BluetoothGattDescriptorWrapper descriptor, int status);

    void onDescriptorWrite(@Nullable BluetoothGattDescriptorWrapper descriptor, int status);

    void onConnectionStateChange(int status, int newState);

    void onMtuChanged(int mtu, int status);

    void onServicesDiscovered(int status);
}
