/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package androidx.window.extensions.embedding;

import android.content.res.Configuration;
import android.os.Build;
import android.view.WindowMetrics;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.window.extensions.layout.WindowLayoutInfo;

/**
 * The parameter container used to report the current device and window state in
 * {@link ActivityEmbeddingComponent#setSplitAttributesCalculator(
 * androidx.window.extensions.core.util.function.Function)} and references the corresponding
 * {@link SplitRule} by {@link #getSplitRuleTag()} if {@link SplitRule#getTag()} is specified.
 *
 * @see ActivityEmbeddingComponent#clearSplitAttributesCalculator()
 * Since {@link androidx.window.extensions.WindowExtensions#VENDOR_API_LEVEL_2}
 */
public class SplitAttributesCalculatorParams {
    @NonNull
    private final WindowMetrics mParentWindowMetrics;
    @NonNull
    private final Configuration mParentConfiguration;
    @NonNull
    private final WindowLayoutInfo mParentWindowLayoutInfo;
    @NonNull
    private final SplitAttributes mDefaultSplitAttributes;
    private final boolean mAreDefaultConstraintsSatisfied;
    @Nullable
    private final String mSplitRuleTag;

    /** Returns the parent container's {@link WindowMetrics} */
    @NonNull
    public WindowMetrics getParentWindowMetrics() {
        return mParentWindowMetrics;
    }

    /** Returns the parent container's {@link Configuration} */
    @NonNull
    public Configuration getParentConfiguration() {
        return new Configuration(mParentConfiguration);
    }

    /**
     * Returns the {@link SplitRule#getDefaultSplitAttributes()}. It could be from
     * {@link SplitRule} Builder APIs
     * ({@link SplitPairRule.Builder#setDefaultSplitAttributes(SplitAttributes)} or
     * {@link SplitPlaceholderRule.Builder#setDefaultSplitAttributes(SplitAttributes)}) or from
     * the {@code splitRatio} and {@code splitLayoutDirection} attributes from static rule
     * definitions.
     */
    @NonNull
    public SplitAttributes getDefaultSplitAttributes() {
        return mDefaultSplitAttributes;
    }

    /**
     * Returns whether the {@link #getParentWindowMetrics()} satisfies the dimensions and aspect
     * ratios requirements specified in the {@link androidx.window.embedding.SplitRule}, which
     * are:
     *  - {@link androidx.window.embedding.SplitRule#minWidthDp}
     *  - {@link androidx.window.embedding.SplitRule#minHeightDp}
     *  - {@link androidx.window.embedding.SplitRule#minSmallestWidthDp}
     *  - {@link androidx.window.embedding.SplitRule#maxAspectRatioInPortrait}
     *  - {@link androidx.window.embedding.SplitRule#maxAspectRatioInLandscape}
     */
    public boolean areDefaultConstraintsSatisfied() {
        return mAreDefaultConstraintsSatisfied;
    }

    /** Returns the parent container's {@link WindowLayoutInfo} */
    @NonNull
    public WindowLayoutInfo getParentWindowLayoutInfo() {
        return mParentWindowLayoutInfo;
    }

    /**
     * Returns {@link SplitRule#getTag()} to apply the {@link SplitAttributes} result if it was
     * set.
     */
    @Nullable
    public String getSplitRuleTag() {
        return mSplitRuleTag;
    }

    SplitAttributesCalculatorParams(
            @NonNull WindowMetrics parentWindowMetrics,
            @NonNull Configuration parentConfiguration,
            @NonNull WindowLayoutInfo parentWindowLayoutInfo,
            @NonNull SplitAttributes defaultSplitAttributes,
            boolean areDefaultConstraintsSatisfied,
            @Nullable String splitRuleTag
    ) {
        mParentWindowMetrics = parentWindowMetrics;
        mParentConfiguration = parentConfiguration;
        mParentWindowLayoutInfo = parentWindowLayoutInfo;
        mDefaultSplitAttributes = defaultSplitAttributes;
        mAreDefaultConstraintsSatisfied = areDefaultConstraintsSatisfied;
        mSplitRuleTag = splitRuleTag;
    }

    @NonNull
    @Override
    public String toString() {
        return getClass().getSimpleName() + ":{"
                + "windowMetrics=" + windowMetricsToString(mParentWindowMetrics)
                + ", configuration=" + mParentConfiguration
                + ", windowLayoutInfo=" + mParentWindowLayoutInfo
                + ", defaultSplitAttributes=" + mDefaultSplitAttributes
                + ", areDefaultConstraintsSatisfied=" + mAreDefaultConstraintsSatisfied
                + ", tag=" + mSplitRuleTag + "}";
    }

    private static String windowMetricsToString(@NonNull WindowMetrics windowMetrics) {
        // TODO(b/187712731): Use WindowMetrics#toString after it's implemented in U.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Api30Impl.windowMetricsToString(windowMetrics);
        }
        throw new UnsupportedOperationException("WindowMetrics didn't exist in R.");
    }

    @RequiresApi(30)
    private static final class Api30Impl {
        static String windowMetricsToString(@NonNull WindowMetrics windowMetrics) {
            return WindowMetrics.class.getSimpleName() + ":{"
                    + "bounds=" + windowMetrics.getBounds()
                    + ", windowInsets=" + windowMetrics.getWindowInsets()
                    + "}";
        }
    }
}
