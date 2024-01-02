/*
 * Copyright 2021 The Android Open Source Project
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

import android.annotation.SuppressLint;
import android.os.Build;
import android.view.WindowMetrics;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Predicate;
import androidx.window.extensions.embedding.SplitAttributes.SplitType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Split configuration rules for activities that are launched to side in a split. Define when an
 * activity that was launched in a side container from another activity should be shown
 * adjacent or on top of it, as well as the visual properties of the split. Can be applied to
 * new activities started from the same process automatically by the embedding implementation on
 * the device.
 */
public abstract class SplitRule extends EmbeddingRule {
    @NonNull
    private final Predicate<WindowMetrics> mParentWindowMetricsPredicate;

    @NonNull
    private final SplitAttributes mDefaultSplitAttributes;

    /**
     * Never finish the associated container.
     * @see SplitFinishBehavior
     */
    public static final int FINISH_NEVER = 0;
    /**
     * Always finish the associated container independent of the current presentation mode.
     * @see SplitFinishBehavior
     */
    public static final int FINISH_ALWAYS = 1;
    /**
     * Only finish the associated container when displayed adjacent to the one being finished. Does
     * not finish the associated one when containers are stacked on top of each other.
     * @see SplitFinishBehavior
     */
    public static final int FINISH_ADJACENT = 2;

    /**
     * Determines what happens with the associated container when all activities are finished in
     * one of the containers in a split.
     * <p>
     * For example, given that {@link SplitPairRule#getFinishPrimaryWithSecondary()} is
     * {@link #FINISH_ADJACENT} and secondary container finishes. The primary associated
     * container is finished if it's shown adjacent to the secondary container. The primary
     * associated container is not finished if it occupies entire task bounds.</p>
     *
     * @see SplitPairRule#getFinishPrimaryWithSecondary()
     * @see SplitPairRule#getFinishSecondaryWithPrimary()
     * @see SplitPlaceholderRule#getFinishPrimaryWithSecondary()
     */
    @IntDef({
            FINISH_NEVER,
            FINISH_ALWAYS,
            FINISH_ADJACENT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SplitFinishBehavior {}

    SplitRule(@NonNull Predicate<WindowMetrics> parentWindowMetricsPredicate,
            @NonNull SplitAttributes defaultSplitAttributes, @Nullable String tag) {
        super(tag);
        mParentWindowMetricsPredicate = parentWindowMetricsPredicate;
        mDefaultSplitAttributes = defaultSplitAttributes;
    }

    /**
     * Checks whether the parent window satisfied the dimensions and aspect ratios requirements
     * specified in the {@link androidx.window.embedding.SplitRule}, which are
     * {@link androidx.window.embedding.SplitRule#minWidthDp},
     * {@link androidx.window.embedding.SplitRule#minHeightDp},
     * {@link androidx.window.embedding.SplitRule#minSmallestWidthDp},
     * {@link androidx.window.embedding.SplitRule#maxAspectRatioInPortrait} and
     * {@link androidx.window.embedding.SplitRule#maxAspectRatioInLandscape}.
     *
     * @param parentMetrics the {@link WindowMetrics} of the parent window.
     * @return whether the parent window satisfied the {@link SplitRule} requirements.
     */
    @SuppressLint("ClassVerificationFailure") // Only called by Extensions implementation on device.
    @RequiresApi(api = Build.VERSION_CODES.N)
    public boolean checkParentMetrics(@NonNull WindowMetrics parentMetrics) {
        return mParentWindowMetricsPredicate.test(parentMetrics);
    }

    /**
     * @deprecated Use {@link #getDefaultSplitAttributes()} instead starting with
     * {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
     * {@link #getDefaultSplitAttributes()} can't be called on
     * {@link WindowExtensions#VENDOR_API_LEVEL_1}.
     */
    @Deprecated
    public float getSplitRatio() {
        final SplitType splitType = mDefaultSplitAttributes.getSplitType();
        if (splitType instanceof SplitType.RatioSplitType) {
            return ((SplitType.RatioSplitType) splitType).getRatio();
        } else { // Fallback to use 0.0 because the WM Jetpack may not support HingeSplitType.
            return 0.0f;
        }
    }

    /**
     * @deprecated Use {@link #getDefaultSplitAttributes()} instead starting with
     * {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
     * {@link #getDefaultSplitAttributes()} can't be called on
     * {@link WindowExtensions#VENDOR_API_LEVEL_1}.
     */
    @Deprecated
    @SplitAttributes.ExtLayoutDirection
    public int getLayoutDirection() {
        return mDefaultSplitAttributes.getLayoutDirection();
    }

    /**
     * Returns the default {@link SplitAttributes} which is applied if
     * {@link #checkParentMetrics(WindowMetrics)} is {@code true}.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    @NonNull
    public SplitAttributes getDefaultSplitAttributes() {
        return mDefaultSplitAttributes;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof SplitRule)) return false;
        SplitRule that = (SplitRule) o;
        return super.equals(that)
                && mDefaultSplitAttributes.equals(that.mDefaultSplitAttributes)
                && mParentWindowMetricsPredicate.equals(that.mParentWindowMetricsPredicate);
    }

    @Override
    public int hashCode() {
        int result = super.hashCode();
        result = 31 * result + mParentWindowMetricsPredicate.hashCode();
        result = 31 * result + Objects.hashCode(mDefaultSplitAttributes);
        return result;
    }

    @NonNull
    @Override
    public String toString() {
        return "SplitRule{"
                + "mTag=" + getTag()
                + ", mDefaultSplitAttributes=" + mDefaultSplitAttributes
                + '}';
    }
}
