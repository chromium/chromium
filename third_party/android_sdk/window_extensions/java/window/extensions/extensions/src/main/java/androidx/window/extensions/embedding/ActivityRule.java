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

import android.app.Activity;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Predicate;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.util.Objects;

/** Split configuration rule for individual activities. */
public class ActivityRule extends EmbeddingRule {
    private final @NonNull Predicate<Activity> mActivityPredicate;
    private final @NonNull Predicate<Intent> mIntentPredicate;
    private final boolean mShouldAlwaysExpand;

    ActivityRule(
            @NonNull Predicate<Activity> activityPredicate,
            @NonNull Predicate<Intent> intentPredicate,
            boolean shouldAlwaysExpand,
            @Nullable String tag) {
        super(tag);
        mActivityPredicate = activityPredicate;
        mIntentPredicate = intentPredicate;
        mShouldAlwaysExpand = shouldAlwaysExpand;
    }

    /** Checks if the rule is applicable to the provided activity. */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public boolean matchesActivity(@NonNull Activity activity) {
        return mActivityPredicate.test(activity);
    }

    /** Checks if the rule is applicable to the provided activity intent. */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public boolean matchesIntent(@NonNull Intent intent) {
        return mIntentPredicate.test(intent);
    }

    /**
     * Indicates whether the activity or activities that are covered by this rule should always be
     * launched in an expanded state and avoid the splits.
     */
    public boolean shouldAlwaysExpand() {
        return mShouldAlwaysExpand;
    }

    /** Builder for {@link ActivityRule}. */
    public static final class Builder {
        private final @NonNull Predicate<Activity> mActivityPredicate;
        private final @NonNull Predicate<Intent> mIntentPredicate;
        private boolean mAlwaysExpand;
        private @Nullable String mTag;

        /**
         * @deprecated Use {@link #Builder(Predicate, Predicate)} starting with {@link
         *     WindowExtensions#VENDOR_API_LEVEL_2}. Only used if {@link #Builder(Predicate,
         *     Predicate)} can't be called on {@link WindowExtensions#VENDOR_API_LEVEL_1}.
         */
        @Deprecated
        @RequiresApi(Build.VERSION_CODES.N)
        public Builder(
                java.util.function.@NonNull Predicate<Activity> activityPredicate,
                java.util.function.@NonNull Predicate<Intent> intentPredicate) {
            mActivityPredicate = activityPredicate::test;
            mIntentPredicate = intentPredicate::test;
        }

        /**
         * The {@link ActivityRule} Builder constructor
         *
         * @param activityPredicate the {@link Predicate} to verify if a given {@link Activity}
         *     matches the rule
         * @param intentPredicate the {@link Predicate} to verify if a given {@link Intent} matches
         *     the rule
         */
        @RequiresVendorApiLevel(level = 2)
        public Builder(
                @NonNull Predicate<Activity> activityPredicate,
                @NonNull Predicate<Intent> intentPredicate) {
            mActivityPredicate = activityPredicate;
            mIntentPredicate = intentPredicate;
        }

        /**
         * @see ActivityRule#shouldAlwaysExpand()
         */
        public @NonNull Builder setShouldAlwaysExpand(boolean alwaysExpand) {
            mAlwaysExpand = alwaysExpand;
            return this;
        }

        /**
         * @see ActivityRule#getTag()
         */
        @RequiresVendorApiLevel(level = 2)
        public @NonNull Builder setTag(@NonNull String tag) {
            mTag = Objects.requireNonNull(tag);
            return this;
        }

        /** Builds a new instance of {@link ActivityRule}. */
        public @NonNull ActivityRule build() {
            return new ActivityRule(mActivityPredicate, mIntentPredicate, mAlwaysExpand, mTag);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof ActivityRule)) return false;
        ActivityRule that = (ActivityRule) o;
        return super.equals(o)
                && mShouldAlwaysExpand == that.mShouldAlwaysExpand
                && mActivityPredicate.equals(that.mActivityPredicate)
                && mIntentPredicate.equals(that.mIntentPredicate);
    }

    @Override
    public int hashCode() {
        int result = super.hashCode();
        result = 31 * result + mActivityPredicate.hashCode();
        result = 31 * result + mIntentPredicate.hashCode();
        result = 31 * result + (mShouldAlwaysExpand ? 1 : 0);
        return result;
    }

    @Override
    public @NonNull String toString() {
        return "ActivityRule{mTag="
                + getTag()
                + ", mShouldAlwaysExpand="
                + mShouldAlwaysExpand
                + '}';
    }
}
