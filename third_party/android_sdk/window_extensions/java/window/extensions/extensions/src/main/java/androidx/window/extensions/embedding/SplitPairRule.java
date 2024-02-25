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

import static androidx.window.extensions.embedding.SplitAttributes.SplitType.createSplitTypeFromLegacySplitRatio;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.util.Pair;
import android.view.WindowMetrics;

import androidx.annotation.FloatRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Predicate;

import java.util.Objects;

/**
 * Split configuration rules for activity pairs.
 */
public class SplitPairRule extends SplitRule {
    @NonNull
    private final Predicate<Pair<Activity, Activity>> mActivityPairPredicate;
    @NonNull
    private final Predicate<Pair<Activity, Intent>> mActivityIntentPredicate;
    @SplitFinishBehavior
    private final int mFinishPrimaryWithSecondary;
    @SplitFinishBehavior
    private final int mFinishSecondaryWithPrimary;
    private final boolean mClearTop;

    SplitPairRule(@NonNull SplitAttributes defaultSplitAttributes,
            @SplitFinishBehavior int finishPrimaryWithSecondary,
            @SplitFinishBehavior int finishSecondaryWithPrimary, boolean clearTop,
            @NonNull Predicate<Pair<Activity, Activity>> activityPairPredicate,
            @NonNull Predicate<Pair<Activity, Intent>> activityIntentPredicate,
            @NonNull Predicate<WindowMetrics> parentWindowMetricsPredicate,
            @Nullable String tag) {
        super(parentWindowMetricsPredicate, defaultSplitAttributes, tag);
        mActivityPairPredicate = activityPairPredicate;
        mActivityIntentPredicate = activityIntentPredicate;
        mFinishPrimaryWithSecondary = finishPrimaryWithSecondary;
        mFinishSecondaryWithPrimary = finishSecondaryWithPrimary;
        mClearTop = clearTop;
    }

    /**
     * Checks if the rule is applicable to the provided activities.
     */
    @SuppressLint("ClassVerificationFailure") // Only called by Extensions implementation on device.
    @RequiresApi(api = Build.VERSION_CODES.N)
    public boolean matchesActivityPair(@NonNull Activity primaryActivity,
            @NonNull Activity secondaryActivity) {
        return mActivityPairPredicate.test(new Pair<>(primaryActivity, secondaryActivity));
    }

    /**
     * Checks if the rule is applicable to the provided primary activity and secondary activity
     * intent.
     */
    @SuppressLint("ClassVerificationFailure") // Only called by Extensions implementation on device.
    @RequiresApi(api = Build.VERSION_CODES.N)
    public boolean matchesActivityIntentPair(@NonNull Activity primaryActivity,
            @NonNull Intent secondaryActivityIntent) {
        return mActivityIntentPredicate.test(new Pair<>(primaryActivity, secondaryActivityIntent));
    }

    /**
     * Determines what happens with the primary container when all activities are finished in the
     * associated secondary container.
     */
    @SplitFinishBehavior
    public int getFinishPrimaryWithSecondary() {
        return mFinishPrimaryWithSecondary;
    }

    /**
     * Determines what happens with the secondary container when all activities are finished in the
     * associated primary container.
     */
    @SplitFinishBehavior
    public int getFinishSecondaryWithPrimary() {
        return mFinishSecondaryWithPrimary;
    }

    /**
     * If there is an existing split with the same primary container, indicates whether the
     * existing secondary container and all activities in it should be destroyed. Otherwise the new
     * secondary will appear on top. Defaults to "true".
     */
    public boolean shouldClearTop() {
        return mClearTop;
    }

