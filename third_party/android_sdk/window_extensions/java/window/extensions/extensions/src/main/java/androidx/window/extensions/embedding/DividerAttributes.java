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

import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Dimension;
import androidx.annotation.IntDef;
import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * The attributes of the divider layout and behavior.
 *
 * @see SplitAttributes.Builder#setDividerAttributes(DividerAttributes)
 */
public final class DividerAttributes {

    /** A divider type that draws a static line between the primary and secondary containers. */
    public static final int DIVIDER_TYPE_FIXED = 1;

    /**
     * A divider type that draws a line between the primary and secondary containers with a drag
     * handle that the user can drag and resize the containers.
     */
    public static final int DIVIDER_TYPE_DRAGGABLE = 2;

    @IntDef({DIVIDER_TYPE_FIXED, DIVIDER_TYPE_DRAGGABLE})
    @Retention(RetentionPolicy.SOURCE)
    @interface DividerType {}

    /**
     * A special value to indicate that the ratio is unset. which means the system will choose a
     * default value based on the display size and form factor.
     *
     * @see #getPrimaryMinRatio()
     * @see #getPrimaryMaxRatio()
     */
    public static final float RATIO_SYSTEM_DEFAULT = -1.0f;

    /**
     * A special value to indicate that the width is unset. which means the system will choose a
     * default value based on the display size and form factor.
     *
     * @see #getWidthDp()
     */
    public static final int WIDTH_SYSTEM_DEFAULT = -1;

    /**
     * The default value for the veil color. When used, the activity window background color will be
     * used.
     *
     * @see #getPrimaryVeilColor()
     * @see #getSecondaryVeilColor()
     */
    public static final int DIVIDER_VEIL_COLOR_DEFAULT = Color.TRANSPARENT;

    /** The {@link DividerType}. */
    private final @DividerType int mDividerType;

    /**
     * The divider width in dp. It defaults to {@link #WIDTH_SYSTEM_DEFAULT}, which means the system
     * will choose a default value based on the display size and form factor.
     */
    private final @Dimension int mWidthDp;

    /**
     * The min split ratio for the primary container. It defaults to {@link #RATIO_SYSTEM_DEFAULT},
     * the system will choose a default value based on the display size and form factor. Will only
     * be used when the divider type is {@link #DIVIDER_TYPE_DRAGGABLE}.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code true}, the user is allowed to drag
     * beyond this ratio, and when dragging is finished, the system will choose to either fully
     * expand the secondary container or move the divider back to this ratio.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code false}, the user is not allowed to
     * drag beyond this ratio.
     *
     * @see SplitAttributes.SplitType.RatioSplitType#getRatio()
     */
    private final float mPrimaryMinRatio;

    /**
     * The max split ratio for the primary container. It defaults to {@link #RATIO_SYSTEM_DEFAULT},
     * the system will choose a default value based on the display size and form factor. Will only
     * be used when the divider type is {@link #DIVIDER_TYPE_DRAGGABLE}.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code true}, the user is allowed to drag
     * beyond this ratio, and when dragging is finished, the system will choose to either fully
     * expand the primary container or move the divider back to this ratio.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code false}, the user is not allowed to
     * drag beyond this ratio.
     *
     * @see SplitAttributes.SplitType.RatioSplitType#getRatio()
     */
    private final float mPrimaryMaxRatio;

    /** The color of the divider. */
    private final @ColorInt int mDividerColor;

    /** Whether it is allowed to expand a container to full screen by dragging the divider. */
    private final boolean mIsDraggingToFullscreenAllowed;

    /** The veil color of the primary container while dragging. */
    private final @ColorInt int mPrimaryVeilColor;

    /** The veil color of the secondary container while dragging. */
    private final @ColorInt int mSecondaryVeilColor;

