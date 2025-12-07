// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.PackageManagerUtils;
import org.chromium.build.annotations.NullMarked;

/** A wrapper to allow querying feature status that depends on java state from native. */
@JNINamespace("device")
@NullMarked
public class XrFeatureStatus {
    @CalledByNative
    public static boolean isXrDevice() {
        return PackageManagerUtils.hasSystemFeature(PackageManagerUtils.XR_OPENXR_FEATURE_NAME);
    }
}
