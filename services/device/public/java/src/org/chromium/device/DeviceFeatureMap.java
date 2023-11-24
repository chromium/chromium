// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base::Features listed in {@link DeviceFeatureList} */
@JNINamespace("features")
public final class DeviceFeatureMap extends FeatureMap {
    private static final DeviceFeatureMap sInstance = new DeviceFeatureMap();

    // Do not instantiate this class.
    private DeviceFeatureMap() {}

    /**
     * @return the singleton DeviceFeatureMap.
     */
    public static DeviceFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return DeviceFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
