// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.hardware.usb.UsbDeviceConnection;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

/**
 * Exposes android.hardware.usb.UsbDeviceConnection as necessary for C++
 * device::UsbDeviceHandleAndroid.
 *
 * Lifetime is controlled by device::UsbDeviceHandleAndroid.
 */
@JNINamespace("device")
class ChromeUsbConnection {
    private static final String TAG = "Usb";

    final UsbDeviceConnection mConnection;

    private ChromeUsbConnection(UsbDeviceConnection connection) {
        mConnection = connection;
        Log.v(TAG, "ChromeUsbConnection created.");
    }

    @CalledByNative
    private static ChromeUsbConnection create(UsbDeviceConnection connection) {
        return new ChromeUsbConnection(connection);
    }

    @CalledByNative
    private int getFileDescriptor() {
        return mConnection.getFileDescriptor();
    }

    @CalledByNative
    private void close() {
        mConnection.close();
    }
}
