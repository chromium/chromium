// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** A wrapper */
@JNINamespace("device")
public class XrFeatureStatus {
    @CalledByNative
    public static boolean hasImmersiveFeature() {
        // TODO(https://crbug.com/333511556): Implement this.
        return false;
    }
}
