// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("webxr")
public final class WebXrAndroidFeatureMap {
    /** Convenience method to check if OpenXR should be enabled due to complex state. */
    public static boolean isOpenXrEnabled() {
        return WebXrAndroidFeatureMapJni.get().isOpenXrEnabled();
    }

    public static boolean isHandTrackingEnabled() {
        return WebXrAndroidFeatureMapJni.get().isHandTrackingEnabled();
    }

    @NativeMethods
    public interface Natives {
        boolean isOpenXrEnabled();

        boolean isHandTrackingEnabled();
    }
}
