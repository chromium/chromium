// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.serial;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

/**
 * Helper class for C++ tests in `serial_device_enumerator_android_unittest.cc`. Provides
 * ChromeSerialManager mock and verifies its calls.
 */
@JNINamespace("device")
public class CppTestHelper {

    private static long sNativePointer;

    @CalledByNative
    private static ChromeSerialManager createFakeSerialManager(long nativePointer) {
        sNativePointer = nativePointer;
        ChromeSerialManager instance =
                new ChromeSerialManager(nativePointer, null) {
                    @Override
                    protected void registerListenerAndEnumeratePorts() {
                        ChromeSerialManagerJni.get()
                                .addPortViaJni(
                                        nativePointer,
                                        "ttyS9",
                                        /* vendorId= */ -1,
                                        /* productId= */ -1,
                                        /* initialEnumeration= */ true);
                        ChromeSerialManagerJni.get()
                                .addPortViaJni(
                                        nativePointer,
                                        "ttyACM0",
                                        /* vendorId= */ 0x0694,
                                        /* productId= */ 0x0009,
                                        /* initialEnumeration= */ true);
                    }

                    @Override
                    protected void close() {}

                    @Override
                    protected String openPort(String name) {
                        return name.equals("not_found") ? "Not found" : "";
                    }
                };
        instance.registerListenerAndEnumeratePorts();
        return instance;
    }

    @CalledByNative
    public static void invokeOpenPathCallback(@JniType("std::string") String portName, int fd) {
        ChromeSerialManagerJni.get().openPathCallbackViaJni(sNativePointer, portName, fd);
    }

    @CalledByNative
    public static void invokeErrorCallback(
            @JniType("std::string") String portName, @JniType("std::string") String error) {
        ChromeSerialManagerJni.get()
                .errorCallbackViaJni(sNativePointer, portName, "Error opening port", error);
    }
}
