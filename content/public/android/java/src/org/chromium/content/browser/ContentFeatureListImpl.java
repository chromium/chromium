// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * Implementation of {@link ContentFeatureList}.
 * Java accessor for base/feature_list.h state.
 */
@JNINamespace("content::android")
@MainDex
public class ContentFeatureListImpl {
    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in content/browser/android/content_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return ContentFeatureListImplJni.get().isEnabled(featureName);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     * {@see ContentFeatureList#getFieldTrialParamByFeatureAsInt}
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in content/browser/android/content_feature_list.cc
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        return ContentFeatureListImplJni.get().getFieldTrialParamByFeatureAsInt(
                featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     * {@see ContentFeatureList#getFieldTrialParamByFeatureAsBoolean}
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in content/browser/android/content_feature_list.cc
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        return ContentFeatureListImplJni.get().getFieldTrialParamByFeatureAsBoolean(
                featureName, paramName, defaultValue);
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled(String featureName);
        int getFieldTrialParamByFeatureAsInt(
                String featureName, String paramName, int defaultValue);
        boolean getFieldTrialParamByFeatureAsBoolean(
                String featureName, String paramName, boolean defaultValue);
    }
}
