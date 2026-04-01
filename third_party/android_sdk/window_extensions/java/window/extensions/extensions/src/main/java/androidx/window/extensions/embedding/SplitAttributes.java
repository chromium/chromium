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

import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.BOTTOM_TO_TOP;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.LEFT_TO_RIGHT;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.LOCALE;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.RIGHT_TO_LEFT;
import static androidx.window.extensions.embedding.SplitAttributes.LayoutDirection.TOP_TO_BOTTOM;
import static androidx.window.extensions.embedding.WindowAttributes.DIM_AREA_ON_TASK;

import android.annotation.SuppressLint;

import androidx.annotation.FloatRange;
import androidx.annotation.IntDef;
import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.core.util.function.Function;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Attributes that describe how the parent window (typically the activity task window) is split
 * between the primary and secondary activity containers, including:
 *
 * <ul>
 *   <li>Split type -- Categorizes the split and specifies the sizes of the primary and secondary
 *       activity containers relative to the parent bounds
 *   <li>Layout direction -- Specifies whether the parent window is split vertically or horizontally
 *       and in which direction the primary and secondary containers are respectively positioned
 *       (left to right, right to left, top to bottom, and so forth)
 *   <li>Animation background -- The background to show during animation of the split involving this
 *       {@code SplitAttributes} object if the animation requires a background
 * </ul>
 *
 * <p>Attributes can be configured by:
 *
 * <ul>
 *   <li>Setting the default {@code SplitAttributes} using {@link
 *       SplitPairRule.Builder#setDefaultSplitAttributes} or {@link
 *       SplitPlaceholderRule.Builder#setDefaultSplitAttributes}.
 *   <li>Using {@link ActivityEmbeddingComponent#setSplitAttributesCalculator(Function)} to set the
 *       callback to customize the {@code SplitAttributes} for a given device and window state.
 * </ul>
 *
 * @see SplitAttributes.SplitType
 * @see SplitAttributes.LayoutDirection
 * @see AnimationParams
 */
@RequiresVendorApiLevel(level = 2)
public class SplitAttributes {

    /**
     * The type of window split, which defines the proportion of the parent window occupied by the
     * primary and secondary activity containers.
     */
    public static class SplitType {
        private final @NonNull String mDescription;

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

        @Override
        public @NonNull String toString() {
            return mDescription;
        }

        @SuppressLint("Range") // The range is covered.
        static @NonNull SplitType createSplitTypeFromLegacySplitRatio(
                @FloatRange(from = 0.0, to = 1.0) float splitRatio) {
            // Treat 0.0 and 1.0 as ExpandContainerSplitType because it means the parent container
            // is filled with secondary or primary container.
            if (splitRatio == 0.0 || splitRatio == 1.0) {
                return new ExpandContainersSplitType();
            }
            return new RatioSplitType(splitRatio);
        }

        /**
         * A window split that's based on the ratio of the size of the primary container to the size
         * of the parent window (excluding area unavailable for the containers such as the divider.
         * See {@link DividerAttributes}).
         *
         * <p>Values in the non-inclusive range (0.0, 1.0) define the size of the primary container
         * relative to the size of the parent window:
         *
         * <ul>
         *   <li>0.5 -- Primary container occupies half of the parent window; secondary container,
         *       the other half
         *   <li>Greater than 0.5 -- Primary container occupies a larger proportion of the parent
         *       window than the secondary container
         *   <li>Less than 0.5 -- Primary container occupies a smaller proportion of the parent
         *       window than the secondary container
         * </ul>
         */
        public static final class RatioSplitType extends SplitType {
            @FloatRange(from = 0.0, to = 1.0, fromInclusive = false, toInclusive = false)
            private final float mRatio;

            /**
             * Creates an instance of this {@code RatioSplitType}.
             *
             * @param ratio The proportion of the parent window occupied by the primary container of
             *     the split (excluding area unavailable for the containers such as the divider. See
             *     {@link DividerAttributes}). Can be a value in the non-inclusive range (0.0, 1.0).
             *     Use {@link SplitType.ExpandContainersSplitType} to create a split type that
             *     occupies the entire parent window.
             */
            public RatioSplitType(
                    @FloatRange(from = 0.0, to = 1.0, fromInclusive = false, toInclusive = false)
                            float ratio) {
                super("ratio:" + ratio);
                if (ratio <= 0.0f || ratio >= 1.0f) {
                    throw new IllegalArgumentException(
                            "Ratio must be in range (0.0, 1.0).  Use"
                                    + " SplitType.ExpandContainersSplitType() instead of 0 or 1.");
                }
                mRatio = ratio;
            }

            /**
             * Gets the proportion of the parent window occupied by the primary activity container
             * of the split (excluding area unavailable for the containers such as the divider. See
             * {@link DividerAttributes}) .
             *
             * @return The proportion of the split occupied by the primary container.
             */
            @FloatRange(from = 0.0, to = 1.0, fromInclusive = false, toInclusive = false)
            public float getRatio() {
                return mRatio;
            }

            /**
             * Creates a split type in which the primary and secondary containers occupy equal
             * portions of the parent window.
             *
             * <p>Serves as the default {@link SplitType} if {@link
             * SplitAttributes.Builder#setSplitType(SplitType)} is not specified.
             *
             * @return A {@code RatioSplitType} in which the activity containers occupy equal
             *     portions of the parent window.
             */
            public static @NonNull RatioSplitType splitEqually() {
                return new RatioSplitType(0.5f);
            }
        }

        /**
         * A parent window split in which the split ratio conforms to the position of a hinge or
         * separating fold in the device display.
         *
         * <p>The split type is created only if:
         *
         * <ul>
         *   <li>The host task is not in multi-window mode (e.g., split-screen mode or
         *       picture-in-picture mode)
         *   <li>The device has a hinge or separating fold reported by
         *       [androidx.window.layout.FoldingFeature.isSeparating]
         *   <li>The hinge or separating fold orientation matches how the parent bounds are split:
         *       <ul>
         *         <li>The hinge or fold orientation is vertical, and the task bounds are also split
         *             vertically (containers are side by side)
         *         <li>The hinge or fold orientation is horizontal, and the task bounds are also
         *             split horizontally (containers are top and bottom)
         *       </ul>
         * </ul>
         *
         * Otherwise, the type falls back to the {@code SplitType} returned by {@link
         * #getFallbackSplitType()}.
         */
        public static final class HingeSplitType extends SplitType {
            private final @NonNull SplitType mFallbackSplitType;

            /**
             * Creates an instance of this {@code HingeSplitType}.
             *
             * @param fallbackSplitType The split type to use if a split based on the device hinge
             *     or separating fold cannot be determined. Can be a {@link RatioSplitType} or
             *     {@link ExpandContainersSplitType}.
             */
            public HingeSplitType(@NonNull SplitType fallbackSplitType) {
                super("hinge, fallbackType=" + fallbackSplitType);
                mFallbackSplitType = fallbackSplitType;
            }

            /**
             * Returns the fallback {@link SplitType} if a split based on the device hinge or
             * separating fold cannot be determined.
             */
            public @NonNull SplitType getFallbackSplitType() {
                return mFallbackSplitType;
            }
        }

        /**
         * A window split in which the primary and secondary activity containers each occupy the
         * entire parent window.
         *
         * <p>The secondary container overlays the primary container.
         */
        public static final class ExpandContainersSplitType extends SplitType {

            /** Creates an instance of this {@code ExpandContainersSplitType}. */
            public ExpandContainersSplitType() {
                super("expandContainers");
            }
        }
    }

    /** The layout direction of the primary and secondary activity containers. */
    public static final class LayoutDirection {

        /**
         * Specifies that the parent bounds are split vertically (side to side).
         *
         * <p>Places the primary container in the left portion of the parent window, and the
         * secondary container in the right portion.
         *
         * <p>A possible return value of {@link SplitType#getLayoutDirection()}.
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
         * Specifies that the parent bounds are split vertically (side to side).
         *
         * <p>Places the primary container in the right portion of the parent window, and the
         * secondary container in the left portion.
         *
         * <p>A possible return value of {@link SplitType#getLayoutDirection()}.
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
         * <p>The direction of the primary and secondary containers is deduced from the locale as
         * either {@link #LEFT_TO_RIGHT} or {@link #RIGHT_TO_LEFT}.
         *
         * <p>A possible return value of {@link SplitType#getLayoutDirection()}.
         */
        // Must match {@link LayoutDirection#LOCALE} for backwards
        // compatibility with prior versions of Extensions.
        public static final int LOCALE = 3;

        /**
         * Specifies that the parent bounds are split horizontally (top and bottom).
         *
         * <p>Places the primary container in the top portion of the parent window, and the
         * secondary container in the bottom portion.
         *
         * <p>If the horizontal layout direction is not supported on the device, layout direction
         * falls back to {@link #LOCALE}.
         *
         * <p>A possible return value of {@link SplitType#getLayoutDirection()}.
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
         * Specifies that the parent bounds are split horizontally (top and bottom).
         *
         * <p>Places the primary container in the bottom portion of the parent window, and the
         * secondary container in the top portion.
         *
         * <p>If the horizontal layout direction is not supported on the device, layout direction
         * falls back to {@link #LOCALE}.
         *
         * <p>A possible return value of {@link SplitType#getLayoutDirection()}.
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

    @ExtLayoutDirection private final int mLayoutDirection;

    private final @NonNull SplitType mSplitType;

    private final @NonNull AnimationParams mAnimationParams;

    private final @NonNull WindowAttributes mWindowAttributes;

    /** The attributes of a divider. If {@code null}, no divider is requested. */
    private final @Nullable DividerAttributes mDividerAttributes;

    /**
     * Creates an instance of this {@code SplitAttributes}.
     *
     * @param splitType The type of split. See {@link SplitAttributes.SplitType}.
     * @param layoutDirection The layout direction of the split, such as left to right or top to
     *     bottom. See {@link SplitAttributes.LayoutDirection}.
     * @param animationParams The {@link AnimationParams} to use for the during animation of the
     *     split involving this {@code SplitAttributes} object.
     * @param attributes The {@link WindowAttributes} of the split, such as dim area behavior.
     * @param dividerAttributes The {@link DividerAttributes}. If {@code null}, no divider is
     *     requested.
     */
    SplitAttributes(
            @NonNull SplitType splitType,
            @ExtLayoutDirection int layoutDirection,
            @NonNull AnimationParams animationParams,
            @NonNull WindowAttributes attributes,
            @Nullable DividerAttributes dividerAttributes) {
        mSplitType = splitType;
        mLayoutDirection = layoutDirection;
        mAnimationParams = animationParams;
        mWindowAttributes = attributes;
        mDividerAttributes = dividerAttributes;
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
    public @NonNull SplitType getSplitType() {
        return mSplitType;
    }

    /**
     * @deprecated Use {@link #getAnimationParams()} starting with vendor API level 7. Only used if
     *     {@link #getAnimationParams()} can't be called on vendor API level 5 and 6.
     */
    @RequiresVendorApiLevel(level = 5, deprecatedSince = 7)
    @Deprecated
    @SuppressWarnings("Deprecation")
    public @NonNull AnimationBackground getAnimationBackground() {
        return mAnimationParams.getAnimationBackground();
    }

    /**
     * Returns the {@link AnimationParams} to use during the animation of the split involving this
     * {@code SplitAttributes} object.
     */
    @RequiresVendorApiLevel(level = 7)
    public @NonNull AnimationParams getAnimationParams() {
        return mAnimationParams;
    }

    /**
     * Returns the {@link WindowAttributes} which contains the configurations of the embedded
     * Activity windows in this SplitAttributes.
     */
    @RequiresVendorApiLevel(level = 5)
    public @NonNull WindowAttributes getWindowAttributes() {
        return mWindowAttributes;
    }

    /** Returns the {@link DividerAttributes}. If {@code null}, no divider is requested. */
    @RequiresVendorApiLevel(level = 6)
    public @Nullable DividerAttributes getDividerAttributes() {
        return mDividerAttributes;
    }

    /**
     * Builder for creating an instance of {@link SplitAttributes}.
     *
     * <p>- The default split type is an equal split between primary and secondary containers. - The
     * default layout direction is based on locale. - The default animation background is to use the
     * current theme window background color.
     */
    public static final class Builder {
        private @NonNull SplitType mSplitType = new SplitType.RatioSplitType(0.5f);
        @ExtLayoutDirection private int mLayoutDirection = LOCALE;

        private @NonNull AnimationParams mAnimationParams = new AnimationParams.Builder().build();

        private @NonNull WindowAttributes mWindowAttributes =
                new WindowAttributes(DIM_AREA_ON_TASK);

        private @Nullable DividerAttributes mDividerAttributes;

        /** Creates a new {@link Builder} to create {@link SplitAttributes}. */
        public Builder() {}

        /**
         * Creates a {@link Builder} with values cloned from the original {@link SplitAttributes}.
         *
         * @param original the original {@link SplitAttributes} to initialize the {@link Builder}.
         */
        @RequiresVendorApiLevel(level = 6)
        public Builder(@NonNull SplitAttributes original) {
            mSplitType = original.mSplitType;
            mLayoutDirection = original.mLayoutDirection;
            mAnimationParams = original.mAnimationParams;
            mWindowAttributes = original.mWindowAttributes;
            mDividerAttributes = original.mDividerAttributes;
        }

        /**
         * Sets the split type attribute.
         *
         * <p>The default is an equal split between primary and secondary containers (see {@link
         * SplitType.RatioSplitType#splitEqually()}).
         *
         * @param splitType The split type attribute.
         * @return This {@code Builder}.
         */
        public @NonNull Builder setSplitType(@NonNull SplitType splitType) {
            mSplitType = splitType;
            return this;
        }

        /**
         * Sets the split layout direction attribute.
         *
         * <p>The default is based on locale.
         *
         * <p>Must be one of:
         *
         * <ul>
         *   <li>{@link LayoutDirection#LOCALE}
         *   <li>{@link LayoutDirection#LEFT_TO_RIGHT}
         *   <li>{@link LayoutDirection#RIGHT_TO_LEFT}
         *   <li>{@link LayoutDirection#TOP_TO_BOTTOM}
         *   <li>{@link LayoutDirection#BOTTOM_TO_TOP}
         * </ul>
         *
         * @param layoutDirection The layout direction attribute.
         * @return This {@code Builder}.
         */
        @SuppressLint("WrongConstant") // To be compat with android.util.LayoutDirection APIs
        public @NonNull Builder setLayoutDirection(@ExtLayoutDirection int layoutDirection) {
            mLayoutDirection = layoutDirection;
            return this;
        }

        /**
         * @deprecated Use {@link #setAnimationParams(AnimationParams)} starting with vendor API
         *     level 7. Only used if {@link #setAnimationParams(AnimationParams)} can't be called on
         *     vendor API level 5 and 6.
         */
        @RequiresVendorApiLevel(level = 5, deprecatedSince = 7)
        @Deprecated
        @SuppressWarnings("Deprecation")
        public @NonNull Builder setAnimationBackground(@NonNull AnimationBackground background) {
            mAnimationParams =
                    new AnimationParams.Builder().setAnimationBackground(background).build();
            return this;
        }

        /**
         * Sets the {@link AnimationParams} to use during the animation of the split involving this
         * {@code SplitAttributes} object.
         *
         * @param params The {@link AnimationParams} to be used for the animation of the split.
         * @return This {@code Builder}.
         */
        @RequiresVendorApiLevel(level = 7)
        public @NonNull Builder setAnimationParams(@NonNull AnimationParams params) {
            mAnimationParams = params;
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
        @RequiresVendorApiLevel(level = 5)
        public @NonNull Builder setWindowAttributes(@NonNull WindowAttributes attributes) {
            mWindowAttributes = attributes;
            return this;
        }

        /** Sets the {@link DividerAttributes}. If {@code null}, no divider is requested. */
        @RequiresVendorApiLevel(level = 6)
        public @NonNull Builder setDividerAttributes(
                @Nullable DividerAttributes dividerAttributes) {
            mDividerAttributes = dividerAttributes;
            return this;
        }

        /**
         * Builds a {@link SplitAttributes} instance with the attributes specified by {@link
         * #setSplitType}, {@link #setLayoutDirection}, and {@link #setAnimationParams}.
         *
         * @return The new {@code SplitAttributes} instance.
         */
        public @NonNull SplitAttributes build() {
            return new SplitAttributes(
                    mSplitType,
                    mLayoutDirection,
                    mAnimationParams,
                    mWindowAttributes,
                    mDividerAttributes);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof SplitAttributes)) return false;
        SplitAttributes that = (SplitAttributes) o;
        return mLayoutDirection == that.mLayoutDirection
                && mSplitType.equals(that.mSplitType)
                && Objects.equals(mAnimationParams, that.mAnimationParams)
                && mWindowAttributes.equals(that.mWindowAttributes)
                && Objects.equals(mDividerAttributes, that.mDividerAttributes);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mLayoutDirection,
                mSplitType,
                mAnimationParams,
                mWindowAttributes,
                mDividerAttributes);
    }

    @Override
    public @NonNull String toString() {
        return SplitAttributes.class.getSimpleName()
                + "{"
                + "layoutDir="
                + layoutDirectionToString()
                + ", splitType="
                + mSplitType
                + ", animationParams="
                + mAnimationParams
                + ", windowAttributes="
                + mWindowAttributes
                + ", dividerAttributes="
                + mDividerAttributes
                + "}";
    }

    private @NonNull String layoutDirectionToString() {
        switch (mLayoutDirection) {
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