    /**
     * Builder for {@link SplitPairRule}.
     */
    public static final class Builder {
        @NonNull
        private final Predicate<Pair<Activity, Activity>> mActivityPairPredicate;
        @NonNull
        private final Predicate<Pair<Activity, Intent>> mActivityIntentPredicate;
        @NonNull
        private final Predicate<WindowMetrics> mParentWindowMetricsPredicate;
        // Keep for backward compatibility
        @FloatRange(from = 0.0, to = 1.0)
        private float mSplitRatio;
        // Keep for backward compatibility
        @SplitAttributes.ExtLayoutDirection
        private int mLayoutDirection;
        private SplitAttributes mDefaultSplitAttributes;
        private boolean mClearTop;
        @SplitFinishBehavior
        private int mFinishPrimaryWithSecondary;
        @SplitFinishBehavior
        private int mFinishSecondaryWithPrimary;
        @Nullable
        private String mTag;

        /**
         * @deprecated Use {@link #Builder(Predicate, Predicate, Predicate)} starting with
         * {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
         * {@link #Builder(Predicate, Predicate, Predicate)} can't be called on
         * {@link WindowExtensions#VENDOR_API_LEVEL_1}.
         */
        @Deprecated
        @RequiresApi(Build.VERSION_CODES.N)
        public Builder(@NonNull java.util.function.Predicate<Pair<Activity, Activity>>
                        activityPairPredicate,
                @NonNull java.util.function.Predicate<Pair<Activity, Intent>>
                        activityIntentPredicate,
                @NonNull java.util.function.Predicate<WindowMetrics>
                        parentWindowMetricsPredicate) {
            mActivityPairPredicate = activityPairPredicate::test;
            mActivityIntentPredicate = activityIntentPredicate::test;
            mParentWindowMetricsPredicate = parentWindowMetricsPredicate::test;
        }

        /**
         * The {@link SplitPairRule} builder constructor
         *
         * @param activityPairPredicate the {@link Predicate} to verify if an {@link Activity} pair
         *                              matches this rule
         * @param activityIntentPredicate the {@link Predicate} to verify if an ({@link Activity},
         *                              {@link Intent}) pair matches this rule
         * @param parentWindowMetricsPredicate the {@link Predicate} to verify if the matched split
         *                               pair is allowed to show adjacent to each other with the
         *                               given parent {@link WindowMetrics}
         * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
         */
        public Builder(@NonNull Predicate<Pair<Activity, Activity>> activityPairPredicate,
                @NonNull Predicate<Pair<Activity, Intent>> activityIntentPredicate,
                @NonNull Predicate<WindowMetrics> parentWindowMetricsPredicate) {
            mActivityPairPredicate = activityPairPredicate;
            mActivityIntentPredicate = activityIntentPredicate;
            mParentWindowMetricsPredicate = parentWindowMetricsPredicate;
        }

        /**
         * @deprecated Use {@link #setDefaultSplitAttributes(SplitAttributes)} starting with
         * {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
         * {@link #setDefaultSplitAttributes(SplitAttributes)} can't be called on
         * {@link WindowExtensions#VENDOR_API_LEVEL_1}. {@code splitRatio} will be translated to
         * {@link SplitAttributes.SplitType.ExpandContainersSplitType} for value {@code 0.0} and
         * {@code 1.0}, and {@link SplitAttributes.SplitType.RatioSplitType} for value with range
         * (0.0, 1.0).
         */
        @Deprecated
        @NonNull
        public Builder setSplitRatio(@FloatRange(from = 0.0, to = 1.0) float splitRatio) {
            mSplitRatio = splitRatio;
            return this;
        }

        /**
         * @deprecated Use {@link #setDefaultSplitAttributes(SplitAttributes)} starting with
         * {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
         * {@link #setDefaultSplitAttributes(SplitAttributes)} can't be called on
         * {@link WindowExtensions#VENDOR_API_LEVEL_1}.
         */
        @Deprecated
        @NonNull
        public Builder setLayoutDirection(@SplitAttributes.ExtLayoutDirection int layoutDirection) {
            mLayoutDirection = layoutDirection;
            return this;
        }

