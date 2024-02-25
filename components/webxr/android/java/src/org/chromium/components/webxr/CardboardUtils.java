// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Helper class for interacting with Cardboard SDK from Java code. */
@JNINamespace("webxr")
public class CardboardUtils {
    /**
     * Forces to always return Cardboard Viewer v1 device parameters to prevent
     * any disk read or write and the QR code scanner activity to be launched.
     */
    public static void useCardboardV1DeviceParamsForTesting() {
        CardboardUtilsJni.get().nativeUseCardboardV1DeviceParamsForTesting();
    }

    /** Forces to use the mock implementation of the `device::CardboardSdk` interface. */
    public static void useCardboardMockForTesting() {
        CardboardUtilsJni.get().nativeUseCardboardMockForTesting();
    }

    /**
     * Returns true if `MockCardboardSdk::ScanQrCodeAndSaveDeviceParams()` has been executed,
     * otherwise it returns false.
     */
    public static boolean checkQrCodeScannerWasLaunchedForTesting() {
        return CardboardUtilsJni.get().nativeCheckQrCodeScannerWasLaunchedForTesting();
    }

    @NativeMethods
    /* package */ interface CardboardUtilsNative {
        void nativeUseCardboardV1DeviceParamsForTesting();

        void nativeUseCardboardMockForTesting();

        boolean nativeCheckQrCodeScannerWasLaunchedForTesting();
    }
}
