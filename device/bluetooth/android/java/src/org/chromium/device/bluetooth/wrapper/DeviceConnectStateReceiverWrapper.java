// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public class DeviceConnectStateReceiverWrapper extends BroadcastReceiver {
    private final Callback mCallback;

    DeviceConnectStateReceiverWrapper(Callback callback) {
        mCallback = callback;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        boolean isConnected;
        if (BluetoothDevice.ACTION_ACL_CONNECTED.equals(action)) {
            isConnected = true;
        } else if (BluetoothDevice.ACTION_ACL_DISCONNECTED.equals(action)) {
            isConnected = false;
        } else {
            return;
        }

        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        int transport =
                intent.getIntExtra(BluetoothDevice.EXTRA_TRANSPORT, BluetoothDevice.TRANSPORT_AUTO);

        mCallback.onDeviceConnectStateChanged(
                new BluetoothDeviceWrapper(device), transport, isConnected);
    }

    public interface Callback {
        void onDeviceConnectStateChanged(
                BluetoothDeviceWrapper device, int transport, boolean connected);
    }
}
