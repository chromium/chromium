// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;

/** Provides an API for querying the status of WebAuthn features. */
@JNINamespace("webauthn::features")
@NullMarked
public class WebauthnFeatureMap extends FeatureMap {
    private static final WebauthnFeatureMap sInstance = new WebauthnFeatureMap();

    /**
     * @return The singleton instance of WebauthnFeatureMap.
     */
    public static WebauthnFeatureMap getInstance() {
        return sInstance;
    }

    private WebauthnFeatureMap() {}

    /**
     * @param featureName The name of the feature to check.
     * @return Whether the feature is enabled.
     */
    public boolean isEnabled(String featureName) {
        return isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return WebauthnFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    interface Natives {
        long getNativeMap();
    }
}
