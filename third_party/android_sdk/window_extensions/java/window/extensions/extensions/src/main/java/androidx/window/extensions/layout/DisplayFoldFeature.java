/*
 * Copyright 2024 The Android Open Source Project
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

package androidx.window.extensions.layout;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.RestrictTo;
import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.core.util.function.Consumer;
import androidx.window.extensions.util.SetUtilApi23;

import org.jspecify.annotations.NonNull;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

/**
 * Represents a fold on a display that may intersect a window. The presence of a fold does not imply
 * that it intersects the window an {@link android.app.Activity} is running in. For example, on a
 * device that can fold like a book and has an outer screen, the fold should be reported regardless
 * of the folding state, or which screen is on to indicate that there may be a fold when the user
 * opens the device.
 *
 * @see WindowLayoutComponent#addWindowLayoutInfoListener(Context, Consumer) to listen to features
 *     that affect the window.
 */
public final class DisplayFoldFeature {

    /**
     * The type of fold is unknown. This is here for compatibility reasons if a new type is added,
     * and cannot be reported to an incompatible application.
     */
    public static final int TYPE_UNKNOWN = 0;

    /** The type of fold is a physical hinge separating two display panels. */
    public static final int TYPE_HINGE = 1;

    /** The type of fold is a screen that folds from 0-180. */
    public static final int TYPE_SCREEN_FOLD_IN = 2;

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(value = {TYPE_UNKNOWN, TYPE_HINGE, TYPE_SCREEN_FOLD_IN})
    public @interface FoldType {}

    /** The fold supports the half opened state. */
    public static final int FOLD_PROPERTY_SUPPORTS_HALF_OPENED = 1;

    @Target(ElementType.TYPE_USE)
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(value = {FOLD_PROPERTY_SUPPORTS_HALF_OPENED})
    public @interface FoldProperty {}

    @FoldType private final int mType;

    private final Set<@FoldProperty Integer> mProperties;

    /**
     * Creates an instance of [FoldDisplayFeature].
     *
     * @param type the type of fold, either [FoldDisplayFeature.TYPE_HINGE] or
     *     [FoldDisplayFeature.TYPE_FOLDABLE_SCREEN]
     */
    DisplayFoldFeature(@FoldType int type, @NonNull Set<@FoldProperty Integer> properties) {
        mType = type;
        if (Build.VERSION.SDK_INT >= 23) {
            mProperties = SetUtilApi23.createSet();
        } else {
            mProperties = new HashSet<>();
        }
        mProperties.addAll(properties);
    }

    /** Returns the type of fold that is either a hinge or a fold. */
    @RequiresVendorApiLevel(level = 6)
    @FoldType
    public int getType() {
        return mType;
    }

    /** Returns {@code true} if the fold has the given property, {@code false} otherwise. */
    @RequiresVendorApiLevel(level = 6)
    public boolean hasProperty(@FoldProperty int property) {
        return mProperties.contains(property);
    }

    /** Returns {@code true} if the fold has all the given properties, {@code false} otherwise. */
    @RequiresVendorApiLevel(level = 6)
    public boolean hasProperties(@FoldProperty int @NonNull ... properties) {
        for (int i = 0; i < properties.length; i++) {
            if (!mProperties.contains(properties[i])) {
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        DisplayFoldFeature that = (DisplayFoldFeature) o;
        return mType == that.mType && Objects.equals(mProperties, that.mProperties);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mType, mProperties);
    }

    @Override
    public @NonNull String toString() {
        return "ScreenFoldDisplayFeature{mType=" + mType + ", mProperties=" + mProperties + '}';
    }

    /** A builder to construct an instance of {@link DisplayFoldFeature}. */
    public static final class Builder {

        @FoldType private int mType;

        private Set<@FoldProperty Integer> mProperties;

        /**
         * Constructs a builder to create an instance of {@link DisplayFoldFeature}.
         *
         * @param type the type of hinge for the {@link DisplayFoldFeature}.
         * @see DisplayFoldFeature.FoldType
         */
        @RequiresVendorApiLevel(level = 6)
        public Builder(@FoldType int type) {
            mType = type;
            if (Build.VERSION.SDK_INT >= 23) {
                mProperties = SetUtilApi23.createSet();
            } else {
                mProperties = new HashSet<>();
            }
        }

        /** Add a property to the set of properties exposed by {@link DisplayFoldFeature}. */
        @SuppressLint("MissingGetterMatchingBuilder")
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder addProperty(@FoldProperty int property) {
            mProperties.add(property);
            return this;
        }

        /**
         * Add a list of properties to the set of properties exposed by {@link DisplayFoldFeature}.
         */
        @SuppressLint("MissingGetterMatchingBuilder")
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder addProperties(@FoldProperty int @NonNull ... properties) {
            for (int i = 0; i < properties.length; i++) {
                mProperties.add(properties[i]);
            }
            return this;
        }

        /** Clear the properties in the builder. */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder clearProperties() {
            mProperties.clear();
            return this;
        }

        /** Returns an instance of {@link DisplayFoldFeature}. */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull DisplayFoldFeature build() {
            return new DisplayFoldFeature(mType, mProperties);
        }
    }
}
