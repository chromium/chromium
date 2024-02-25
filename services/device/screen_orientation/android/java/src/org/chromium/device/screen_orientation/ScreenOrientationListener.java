// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.screen_orientation;

import android.provider.Settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/** Android implementation details for device::ScreenOrientationListenerAndroid. */
@JNINamespace("device")
class ScreenOrientationListener {
    @CalledByNative
    static boolean isAutoRotateEnabledByUser() {
        return Settings.System.getInt(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.System.ACCELEROMETER_ROTATION,
                        0)
                == 1;
    }
}