    /**
     * Constructor of {@link DividerAttributes}.
     *
     * @param dividerType the divider type. See {@link DividerType}.
     * @param widthDp the width of the divider.
     * @param primaryMinRatio the min split ratio for the primary container.
     * @param primaryMaxRatio the max split ratio for the primary container.
     * @param dividerColor the color of the divider.
     * @param isDraggingToFullscreenAllowed whether it is allowed to expand a container to full
     *     screen by dragging the divider.
     * @param primaryVeilColor the veil color of the primary container while dragging. If {@link
     *     #DIVIDER_VEIL_COLOR_DEFAULT}, activity window background color is used.
     * @param secondaryVeilColor the veil color of the secondary container while dragging. If {@link
     *     #DIVIDER_VEIL_COLOR_DEFAULT}, activity window background color is used.
     * @throws IllegalStateException if the provided values are invalid.
     */
    private DividerAttributes(
            @DividerType int dividerType,
            @Dimension int widthDp,
            float primaryMinRatio,
            float primaryMaxRatio,
            @ColorInt int dividerColor,
            boolean isDraggingToFullscreenAllowed,
            @ColorInt int primaryVeilColor,
            @ColorInt int secondaryVeilColor) {
        if (dividerType == DIVIDER_TYPE_FIXED
                && (primaryMinRatio != RATIO_SYSTEM_DEFAULT
                        || primaryMaxRatio != RATIO_SYSTEM_DEFAULT)) {
            throw new IllegalStateException(
                    "primaryMinRatio and primaryMaxRatio must be RATIO_SYSTEM_DEFAULT for "
                            + "DIVIDER_TYPE_FIXED.");
        }
        if (dividerType == DIVIDER_TYPE_FIXED
                && (primaryVeilColor != DIVIDER_VEIL_COLOR_DEFAULT
                        || secondaryVeilColor != DIVIDER_VEIL_COLOR_DEFAULT)) {
            throw new IllegalStateException(
                    "primaryVeilColor and secondaryVeilColor must be unset for"
                            + "DIVIDER_TYPE_FIXED.");
        }
        if (primaryMinRatio != RATIO_SYSTEM_DEFAULT
                && primaryMaxRatio != RATIO_SYSTEM_DEFAULT
                && primaryMinRatio > primaryMaxRatio) {
            throw new IllegalStateException(
                    "primaryMinRatio must be less than or equal to primaryMaxRatio");
        }
        mDividerType = dividerType;
        mWidthDp = widthDp;
        mPrimaryMinRatio = primaryMinRatio;
        mPrimaryMaxRatio = primaryMaxRatio;
        mDividerColor = dividerColor;
        mIsDraggingToFullscreenAllowed = isDraggingToFullscreenAllowed;
        mPrimaryVeilColor = primaryVeilColor;
        mSecondaryVeilColor = secondaryVeilColor;
    }

    /**
     * Returns the divider type.
     *
     * @see #DIVIDER_TYPE_FIXED
     * @see #DIVIDER_TYPE_DRAGGABLE
     */
    @RequiresVendorApiLevel(level = 6)
    public @DividerType int getDividerType() {
        return mDividerType;
    }

    /**
     * Returns the width of the divider. It defaults to {@link #WIDTH_SYSTEM_DEFAULT}, which means
     * the system will choose a default value based on the display size and form factor.
     */
    @RequiresVendorApiLevel(level = 6)
    public @Dimension int getWidthDp() {
        return mWidthDp;
    }

    /**
     * Returns the min split ratio for the primary container the divider can be dragged to. It
     * defaults to {@link #RATIO_SYSTEM_DEFAULT}, which means the system will choose a default value
     * based on the display size and form factor. Will only be used when the divider type is {@link
     * #DIVIDER_TYPE_DRAGGABLE}.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code true}, the user is allowed to drag
     * beyond this ratio, and when dragging is finished, the system will choose to either fully
     * expand the secondary container or move the divider back to this ratio.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code false}, the user is not allowed to
     * drag beyond this ratio.
     *
     * @see SplitAttributes.SplitType.RatioSplitType#getRatio()
     */
    @RequiresVendorApiLevel(level = 6)
    public float getPrimaryMinRatio() {
        return mPrimaryMinRatio;
    }

