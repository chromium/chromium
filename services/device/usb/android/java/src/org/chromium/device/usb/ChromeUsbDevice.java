// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.annotation.TargetApi;
import android.hardware.usb.UsbConfiguration;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.hardware.usb.UsbDevice as necessary for C++
 * device::UsbDeviceAndroid.
 *
 * Lifetime is controlled by device::UsbDeviceAndroid.
 */
@JNINamespace("device")
final class ChromeUsbDevice {
    private static final String TAG = "Usb";

    final UsbDevice mDevice;

    private ChromeUsbDevice(UsbDevice device) {
        mDevice = device;
        Log.v(TAG, "ChromeUsbDevice created.");
    }

    public UsbDevice getDevice() {
        return mDevice;
    }

    @CalledByNative
    private static ChromeUsbDevice create(UsbDevice device) {
        return new ChromeUsbDevice(device);
    }

    @CalledByNative
    private int getDeviceId() {
        return mDevice.getDeviceId();
    }

    @CalledByNative
    private int getDeviceClass() {
        return mDevice.getDeviceClass();
    }

    @CalledByNative
    private int getDeviceSubclass() {
        return mDevice.getDeviceSubclass();
    }

    @CalledByNative
    private int getDeviceProtocol() {
        return mDevice.getDeviceProtocol();
    }

    @CalledByNative
    private int getVendorId() {
        return mDevice.getVendorId();
    }

    @CalledByNative
    private int getProductId() {
        return mDevice.getProductId();
    }

    @TargetApi(Build.VERSION_CODES.M)
    @CalledByNative
    private int getDeviceVersion() {
        // The Android framework generates this string with:
        // Integer.toString(version >> 8) + "." + (version & 0xFF)
        //
        // This is not technically correct because the low nibble is actually
        // two separate version components (per spec). This undoes it at least.
        String[] parts = mDevice.getVersion().split("\\.");
        assert parts.length == 2;
        return Integer.parseInt(parts[0]) << 8 | Integer.parseInt(parts[1]);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    private String getManufacturerName() {
        return mDevice.getManufacturerName();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    private String getProductName() {
        return mDevice.getProductName();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    private String getSerialNumber() {
        return mDevice.getSerialNumber();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @CalledByNative
    private UsbConfiguration[] getConfigurations() {
        int count = mDevice.getConfigurationCount();
        UsbConfiguration[] configurations = new UsbConfiguration[count];
        for (int i = 0; i < count; ++i) {
            configurations[i] = mDevice.getConfiguration(i);
        }
        return configurations;
    }

    @CalledByNative
    private UsbInterface[] getInterfaces() {
        int count = mDevice.getInterfaceCount();
        UsbInterface[] interfaces = new UsbInterface[count];
        for (int i = 0; i < count; ++i) {
            interfaces[i] = mDevice.getInterface(i);
        }
        return interfaces;
    }
}
