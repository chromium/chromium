// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base::Features listed in content/browser/android/content_feature_map.cc. */
@JNINamespace("content::android")
public class ContentFeatureMap extends FeatureMap {
    private static final ContentFeatureMap sInstance = new ContentFeatureMap();

    // Do not instantiate this class.
    private ContentFeatureMap() {}

    /**
     * @return the singleton {@link ContentFeatureMap}
     */
    public static ContentFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return ContentFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