    /**
     * Returns the max split ratio for the primary container the divider can be dragged to. It
     * defaults to {@link #RATIO_SYSTEM_DEFAULT}, which means the system will choose a default value
     * based on the display size and form factor. Will only be used when the divider type is {@link
     * #DIVIDER_TYPE_DRAGGABLE}.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code true}, the user is allowed to drag
     * beyond this ratio, and when dragging is finished, the system will choose to either fully
     * expand the primary container or move the divider back to this ratio.
     *
     * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code false}, the user is not allowed to
     * drag beyond this ratio.
     *
     * @see SplitAttributes.SplitType.RatioSplitType#getRatio()
     */
    @RequiresVendorApiLevel(level = 6)
    public float getPrimaryMaxRatio() {
        return mPrimaryMaxRatio;
    }

    /** Returns the color of the divider. */
    @RequiresVendorApiLevel(level = 6)
    public @ColorInt int getDividerColor() {
        return mDividerColor;
    }

    /**
     * Returns whether it is allowed to expand a container to full screen by dragging the divider.
     * Default is {@code true}.
     */
    @RequiresVendorApiLevel(level = 7)
    public boolean isDraggingToFullscreenAllowed() {
        return mIsDraggingToFullscreenAllowed;
    }

    /**
     * Returns the veil color of the primary container. {@link #DIVIDER_VEIL_COLOR_DEFAULT}
     * indicates that activity window background color should be used.
     */
    @RequiresVendorApiLevel(level = 8)
    public @ColorInt int getPrimaryVeilColor() {
        return mPrimaryVeilColor;
    }

