// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.hardware.usb.UsbEndpoint;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

/**
 * Exposes android.hardware.usb.UsbEndpoint as necessary for C++
 * device::UsbEndpointAndroid.
 *
 * Lifetime is controlled by device::UsbEndpointAndroid.
 */
@JNINamespace("device")
final class ChromeUsbEndpoint {
    private static final String TAG = "Usb";

    final UsbEndpoint mEndpoint;

    private ChromeUsbEndpoint(UsbEndpoint endpoint) {
        mEndpoint = endpoint;
        Log.v(TAG, "ChromeUsbEndpoint created.");
    }

    @CalledByNative
    private static ChromeUsbEndpoint create(UsbEndpoint endpoint) {
        return new ChromeUsbEndpoint(endpoint);
    }

    @CalledByNative
    private int getAddress() {
        return mEndpoint.getAddress();
    }

    @CalledByNative
    private int getMaxPacketSize() {
        return mEndpoint.getMaxPacketSize();
    }

    @CalledByNative
    private int getAttributes() {
        return mEndpoint.getAttributes();
    }

    @CalledByNative
    private int getInterval() {
        return mEndpoint.getInterval();
    }
}
