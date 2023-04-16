// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.audio;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * Provides an API for querying the status of Audio Service Features.
 */
// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList exists.
@JNINamespace("features")
@MainDex
public class AudioFeatureList {
    public static final String BLOCK_MIDI_BY_DEFAULT = "BlockMidiByDefault";

    private AudioFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in //services/audio/public/cpp/audio_feature_list.cc.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        assert FeatureList.isNativeInitialized();
        return AudioFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
