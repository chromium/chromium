/*
 * Copyright 2023 The Android Open Source Project
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

import android.view.WindowMetrics;

import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.core.util.function.Predicate;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.util.Objects;

/**
 * Split configuration rules for keeping an {@link ActivityStack} in the split in a pin state to
 * provide an isolated Activity navigation from the split. A pin state here is referring the {@link
 * ActivityStack} to be fixed on top.
 *
 * @see ActivityEmbeddingComponent#pinTopActivityStack
 */
@RequiresVendorApiLevel(level = 5)
public class SplitPinRule extends SplitRule {
    /**
     * Whether the rule should be applied whenever the parent Task satisfied the parent window
     * metrics predicate. See {@link ActivityEmbeddingComponent#pinTopActivityStack}.
     */
    private final boolean mIsSticky;

    SplitPinRule(
            @NonNull SplitAttributes defaultSplitAttributes,
            @NonNull Predicate<WindowMetrics> parentWindowMetricsPredicate,
            boolean isSticky,
            @Nullable String tag) {
        super(parentWindowMetricsPredicate, defaultSplitAttributes, tag);
        mIsSticky = isSticky;
    }

    /**
     * Whether the rule is sticky. This configuration rule can only be applied once when possible.
     * That is, the rule will be abandoned whenever the pinned {@link ActivityStack} no longer able
     * to be split with another {@link ActivityStack} once the configuration of the parent Task is
     * changed. Sets the rule to be sticky if the rule should be permanent until the {@link
     * ActivityStack} explicitly unpin.
     *
     * @see ActivityEmbeddingComponent#pinTopActivityStack
     */
    public boolean isSticky() {
        return mIsSticky;
    }

    /** Builder for {@link SplitPinRule}. */
    public static final class Builder {
        private final @NonNull SplitAttributes mDefaultSplitAttributes;
        private final @NonNull Predicate<WindowMetrics> mParentWindowMetricsPredicate;
        private boolean mIsSticky;
        private @Nullable String mTag;

        /**
         * The {@link SplitPinRule} builder constructor.
         *
         * @param defaultSplitAttributes the default {@link SplitAttributes} to apply
         * @param parentWindowMetricsPredicate the {@link Predicate} to verify if the pinned {@link
         *     ActivityStack} and the one behind are allowed to show adjacent to each other with the
         *     given parent {@link WindowMetrics}
         */
        public Builder(
                @NonNull SplitAttributes defaultSplitAttributes,
                @NonNull Predicate<WindowMetrics> parentWindowMetricsPredicate) {
            mDefaultSplitAttributes = defaultSplitAttributes;
            mParentWindowMetricsPredicate = parentWindowMetricsPredicate;
        }

        /**
         * Sets the rule to be sticky.
         *
         * @see SplitPinRule#isSticky()
         */
        public @NonNull Builder setSticky(boolean isSticky) {
            mIsSticky = isSticky;
            return this;
        }

        /**
         * @see SplitPinRule#getTag()
         */
        public @NonNull Builder setTag(@NonNull String tag) {
            mTag = Objects.requireNonNull(tag);
            return this;
        }

        /** Builds a new instance of {@link SplitPinRule}. */
        public @NonNull SplitPinRule build() {
            return new SplitPinRule(
                    mDefaultSplitAttributes, mParentWindowMetricsPredicate, mIsSticky, mTag);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof SplitPinRule)) return false;
        SplitPinRule that = (SplitPinRule) o;
        return super.equals(o) && mIsSticky == that.mIsSticky;
    }

    @Override
    public int hashCode() {
        int result = super.hashCode();
        result = 31 * result + (mIsSticky ? 1 : 0);
        return result;
    }

    @Override
    public @NonNull String toString() {
        return "SplitPinRule{"
                + "mTag="
                + getTag()
                + ", mDefaultSplitAttributes="
                + getDefaultSplitAttributes()
                + ", mIsSticky="
                + mIsSticky
                + '}';
    }
}
