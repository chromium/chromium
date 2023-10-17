// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.hardware.usb.UsbConfiguration;
import android.hardware.usb.UsbInterface;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

/**
 * Exposes android.hardware.usb.UsbConfiguration as necessary for C++
 * device::UsbConfigurationAndroid.
 *
 * Lifetime is controlled by device::UsbConfigurationAndroid.
 */
@JNINamespace("device")
final class ChromeUsbConfiguration {
    private static final String TAG = "Usb";

    final UsbConfiguration mConfiguration;

    private ChromeUsbConfiguration(UsbConfiguration configuration) {
        mConfiguration = configuration;
        Log.v(TAG, "ChromeUsbConfiguration created.");
    }

    @CalledByNative
    private static ChromeUsbConfiguration create(UsbConfiguration configuration) {
        return new ChromeUsbConfiguration(configuration);
    }

    @CalledByNative
    private int getConfigurationValue() {
        return mConfiguration.getId();
    }

    @CalledByNative
    private boolean isSelfPowered() {
        return mConfiguration.isSelfPowered();
    }

    @CalledByNative
    private boolean isRemoteWakeup() {
        return mConfiguration.isRemoteWakeup();
    }

    @CalledByNative
    private int getMaxPower() {
        return mConfiguration.getMaxPower();
    }

    @CalledByNative
    private UsbInterface[] getInterfaces() {
        int count = mConfiguration.getInterfaceCount();
        UsbInterface[] interfaces = new UsbInterface[count];
        for (int i = 0; i < count; ++i) {
            interfaces[i] = mConfiguration.getInterface(i);
        }
        return interfaces;
    }
}
