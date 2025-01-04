// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothGattDescriptor;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.UUID;

/** Wraps android.bluetooth.BluetoothGattDescriptor. */
@NullMarked
public class BluetoothGattDescriptorWrapper {
    final BluetoothGattDescriptor mDescriptor;
    final BluetoothDeviceWrapper mDeviceWrapper;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothGattDescriptorWrapper(
            BluetoothGattDescriptor descriptor, BluetoothDeviceWrapper deviceWrapper) {
        mDescriptor = descriptor;
        mDeviceWrapper = deviceWrapper;
    }

    public @Nullable BluetoothGattCharacteristicWrapper getCharacteristic() {
        return mDeviceWrapper.mCharacteristicsToWrappers.get(mDescriptor.getCharacteristic());
    }

    public UUID getUuid() {
        return mDescriptor.getUuid();
    }

    public byte[] getValue() {
        return mDescriptor.getValue();
    }

    public boolean setValue(byte[] value) {
        return mDescriptor.setValue(value);
    }
}
