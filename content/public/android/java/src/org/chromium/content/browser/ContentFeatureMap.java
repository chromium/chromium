// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.FeatureMap;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base::Features listed in content/browser/android/content_feature_map.cc.
 */
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

    @Override
    protected long getNativeMap() {
        return ContentFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
