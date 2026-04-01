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

import static androidx.window.extensions.embedding.WindowAttributes.DIM_AREA_ON_ACTIVITY_STACK;

import android.graphics.Rect;

import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

/** Attributes used to update the layout and configuration of an {@link ActivityStack}. */
public final class ActivityStackAttributes {

    private final @NonNull Rect mRelativeBounds;

    private final @NonNull WindowAttributes mWindowAttributes;

    private ActivityStackAttributes(
            @NonNull Rect relativeBounds, @NonNull WindowAttributes windowAttributes) {
        mRelativeBounds = relativeBounds;
        mWindowAttributes = windowAttributes;
    }

    /**
     * Returns the requested bounds of an {@link ActivityStack} which relative to its parent
     * container.
     *
     * <p>{@link Rect#isEmpty() Empty} bounds mean that this {@link ActivityStack} should fill its
     * parent container bounds.
     */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull Rect getRelativeBounds() {
        return mRelativeBounds;
    }

    /**
     * Returns the {@link WindowAttributes} which contains the configurations of the embedded
     * Activity windows with this attributes.
     */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull WindowAttributes getWindowAttributes() {
        return mWindowAttributes;
    }

    @Override
    public int hashCode() {
        return mRelativeBounds.hashCode() * 31 + mWindowAttributes.hashCode();
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof ActivityStackAttributes)) return false;
        final ActivityStackAttributes attrs = (ActivityStackAttributes) obj;
        return mRelativeBounds.equals(attrs.mRelativeBounds)
                && mWindowAttributes.equals(attrs.mWindowAttributes);
    }

    @Override
    public @NonNull String toString() {
        return ActivityStackAttributes.class.getSimpleName()
                + ": {"
                + " relativeBounds="
                + mRelativeBounds
                + ", windowAttributes="
                + mWindowAttributes
                + "}";
    }

    /** The builder class of {@link ActivityStackAttributes}. */
    public static final class Builder {

        /** The {@link ActivityStackAttributes} builder constructor. */
        @RequiresVendorApiLevel(level = 6)
        public Builder() {}

        private final @NonNull Rect mRelativeBounds = new Rect();

        private @NonNull WindowAttributes mWindowAttributes =
                new WindowAttributes(DIM_AREA_ON_ACTIVITY_STACK);

        /**
         * Sets the requested relative bounds of the {@link ActivityStack}. If this value is not
         * specified, {@link #getRelativeBounds()} defaults to {@link Rect#isEmpty() empty} bounds,
         * which means to follow the parent container bounds.
         *
         * @param relativeBounds The requested relative bounds.
         * @return This {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setRelativeBounds(@NonNull Rect relativeBounds) {
            mRelativeBounds.set(relativeBounds);
            return this;
        }

        /**
         * Sets the window attributes. If this value is not specified, the {@link
         * WindowAttributes#getDimAreaBehavior()} will be only applied on the {@link ActivityStack}
         * of the requested activity.
         *
         * @param attributes The {@link WindowAttributes}
         * @return This {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setWindowAttributes(@NonNull WindowAttributes attributes) {
            mWindowAttributes = attributes;
            return this;
        }

        /** Builds an {@link ActivityStackAttributes} instance. */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull ActivityStackAttributes build() {
            return new ActivityStackAttributes(mRelativeBounds, mWindowAttributes);
        }
    }
}