        /**
         * See {@link SplitPairRule#getDefaultSplitAttributes()} for reference.
         * Overrides values if set in {@link #setSplitRatio(float)} and
         * {@link #setLayoutDirection(int)}
         *
         * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
         */
        @NonNull
        public Builder setDefaultSplitAttributes(@NonNull SplitAttributes attrs) {
            mDefaultSplitAttributes = attrs;
            return this;
        }

        /** @deprecated To be removed with next developer preview. */
        @Deprecated
        @NonNull
        public Builder setShouldFinishPrimaryWithSecondary(
                boolean finishPrimaryWithSecondary) {
            return this;
        }

        /** @deprecated To be removed with next developer preview. */
        @Deprecated
        @NonNull
        public Builder setShouldFinishSecondaryWithPrimary(boolean finishSecondaryWithPrimary) {
            return this;
        }

        /** @see SplitPairRule#getFinishPrimaryWithSecondary() */
        @NonNull
        public Builder setFinishPrimaryWithSecondary(@SplitFinishBehavior int finishBehavior) {
            mFinishPrimaryWithSecondary = finishBehavior;
            return this;
        }

        /** @see SplitPairRule#getFinishSecondaryWithPrimary() */
        @NonNull
        public Builder setFinishSecondaryWithPrimary(@SplitFinishBehavior int finishBehavior) {
            mFinishSecondaryWithPrimary = finishBehavior;
            return this;
        }

        /** @see SplitPairRule#shouldClearTop() */
        @NonNull
        public Builder setShouldClearTop(boolean shouldClearTop) {
            mClearTop = shouldClearTop;
            return this;
        }

        /**
         * @see SplitPairRule#getTag()
         * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
         */
        @NonNull
        public Builder setTag(@NonNull String tag) {
            mTag = Objects.requireNonNull(tag);
            return this;
        }

        /** Builds a new instance of {@link SplitPairRule}. */
        @NonNull
        public SplitPairRule build() {
            // To provide compatibility with prior version of WM Jetpack library, where
            // #setDefaultAttributes hasn't yet been supported and thus would not be set.
            mDefaultSplitAttributes = (mDefaultSplitAttributes != null)
                    ? mDefaultSplitAttributes
                    : new SplitAttributes.Builder()
                            .setSplitType(createSplitTypeFromLegacySplitRatio(mSplitRatio))
                            .setLayoutDirection(mLayoutDirection)
                            .build();
            return new SplitPairRule(mDefaultSplitAttributes,
                    mFinishPrimaryWithSecondary, mFinishSecondaryWithPrimary,
                    mClearTop, mActivityPairPredicate, mActivityIntentPredicate,
                    mParentWindowMetricsPredicate, mTag);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof SplitPairRule)) return false;
        SplitPairRule that = (SplitPairRule) o;
        return super.equals(o)
                && mActivityPairPredicate.equals(that.mActivityPairPredicate)
                && mActivityIntentPredicate.equals(that.mActivityIntentPredicate)
                && mFinishPrimaryWithSecondary == that.mFinishPrimaryWithSecondary
                && mFinishSecondaryWithPrimary == that.mFinishSecondaryWithPrimary
                && mClearTop == that.mClearTop;
    }

    @Override
    public int hashCode() {
        int result = super.hashCode();
        result = 31 * result + mActivityPairPredicate.hashCode();
        result = 31 * result + mActivityIntentPredicate.hashCode();
        result = 31 * result + mFinishPrimaryWithSecondary;
        result = 31 * result + mFinishSecondaryWithPrimary;
        result = 31 * result + (mClearTop ? 1 : 0);
        return result;
    }

    @NonNull
    @Override
    public String toString() {
        return "SplitPairRule{"
                + "mTag=" + getTag()
                + ", mDefaultSplitAttributes=" + getDefaultSplitAttributes()
                + ", mFinishPrimaryWithSecondary=" + mFinishPrimaryWithSecondary
                + ", mFinishSecondaryWithPrimary=" + mFinishSecondaryWithPrimary
                + ", mClearTop=" + mClearTop
                + '}';
    }
}
