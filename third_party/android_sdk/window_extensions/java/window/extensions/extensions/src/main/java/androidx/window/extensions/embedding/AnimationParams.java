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

package androidx.window.extensions.embedding;

import android.content.res.Resources;

import androidx.annotation.AnimRes;
import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;

import java.util.Objects;

/**
 * A class to represent the animation parameters to use while animating embedding activity
 * containers.
 *
 * @see SplitAttributes.Builder#setAnimationParams
 */
public final class AnimationParams {

    /**
     * The default value for animation resources ID, which means to use the system default
     * animation.
     */
    @RequiresVendorApiLevel(level = 7)
    @SuppressWarnings("ResourceType") // Use as a hint to use the system default animation.
    @AnimRes
    public static final int DEFAULT_ANIMATION_RESOURCES_ID = 0xFFFFFFFF;

    private final @NonNull AnimationBackground mAnimationBackground;

    @AnimRes private final int mOpenAnimationResId;

    @AnimRes private final int mCloseAnimationResId;

    @AnimRes private final int mChangeAnimationResId;

    /**
     * Creates an instance of this {@code AnimationParams}.
     *
     * @param animationBackground The {@link AnimationBackground} to use for the animation involving
     *     this {@code AnimationParams} object.
     * @param openAnimationResId The animation resources ID from the "android" package to use for
     *     open transitions.
     * @param closeAnimationResId The animation resources ID from the "android" package to use for
     *     close transitions.
     * @param changeAnimationResId The animation resources ID from the "android" package to use for
     *     change (resize or move) transitions.
     */
    private AnimationParams(
            @NonNull AnimationBackground animationBackground,
            @AnimRes int openAnimationResId,
            @AnimRes int closeAnimationResId,
            @AnimRes int changeAnimationResId) {
        mAnimationBackground = animationBackground;
        mOpenAnimationResId = openAnimationResId;
        mCloseAnimationResId = closeAnimationResId;
        mChangeAnimationResId = changeAnimationResId;
    }

    /** Returns the {@link AnimationBackground} to use for the background during the animation. */
    @RequiresVendorApiLevel(level = 7)
    public @NonNull AnimationBackground getAnimationBackground() {
        return mAnimationBackground;
    }

    /**
     * Gets the open animation.
     *
     * @return the open animation transition resources ID from the "android" package.
     */
    @RequiresVendorApiLevel(level = 7)
    @AnimRes
    public int getOpenAnimationResId() {
        return mOpenAnimationResId;
    }

    /**
     * Gets the close animation.
     *
     * @return the close animation transition resources ID from the "android" package.
     */
    @RequiresVendorApiLevel(level = 7)
    @AnimRes
    public int getCloseAnimationResId() {
        return mCloseAnimationResId;
    }

    /**
     * Gets the change (resize or move) animation.
     *
     * @return the change (resize or move) animation transition resources ID from the "android"
     *     package.
     */
    @RequiresVendorApiLevel(level = 7)
    @AnimRes
    public int getChangeAnimationResId() {
        return mChangeAnimationResId;
    }

    /**
     * Builder for creating an instance of {@link AnimationParams}.
     *
     * <p>- The default animation background is to use the current theme window background color. -
     * The default animation resources ID's for transitions is the system default.
     */
    public static final class Builder {
        private @NonNull AnimationBackground mAnimationBackground =
                AnimationBackground.ANIMATION_BACKGROUND_DEFAULT;

        @AnimRes private int mOpenAnimationResId = DEFAULT_ANIMATION_RESOURCES_ID;

        @AnimRes private int mCloseAnimationResId = DEFAULT_ANIMATION_RESOURCES_ID;

        @AnimRes private int mChangeAnimationResId = DEFAULT_ANIMATION_RESOURCES_ID;

        /** Creates a new {@link AnimationParams.Builder} to create {@link AnimationParams}. */
        @RequiresVendorApiLevel(level = 7)
        public Builder() {}

        /**
         * Sets the {@link AnimationBackground} to use for the background during the animation. The
         * default value is {@link AnimationBackground#ANIMATION_BACKGROUND_DEFAULT}, which means to
         * use the current theme window background color.
         *
         * @param background An {@link AnimationBackground} to be used for the animation.
         * @return this {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 7)
        public AnimationParams.@NonNull Builder setAnimationBackground(
                @NonNull AnimationBackground background) {
            mAnimationBackground = background;
            return this;
        }

        /**
         * Sets the open animation. Use {@link #DEFAULT_ANIMATION_RESOURCES_ID} for the system
         * default animation. Use {@code 0} or {@link Resources#ID_NULL} for no animation.
         *
         * @param resId The resources ID to set from the "android" package.
         * @return this {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 7)
        public AnimationParams.@NonNull Builder setOpenAnimationResId(@AnimRes int resId) {
            mOpenAnimationResId = resId;
            return this;
        }

        /**
         * Sets the close animation. Use {@link #DEFAULT_ANIMATION_RESOURCES_ID} for the system
         * default animation. Use {@code 0} or {@link Resources#ID_NULL} for no animation.
         *
         * @param resId The resources ID to set from the "android" package.
         * @return this {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 7)
        public AnimationParams.@NonNull Builder setCloseAnimationResId(@AnimRes int resId) {
            mCloseAnimationResId = resId;
            return this;
        }

        /**
         * Sets the change (resize or move) animation. Use {@link #DEFAULT_ANIMATION_RESOURCES_ID}
         * for the system default animation. Use {@code 0} or {@link Resources#ID_NULL} for no
         * animation.
         *
         * @param resId The resources ID to set from the "android" package.
         * @return this {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 7)
        public AnimationParams.@NonNull Builder setChangeAnimationResId(@AnimRes int resId) {
            mChangeAnimationResId = resId;
            return this;
        }

        /**
         * Builds a {@link AnimationParams} instance with the attributes specified by {@link
         * #setAnimationBackground(AnimationBackground)}, {@link #setOpenAnimationResId(int)},
         * {@link #setCloseAnimationResId(int)}, and {@link #setChangeAnimationResId(int)}.
         *
         * @return the new {@code AnimationParams} instance.
         */
        @RequiresVendorApiLevel(level = 7)
        public @NonNull AnimationParams build() {
            return new AnimationParams(
                    mAnimationBackground,
                    mOpenAnimationResId,
                    mCloseAnimationResId,
                    mChangeAnimationResId);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof AnimationParams)) return false;
        AnimationParams that = (AnimationParams) o;
        return mAnimationBackground.equals(that.mAnimationBackground)
                && mOpenAnimationResId == that.mOpenAnimationResId
                && mCloseAnimationResId == that.mCloseAnimationResId
                && mChangeAnimationResId == that.mChangeAnimationResId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mAnimationBackground,
                mOpenAnimationResId,
                mCloseAnimationResId,
                mChangeAnimationResId);
    }

    @Override
    public @NonNull String toString() {
        return AnimationParams.class.getSimpleName()
                + "{"
                + "animationBackground="
                + mAnimationBackground
                + ", openAnimation="
                + mOpenAnimationResId
                + ", closeAnimation="
                + mCloseAnimationResId
                + ", changeAnimation="
                + mChangeAnimationResId
                + "}";
    }
}
