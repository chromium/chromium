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

import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;

import java.util.Objects;

/**
 * A class to represent the background to show while animating embedding activity containers if the
 * animation requires a background.
 *
 * @see SplitAttributes.Builder#setAnimationBackground
 */
public abstract class AnimationBackground {

    /**
     * The special {@link AnimationBackground} object representing the default option. When used,
     * the system will determine the color to use, such as using the current theme window background
     * color.
     */
    @RequiresVendorApiLevel(level = 5)
    public static final @NonNull AnimationBackground ANIMATION_BACKGROUND_DEFAULT =
            new DefaultBackground();

    /**
     * Creates a {@link ColorBackground} that wraps the given color. Only opaque background is
     * supported.
     *
     * @param color the color to be stored.
     * @throws IllegalArgumentException if the color is not opaque.
     */
    @RequiresVendorApiLevel(level = 5)
    public static @NonNull ColorBackground createColorBackground(@ColorInt int color) {
        return new ColorBackground(color);
    }

    private AnimationBackground() {}

    /**
     * @see #ANIMATION_BACKGROUND_DEFAULT
     */
    private static class DefaultBackground extends AnimationBackground {
        @Override
        public String toString() {
            return DefaultBackground.class.getSimpleName();
        }
    }

    /**
     * An {@link AnimationBackground} to specify of using a developer-defined color as the animation
     * background. Only opaque background is supported.
     *
     * @see #createColorBackground(int)
     */
    @RequiresVendorApiLevel(level = 5)
    public static class ColorBackground extends AnimationBackground {

        @ColorInt private final int mColor;

        private ColorBackground(@ColorInt int color) {
            final int alpha = Color.alpha(color);
            if (alpha != 255) {
                throw new IllegalArgumentException(
                        "Color must be fully opaque, current alpha is " + alpha);
            }
            mColor = color;
        }

        /** Returns the color to use as the animation background. */
        @RequiresVendorApiLevel(level = 5)
        @ColorInt
        public int getColor() {
            return mColor;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof ColorBackground)) return false;
            final ColorBackground that = (ColorBackground) o;
            return mColor == that.mColor;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mColor);
        }

        @Override
        public @NonNull String toString() {
            return ColorBackground.class.getSimpleName() + " { color: " + mColor + " }";
        }
    }
}
