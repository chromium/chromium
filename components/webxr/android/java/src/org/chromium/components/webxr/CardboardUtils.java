// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Helper class for interacting with Cardboard SDK from Java code.
 */
@JNINamespace("webxr")
public class CardboardUtils {
    /**
     * Forces to always return Cardboard Viewer v1 device parameters to prevent
     * any disk read or write and the QR code scanner activity to be launched.
     * Meant to be used for testing purposes only.
     */
    public static void useCardboardV1DeviceParamsForTesting() {
        CardboardUtilsJni.get().nativeUseCardboardV1DeviceParamsForTesting();
    }

    @NativeMethods
    /* package */ interface CardboardUtilsNative {
        void nativeUseCardboardV1DeviceParamsForTesting();
    }
}
