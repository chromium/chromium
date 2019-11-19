// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.annotation.TargetApi;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.hardware.usb.UsbInterface as necessary for C++
 * device::UsbInterfaceAndroid.
 *
 * Lifetime is controlled by device::UsbInterfaceAndroid.
 */
@JNINamespace("device")
final class ChromeUsbInterface {
    private static final String TAG = "Usb";

    final UsbInterface mInterface;

    private ChromeUsbInterface(UsbInterface iface) {
        mInterface = iface;
        Log.v(TAG, "ChromeUsbInterface created.");
    }

    @CalledByNative
    private static ChromeUsbInterface create(UsbInterface iface) {
        return new ChromeUsbInterface(iface);
    }

    @CalledByNative
    private int getInterfaceNumber() {
        return mInterface.getId();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    private int getAlternateSetting() {
        return mInterface.getAlternateSetting();
    }

    @CalledByNative
    private int getInterfaceClass() {
        return mInterface.getInterfaceClass();
    }

    @CalledByNative
    private int getInterfaceSubclass() {
        return mInterface.getInterfaceSubclass();
    }

    @CalledByNative
    private int getInterfaceProtocol() {
        return mInterface.getInterfaceProtocol();
    }

    @CalledByNative
    private UsbEndpoint[] getEndpoints() {
        int count = mInterface.getEndpointCount();
        UsbEndpoint[] endpoints = new UsbEndpoint[count];
        for (int i = 0; i < count; ++i) {
            endpoints[i] = mInterface.getEndpoint(i);
        }
        return endpoints;
    }
}
