// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device;

import android.text.format.DateUtils;

import org.jni_zero.JNINamespace;

import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/**
 * Lists //services/device features that can be accessed through {@link DeviceFeatureMap}.
 *
 * <p>Note: Features must be added to the array |kFeaturesExposedToJava| in
 * //services/device/public/cpp/device_feature_map.cc.
 */
@JNINamespace("features")
@NullMarked
public abstract class DeviceFeatureList {
    public static final String GENERIC_SENSOR_EXTRA_CLASSES = "GenericSensorExtraClasses";
    public static final String BATTERY_STATUS_MANAGER_BROADCAST_RECEIVER_IN_BACKGROUND =
            "BatteryStatusManagerBroadcastReceiverInBackground";
    public static final String WEBAUTHN_ANDROID_SIGNAL = "WebAuthenticationAndroidSignal";
    public static final String WEBAUTHN_PASSKEY_UPGRADE = "WebAuthenticationPasskeyUpgrade";
    public static final String WEBAUTHN_IMMEDIATE_GET = "WebAuthenticationImmediateGet";

    public static final MutableFlagWithSafeDefault sGmsCoreLocationRequestParamOverride =
            newMutableFlagWithSafeDefault("GmsCoreLocationRequestParamOverride", false);
    public static final MutableIntParamWithSafeDefault sGmsCoreLocationRequestUpdateInterval =
            sGmsCoreLocationRequestParamOverride.newIntParam(
                    "location_request_min_update_interval_millis",
                    (int) (9 * DateUtils.SECOND_IN_MILLIS));
    public static final MutableIntParamWithSafeDefault sGmsCoreLocationRequestMaxLocationAge =
            sGmsCoreLocationRequestParamOverride.newIntParam(
                    "location_request_max_location_age_mills",
                    (int) (5 * DateUtils.SECOND_IN_MILLIS));
    public static final MutableFlagWithSafeDefault sWebAuthnImmediateGet =
            newMutableFlagWithSafeDefault(WEBAUTHN_IMMEDIATE_GET, false);
    public static final MutableIntParamWithSafeDefault sWebAuthnImmmediateTimeoutMs =
            sWebAuthnImmediateGet.newIntParam("timeout_ms", 500);

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return DeviceFeatureMap.getInstance().mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