    /**
     * Returns the veil color of the secondary container. {@link #DIVIDER_VEIL_COLOR_DEFAULT}
     * indicates that activity window background color should be used.
     */
    @RequiresVendorApiLevel(level = 8)
    public @ColorInt int getSecondaryVeilColor() {
        return mSecondaryVeilColor;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof DividerAttributes)) return false;
        final DividerAttributes other = (DividerAttributes) obj;
        return mDividerType == other.mDividerType
                && mWidthDp == other.mWidthDp
                && mPrimaryMinRatio == other.mPrimaryMinRatio
                && mPrimaryMaxRatio == other.mPrimaryMaxRatio
                && mDividerColor == other.mDividerColor
                && mIsDraggingToFullscreenAllowed == other.mIsDraggingToFullscreenAllowed
                && mPrimaryVeilColor == other.mPrimaryVeilColor
                && mSecondaryVeilColor == other.mSecondaryVeilColor;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mDividerType,
                mWidthDp,
                mPrimaryMinRatio,
                mPrimaryMaxRatio,
                mIsDraggingToFullscreenAllowed,
                mPrimaryVeilColor,
                mSecondaryVeilColor);
    }

    @Override
    public @NonNull String toString() {
        return DividerAttributes.class.getSimpleName()
                + "{"
                + "dividerType="
                + mDividerType
                + ", width="
                + mWidthDp
                + ", minPrimaryRatio="
                + mPrimaryMinRatio
                + ", maxPrimaryRatio="
                + mPrimaryMaxRatio
                + ", dividerColor="
                + mDividerColor
                + ", isDraggingToFullscreenAllowed="
                + mIsDraggingToFullscreenAllowed
                + ", mPrimaryVeilColor="
                + mPrimaryVeilColor
                + ", mSecondaryVeilColor="
                + mSecondaryVeilColor
                + "}";
    }

    /** The {@link DividerAttributes} builder. */
    public static final class Builder {

        private final @DividerType int mDividerType;

        private @Dimension int mWidthDp = WIDTH_SYSTEM_DEFAULT;

        private float mPrimaryMinRatio = RATIO_SYSTEM_DEFAULT;

        private float mPrimaryMaxRatio = RATIO_SYSTEM_DEFAULT;

        private @ColorInt int mDividerColor = Color.BLACK;

        private boolean mIsDraggingToFullscreenAllowed = false;

        private @ColorInt int mPrimaryVeilColor = DIVIDER_VEIL_COLOR_DEFAULT;

        private @ColorInt int mSecondaryVeilColor = DIVIDER_VEIL_COLOR_DEFAULT;

        /**
         * The {@link DividerAttributes} builder constructor.
         *
         * @param dividerType the divider type, possible values are {@link #DIVIDER_TYPE_FIXED} and
         *     {@link #DIVIDER_TYPE_DRAGGABLE}.
         */
        @RequiresVendorApiLevel(level = 6)
        public Builder(@DividerType int dividerType) {
            mDividerType = dividerType;
        }

        /**
         * The {@link DividerAttributes} builder constructor initialized by an existing {@link
         * DividerAttributes}.
         *
         * @param original the original {@link DividerAttributes} to initialize the {@link Builder}.
         */
        @RequiresVendorApiLevel(level = 6)
        public Builder(@NonNull DividerAttributes original) {
            Objects.requireNonNull(original);
            mDividerType = original.mDividerType;
            mWidthDp = original.mWidthDp;
            mPrimaryMinRatio = original.mPrimaryMinRatio;
            mPrimaryMaxRatio = original.mPrimaryMaxRatio;
            mDividerColor = original.mDividerColor;
            mIsDraggingToFullscreenAllowed = original.mIsDraggingToFullscreenAllowed;
            mPrimaryVeilColor = original.mPrimaryVeilColor;
            mSecondaryVeilColor = original.mSecondaryVeilColor;
        }

        /**
         * Sets the divider width. It defaults to {@link #WIDTH_SYSTEM_DEFAULT}, which means the
         * system will choose a default value based on the display size and form factor.
         *
         * @throws IllegalArgumentException if the provided value is invalid.
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setWidthDp(@Dimension int widthDp) {
            if (widthDp != WIDTH_SYSTEM_DEFAULT && widthDp < 0) {
                throw new IllegalArgumentException(
                        "widthDp must be greater than or equal to 0 or WIDTH_SYSTEM_DEFAULT.");
            }
            mWidthDp = widthDp;
            return this;
        }

        /**
         * Sets the min split ratio for the primary container. It defaults to {@link
         * #RATIO_SYSTEM_DEFAULT}, which means the system will choose a default value based on the
         * display size and form factor. Will only be used when the divider type is {@link
         * #DIVIDER_TYPE_DRAGGABLE}.
         *
         * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code true}, the user is allowed to
         * drag beyond this ratio, and when dragging is finished, the system will choose to either
         * fully expand the secondary container or move the divider back to this ratio.
         *
         * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code false}, the user is not allowed
         * to drag beyond this ratio.
         *
         * @param primaryMinRatio the min ratio for the primary container. Must be in range [0.0,
         *     1.0) or {@link #RATIO_SYSTEM_DEFAULT}.
         * @throws IllegalArgumentException if the provided value is invalid.
         * @see SplitAttributes.SplitType.RatioSplitType#getRatio()
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setPrimaryMinRatio(float primaryMinRatio) {
            if (primaryMinRatio != RATIO_SYSTEM_DEFAULT
                    && (primaryMinRatio >= 1.0 || primaryMinRatio < 0.0)) {
                throw new IllegalArgumentException(
                        "primaryMinRatio must be in [0.0, 1.0) or RATIO_SYSTEM_DEFAULT.");
            }
            mPrimaryMinRatio = primaryMinRatio;
            return this;
        }

        /**
         * Sets the max split ratio for the primary container. It defaults to {@link
         * #RATIO_SYSTEM_DEFAULT}, which means the system will choose a default value based on the
         * display size and form factor. Will only be used when the divider type is {@link
         * #DIVIDER_TYPE_DRAGGABLE}.
         *
         * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code true}, the user is allowed to
         * drag beyond this ratio, and when dragging is finished, the system will choose to either
         * fully expand the primary container or move the divider back to this ratio.
         *
         * <p>If {@link #isDraggingToFullscreenAllowed()} is {@code false}, the user is not allowed
         * to drag beyond this ratio.
         *
         * @param primaryMaxRatio the max ratio for the primary container. Must be in range (0.0,
         *     1.0] or {@link #RATIO_SYSTEM_DEFAULT}.
         * @throws IllegalArgumentException if the provided value is invalid.
         * @see SplitAttributes.SplitType.RatioSplitType#getRatio()
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setPrimaryMaxRatio(float primaryMaxRatio) {
            if (primaryMaxRatio != RATIO_SYSTEM_DEFAULT
                    && (primaryMaxRatio > 1.0 || primaryMaxRatio <= 0.0)) {
                throw new IllegalArgumentException(
                        "primaryMaxRatio must be in (0.0, 1.0] or RATIO_SYSTEM_DEFAULT.");
            }
            mPrimaryMaxRatio = primaryMaxRatio;
            return this;
        }

        /**
         * Sets the color of the divider. If not set, the default color {@link Color#BLACK} is used.
         * Only the RGB components are used and the alpha value is ignored and always considered as
         * fully opaque.
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setDividerColor(@ColorInt int dividerColor) {
            mDividerColor = dividerColor;
            return this;
        }

        /**
         * Sets whether it is allowed to expand a container to full screen by dragging the divider.
         * Default is {@code true}.
         */
        @RequiresVendorApiLevel(level = 7)
        public @NonNull Builder setDraggingToFullscreenAllowed(
                boolean isDraggingToFullscreenAllowed) {
            mIsDraggingToFullscreenAllowed = isDraggingToFullscreenAllowed;
            return this;
        }

        /**
         * Sets the veil color of the primary container. Solid color veils are used to cover
         * activity content while dragging.
         *
         * <p>The default value is {@link #DIVIDER_VEIL_COLOR_DEFAULT}.
         *
         * @param color the veil color for the primary container. If the value equals to {@link
         *     #DIVIDER_VEIL_COLOR_DEFAULT}, activity window background color is used. If {@link
         *     Color#TRANSPARENT} is used, it is treated as {@link #DIVIDER_VEIL_COLOR_DEFAULT}.
         *     Only the RGB components are used and the alpha value is ignored and always considered
         *     as fully opaque.
         */
        @RequiresVendorApiLevel(level = 8)
        public @NonNull Builder setPrimaryVeilColor(@ColorInt int color) {
            mPrimaryVeilColor = color;
            return this;
        }

        /**
         * Sets the veil color of the secondary container. Solid color veils are used to cover
         * activity content while dragging.
         *
         * <p>The default value is {@link #DIVIDER_VEIL_COLOR_DEFAULT}.
         *
         * @param color the veil color for the secondary container. If the value equals to {@link
         *     #DIVIDER_VEIL_COLOR_DEFAULT}, activity window background color is used. If {@link
         *     Color#TRANSPARENT} is used, it is treated as {@link #DIVIDER_VEIL_COLOR_DEFAULT}.
         *     Only the RGB components are used and the alpha value is ignored and always considered
         *     as fully opaque.
         */
        @RequiresVendorApiLevel(level = 8)
        public @NonNull Builder setSecondaryVeilColor(@ColorInt int color) {
            mSecondaryVeilColor = color;
            return this;
        }

        /**
         * Builds a {@link DividerAttributes} instance.
         *
         * @return a {@link DividerAttributes} instance.
         * @throws IllegalArgumentException if the provided values are invalid.
         */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull DividerAttributes build() {
            return new DividerAttributes(
                    mDividerType,
                    mWidthDp,
                    mPrimaryMinRatio,
                    mPrimaryMaxRatio,
                    mDividerColor,
                    mIsDraggingToFullscreenAllowed,
                    mPrimaryVeilColor,
                    mSecondaryVeilColor);
        }
    }
}
