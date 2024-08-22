// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device;

import org.jni_zero.JNINamespace;

/**
 * Lists //services/device features that can be accessed through {@link DeviceFeatureMap}.
 *
 * <p>Note: Features must be added to the array |kFeaturesExposedToJava| in
 * //services/device/public/cpp/device_feature_map.cc.
 */
@JNINamespace("features")
public abstract class DeviceFeatureList {
    public static final String GENERIC_SENSOR_EXTRA_CLASSES = "GenericSensorExtraClasses";
    public static final String WEBAUTHN_ANDROID_CRED_MAN = "WebAuthenticationAndroidCredMan";
}
