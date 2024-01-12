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

import static androidx.annotation.RestrictTo.Scope.LIBRARY;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.BOTTOM_TO_TOP;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.LEFT_TO_RIGHT;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.LOCALE;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.RIGHT_TO_LEFT;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.TOP_TO_BOTTOM;

import android.annotation.SuppressLint;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.FloatRange;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.window.extensions.core.util.function.Function;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Attributes that describe how the parent window (typically the activity task
 * window) is split between the primary and secondary activity containers,
 * including:
 * <ul>
 *     <li>Split type -- Categorizes the split and specifies the sizes of the
 *         primary and secondary activity containers relative to the parent
 *         bounds</li>
 *     <li>Layout direction -- Specifies whether the parent window is split
 *         vertically or horizontally and in which direction the primary and
 *         secondary containers are respectively positioned (left to right,
 *         right to left, top to bottom, and so forth)</li>
 *     <li>Animation background color -- The color of the background during
 *         animation of the split involving this {@code SplitAttributes} object
 *         if the animation requires a background</li>
 * </ul>
 *
 * <p>Attributes can be configured by:
 * <ul>
 *     <li>Setting the default {@code SplitAttributes} using
 *         {@link SplitPairRule.Builder#setDefaultSplitAttributes} or
 *         {@link SplitPlaceholderRule.Builder#setDefaultSplitAttributes}.</li>
 *     <li>Using {@link ActivityEmbeddingComponent#setSplitAttributesCalculator(Function)} to set
 *         the callback to customize the {@code SplitAttributes} for a given device and window
 *         state.</li>
 * </ul>
 *
 * @see SplitAttributes.SplitType
 * @see SplitAttributes.LayoutDirection
 * Since {@link androidx.window.extensions.WindowExtensions#VENDOR_API_LEVEL_2}
 */
public class SplitAttributes {

    /**
     * The default value for animation background color, which means to use the current theme window
     * background color.
     *
     * Only opaque color is supported, so {@code 0} is used as the default. Any other non-opaque
     * color will be treated as the default.
     *
     * @see Builder#setAnimationBackgroundColor(int)
     */
    @ColorInt
    @RestrictTo(LIBRARY)
    public static final int DEFAULT_ANIMATION_BACKGROUND_COLOR = 0;

    /**
     * The type of window split, which defines the proportion of the parent
     * window occupied by the primary and secondary activity containers.
     */
    public static class SplitType {
        @NonNull
        private final String mDescription;

        SplitType(@NonNull String description) {
            mDescription = description;
        }

        @Override
        public int hashCode() {
            return mDescription.hashCode();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (this == obj) {
                return true;
            }
            if (!(obj instanceof SplitType)) {
                return false;
            }
            final SplitType that = (SplitType) obj;
            return mDescription.equals(that.mDescription);
        }

        @NonNull
        @Override
        public String toString() {
            return mDescription;
        }

        @SuppressLint("Range") // The range is covered.
        @NonNull
        static SplitType createSplitTypeFromLegacySplitRatio(
                @FloatRange(from = 0.0, to = 1.0) float splitRatio) {
            // Treat 0.0 and 1.0 as ExpandContainerSplitType because it means the parent container
            // is filled with secondary or primary container.
            if (splitRatio == 0.0 || splitRatio == 1.0) {
                return new ExpandContainersSplitType();
            }
            return new RatioSplitType(splitRatio);
        }

        /**
         * A window split that's based on the ratio of the size of the primary
         * container to the size of the parent window.
         *
         * <p>Values in the non-inclusive range (0.0, 1.0) define the size of
         * the primary container relative to the size of the parent window:
         * <ul>
         *     <li>0.5 -- Primary container occupies half of the parent
         *         window; secondary container, the other half</li>
         *     <li>Greater than 0.5 -- Primary container occupies a larger
         *         proportion of the parent window than the secondary
         *         container</li>
         *     <li>Less than 0.5 -- Primary container occupies a smaller
         *         proportion of the parent window than the secondary
         *         container</li>
         * </ul>
         */
        public static final class RatioSplitType extends SplitType {
            @FloatRange(from = 0.0, to = 1.0, fromInclusive = false, toInclusive = false)
            private final float mRatio;

            /**
             * Creates an instance of this {@code RatioSplitType}.
             *
             * @param ratio The proportion of the parent window occupied by the
             *     primary container of the split. Can be a value in the
             *     non-inclusive range (0.0, 1.0). Use
             *     {@link SplitType.ExpandContainersSplitType} to create a split
             *     type that occupies the entire parent window.
             */
            public RatioSplitType(
                    @FloatRange(from = 0.0, to = 1.0, fromInclusive = false, toInclusive = false)
                    float ratio) {
                super("ratio:" + ratio);
                if (ratio <= 0.0f || ratio >= 1.0f) {
                    throw new IllegalArgumentException("Ratio must be in range (0.0, 1.0). "
                            + " Use SplitType.ExpandContainersSplitType() instead of 0 or 1.");
                }
                mRatio = ratio;
            }

            /**
             * Gets the proportion of the parent window occupied by the primary
             * activity container of the split.
             *
             * @return The proportion of the split occupied by the primary
             *     container.
             */
            @FloatRange(from = 0.0, to = 1.0, fromInclusive = false, toInclusive = false)
            public float getRatio() {
                return mRatio;
            }

            /**
             * Creates a split type in which the primary and secondary
             * containers occupy equal portions of the parent window.
             *
             * Serves as the default {@link SplitType} if
             * {@link SplitAttributes.Builder#setSplitType(SplitType)} is not
             * specified.
             *
             * @return A {@code RatioSplitType} in which the activity containers
             *     occupy equal portions of the parent window.
             */
            @NonNull
            public static RatioSplitType splitEqually() {
                return new RatioSplitType(0.5f);
            }
        }

        /**
         * A parent window split in which the split ratio conforms to the
         * position of a hinge or separating fold in the device display.
         *
         * The split type is created only if:
         * <ul>
         *     <li>The host task is not in multi-window mode (e.g.,
         *         split-screen mode or picture-in-picture mode)</li>
         *     <li>The device has a hinge or separating fold reported by
         *         [androidx.window.layout.FoldingFeature.isSeparating]</li>
         *     <li>The hinge or separating fold orientation matches how the
         *         parent bounds are split:
         *         <ul>
         *             <li>The hinge or fold orientation is vertical, and
         *                 the task bounds are also split vertically
         *                 (containers are side by side)</li>
         *             <li>The hinge or fold orientation is horizontal, and
         *                 the task bounds are also split horizontally
         *                 (containers are top and bottom)</li>
         *         </ul>
         *     </li>
         * </ul>
         *
         * Otherwise, the type falls back to the {@code SplitType} returned by
         * {@link #getFallbackSplitType()}.
         */
        public static final class HingeSplitType extends SplitType {
            @NonNull
            private final SplitType mFallbackSplitType;

            /**
             * Creates an instance of this {@code HingeSplitType}.
             *
             * @param fallbackSplitType The split type to use if a split based
             *     on the device hinge or separating fold cannot be determined.
             *     Can be a {@link RatioSplitType} or
             *     {@link ExpandContainersSplitType}.
             */
            public HingeSplitType(@NonNull SplitType fallbackSplitType) {
                super("hinge, fallbackType=" + fallbackSplitType);
                mFallbackSplitType = fallbackSplitType;
            }

            /**
             * Returns the fallback {@link SplitType} if a split based on the
             * device hinge or separating fold cannot be determined.
             */
            @NonNull
            public SplitType getFallbackSplitType() {
                return mFallbackSplitType;
            }
        }

        /**
         * A window split in which the primary and secondary activity containers
         * each occupy the entire parent window.
         *
         * The secondary container overlays the primary container.
         */
        public static final class ExpandContainersSplitType extends SplitType {

            /**
             * Creates an instance of this {@code ExpandContainersSplitType}.
             */
            public ExpandContainersSplitType() {
                super("expandContainers");
            }
        }
    }

    /**
     * The layout direction of the primary and secondary activity containers.
     */
    public static final class LayoutDirection {

        /**
         * Specifies that the parent bounds are split vertically (side to side).
         *
         * Places the primary container in the left portion of the parent
         * window, and the secondary container in the right portion.
         *
         * A possible return value of {@link SplitType#getLayoutDirection()}.
         */
         //
         // -------------------------
         // |           |           |
         // |  Primary  | Secondary |
         // |           |           |
         // -------------------------
         //
         // Must match {@link LayoutDirection#LTR} for backwards compatibility
         // with prior versions of Extensions.
        public static final int LEFT_TO_RIGHT = 0;

        /**
         * Specifies that the parent bounds are split vertically (side to
         * side).
         *
         * Places the primary container in the right portion of the parent
         * window, and the secondary container in the left portion.
         *
         * A possible return value of {@link SplitType#getLayoutDirection()}.
         */
         // -------------------------
         // |           |           |
         // | Secondary |  Primary  |
         // |           |           |
         // -------------------------
         //
         // Must match {@link LayoutDirection#RTL} for backwards compatibility
         // with prior versions of Extensions.
        public static final int RIGHT_TO_LEFT = 1;

        /**
         * Specifies that the parent bounds are split vertically (side to side).
         *
         * The direction of the primary and secondary containers is deduced from
         * the locale as either {@link #LEFT_TO_RIGHT} or
         * {@link #RIGHT_TO_LEFT}.
         *
         * A possible return value of {@link SplitType#getLayoutDirection()}.
         */
         // Must match {@link LayoutDirection#LOCALE} for backwards
         // compatibility with prior versions of Extensions.
        public static final int LOCALE = 3;

        /**
         * Specifies that the parent bounds are split horizontally (top and
         * bottom).
         *
         * Places the primary container in the top portion of the parent window,
         * and the secondary container in the bottom portion.
         *
         * If the horizontal layout direction is not supported on the device,
         * layout direction falls back to {@link #LOCALE}.
         *
         * A possible return value of {@link SplitType#getLayoutDirection()}.
         */
         // -------------
         // |           |
         // |  Primary  |
         // |           |
         // -------------
         // |           |
         // | Secondary |
         // |           |
         // -------------
        public static final int TOP_TO_BOTTOM = 4;

        /**
         * Specifies that the parent bounds are split horizontally (top and
         * bottom).
         *
         * Places the primary container in the bottom portion of the parent
         * window, and the secondary container in the top portion.
         *
         * If the horizontal layout direction is not supported on the device,
         * layout direction falls back to {@link #LOCALE}.
         *
         * A possible return value of {@link SplitType#getLayoutDirection()}.
         */
         // -------------
         // |           |
         // | Secondary |
         // |           |
         // -------------
         // |           |
         // |  Primary  |
         // |           |
         // -------------
        public static final int BOTTOM_TO_TOP = 5;

        private LayoutDirection() {}
    }

    @IntDef({LEFT_TO_RIGHT, RIGHT_TO_LEFT, LOCALE, TOP_TO_BOTTOM, BOTTOM_TO_TOP})
    @Retention(RetentionPolicy.SOURCE)
    @interface ExtLayoutDirection {}

    @ExtLayoutDirection
    private final int mLayoutDirection;

    private final SplitType mSplitType;

    @ColorInt
    private final int mAnimationBackgroundColor;

    /**
     * Creates an instance of this {@code SplitAttributes}.
     *
     * @param splitType The type of split. See
     *     {@link SplitAttributes.SplitType}.
     * @param layoutDirection The layout direction of the split, such as left to
     *     right or top to bottom. See {@link SplitAttributes.LayoutDirection}.
     * @param animationBackgroundColor The {@link ColorInt} to use for the
     *     background color during animation of the split involving this
     *     {@code SplitAttributes} object if the animation requires a
     *     background.
     */
    SplitAttributes(@NonNull SplitType splitType, @ExtLayoutDirection int layoutDirection,
            @ColorInt int animationBackgroundColor) {
        mSplitType = splitType;
        mLayoutDirection = layoutDirection;
        mAnimationBackgroundColor = animationBackgroundColor;
    }

    /**
     * Gets the layout direction of the split.
     *
     * @return The layout direction of the split.
     */
    @ExtLayoutDirection
    public int getLayoutDirection() {
        return mLayoutDirection;
    }

    /**
     * Gets the split type.
     *
     * @return The split type.
     */
    @NonNull
    public SplitType getSplitType() {
        return mSplitType;
    }

    /**
     * Gets the {@link ColorInt} to use for the background color during the
     * animation of the split involving this {@code SplitAttributes} object.
     *
     * The default is {@link #DEFAULT_ANIMATION_BACKGROUND_COLOR}, which means
     * to use the current theme window background color.
     *
     * @return The animation background {@code ColorInt}.
     */
    @ColorInt
    @RestrictTo(LIBRARY)
    public int getAnimationBackgroundColor() {
        return mAnimationBackgroundColor;
    }

    /**
     * Builder for creating an instance of {@link SplitAttributes}.
     *
     * - The default split type is an equal split between primary and secondary containers.
     * - The default layout direction is based on locale.
     * - The default animation background color is to use the current theme window background color.
     */
    public static final class Builder {
        @NonNull
        private SplitType mSplitType =  new SplitType.RatioSplitType(0.5f);
        @ExtLayoutDirection
        private int mLayoutDirection = LOCALE;

        @ColorInt
        private int mAnimationBackgroundColor = 0;

        /**
         * Sets the split type attribute.
         *
         * The default is an equal split between primary and secondary
         * containers (see {@link SplitType.RatioSplitType#splitEqually()}).
         *
         * @param splitType The split type attribute.
         * @return This {@code Builder}.
         */
        @NonNull
        public Builder setSplitType(@NonNull SplitType splitType) {
            mSplitType = splitType;
            return this;
        }

        /**
         * Sets the split layout direction attribute.
         *
         * The default is based on locale.
         *
         * Must be one of:
         * <ul>
         *     <li>{@link LayoutDirection#LOCALE}</li>
         *     <li>{@link LayoutDirection#LEFT_TO_RIGHT}</li>
         *     <li>{@link LayoutDirection#RIGHT_TO_LEFT}</li>
         *     <li>{@link LayoutDirection#TOP_TO_BOTTOM}</li>
         *     <li>{@link LayoutDirection#BOTTOM_TO_TOP}</li>
         * </ul>
         *
         * @param layoutDirection The layout direction attribute.
         * @return This {@code Builder}.
         */
        @SuppressLint("WrongConstant") // To be compat with android.util.LayoutDirection APIs
        @NonNull
        public Builder setLayoutDirection(@ExtLayoutDirection int layoutDirection) {
            mLayoutDirection = layoutDirection;
            return this;
        }

        /**
         * Sets the {@link ColorInt} to use for the background during the
         * animation of the split involving this {@code SplitAttributes} object
         * if the animation requires a background.
         *
         * Only opaque color is supported.
         *
         * The default value is {@link #DEFAULT_ANIMATION_BACKGROUND_COLOR}, which
         * means to use the current theme window background color. Any non-opaque
         * animation color will be treated as
         * {@link #DEFAULT_ANIMATION_BACKGROUND_COLOR}.
         *
         * @param color A packed color int of the form {@code AARRGGBB} for the
         *              animation background color.
         * @return This {@code Builder}.
         */
        @NonNull
        @RestrictTo(LIBRARY)
        public Builder setAnimationBackgroundColor(@ColorInt int color) {
            // Any non-opaque color will be treated as the default.
            mAnimationBackgroundColor = Color.alpha(color) != 255
                    ? DEFAULT_ANIMATION_BACKGROUND_COLOR
                    : color;
            return this;
        }

        /**
         * Builds a {@link SplitAttributes} instance with the attributes
         * specified by {@link #setSplitType}, {@link #setLayoutDirection}, and
         * {@link #setAnimationBackgroundColor}.
         *
         * @return The new {@code SplitAttributes} instance.
         */
        @NonNull
        public SplitAttributes build() {
            return new SplitAttributes(mSplitType, mLayoutDirection, mAnimationBackgroundColor);
        }
    }

    @Override
    public int hashCode() {
        int result = mSplitType.hashCode();
        result = result * 31 + mLayoutDirection;
        result = result * 31 + mAnimationBackgroundColor;
        return result;
    }

    @Override
    public boolean equals(Object other) {
        if (other == this) {
            return true;
        }
        if (!(other instanceof SplitAttributes)) {
            return false;
        }
        final SplitAttributes otherAttributes = (SplitAttributes) other;
        return mLayoutDirection == otherAttributes.mLayoutDirection
                && mSplitType.equals(otherAttributes.mSplitType)
                && mAnimationBackgroundColor == otherAttributes.mAnimationBackgroundColor;
    }

    @NonNull
    @Override
    public String toString() {
        return SplitAttributes.class.getSimpleName() + "{"
                + "layoutDir=" + layoutDirectionToString()
                + ", ratio=" + mSplitType
                + ", animationBgColor=" + Integer.toHexString(mAnimationBackgroundColor)
                + "}";
    }

    @NonNull
    private String layoutDirectionToString() {
        switch(mLayoutDirection) {
            case LEFT_TO_RIGHT:
                return "LEFT_TO_RIGHT";
            case RIGHT_TO_LEFT:
                return "RIGHT_TO_LEFT";
            case LOCALE:
                return "LOCALE";
            case TOP_TO_BOTTOM:
                return "TOP_TO_BOTTOM";
            case BOTTOM_TO_TOP:
                return "BOTTOM_TO_TOP";
            default:
                throw new IllegalArgumentException("Invalid layout direction:" + mLayoutDirection);
        }
    }
}
