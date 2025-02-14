/*
 * Copyright (C) 2015 The Android Open Source Project
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
package androidx.core.content.res;

import static androidx.annotation.RestrictTo.Scope.LIBRARY_GROUP_PREFIX;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.TypedValue;

import androidx.annotation.AnyRes;
import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.RestrictTo;
import androidx.annotation.StyleableRes;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;
import org.xmlpull.v1.XmlPullParser;

/**
 * Compat methods for accessing TypedArray values.
 *
 * All the getNamed*() functions added the attribute name match, to take care of potential ID
 * collision between the private attributes in older OS version (OEM) and the attributes existed in
 * the newer OS version.
 * For example, if an private attribute named "abcdefg" in Kitkat has the
 * same id value as "android:pathData" in Lollipop, we need to match the attribute's name first.
 *
 */
@RestrictTo(LIBRARY_GROUP_PREFIX)
public class TypedArrayUtils {

    private static final String NAMESPACE = "http://schemas.android.com/apk/res/android";

    /**
     * @return Whether the current node of the {@link XmlPullParser} has an attribute with the
     * specified {@code attrName}.
     */
    public static boolean hasAttribute(@NonNull XmlPullParser parser, @NonNull String attrName) {
        return parser.getAttributeValue(NAMESPACE, attrName) != null;
    }

    /**
     * Retrieves a float attribute value. In addition to the styleable resource ID, we also make
     * sure that the attribute name matches.
     *
     * @return a float value in the {@link TypedArray} with the specified {@code resId}, or
     * {@code defaultValue} if it does not exist.
     */
    public static float getNamedFloat(@NonNull TypedArray a, @NonNull XmlPullParser parser,
            @NonNull String attrName, @StyleableRes int resId, float defaultValue) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return defaultValue;
        } else {
            return a.getFloat(resId, defaultValue);
        }
    }

    /**
     * Retrieves a boolean attribute value. In addition to the styleable resource ID, we also make
     * sure that the attribute name matches.
     *
     * @return a boolean value in the {@link TypedArray} with the specified {@code resId}, or
     * {@code defaultValue} if it does not exist.
     */
    public static boolean getNamedBoolean(@NonNull TypedArray a, @NonNull XmlPullParser parser,
            @NonNull String attrName, @StyleableRes int resId, boolean defaultValue) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return defaultValue;
        } else {
            return a.getBoolean(resId, defaultValue);
        }
    }

    /**
     * Retrieves an int attribute value. In addition to the styleable resource ID, we also make
     * sure that the attribute name matches.
     *
     * @return an int value in the {@link TypedArray} with the specified {@code resId}, or
     * {@code defaultValue} if it does not exist.
     */
    public static int getNamedInt(@NonNull TypedArray a, @NonNull XmlPullParser parser,
            @NonNull String attrName, @StyleableRes int resId, int defaultValue) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return defaultValue;
        } else {
            return a.getInt(resId, defaultValue);
        }
    }

    /**
     * Retrieves a color attribute value. In addition to the styleable resource ID, we also make
     * sure that the attribute name matches.
     *
     * @return a color value in the {@link TypedArray} with the specified {@code resId}, or
     * {@code defaultValue} if it does not exist.
     */
    @ColorInt
    public static int getNamedColor(@NonNull TypedArray a, @NonNull XmlPullParser parser,
            @NonNull String attrName, @StyleableRes int resId, @ColorInt int defaultValue) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return defaultValue;
        } else {
            return a.getColor(resId, defaultValue);
        }
    }

    /**
     * Retrieves a complex color attribute value. In addition to the styleable resource ID, we also
     * make sure that the attribute name matches.
     *
     * @return a complex color value from the {@link TypedArray} with the specified {@code resId},
     * or containing {@code defaultValue} if it does not exist.
     */
    public static ComplexColorCompat getNamedComplexColor(@NonNull TypedArray a,
            @NonNull XmlPullParser parser, Resources.@Nullable Theme theme,
            @NonNull String attrName, @StyleableRes int resId, @ColorInt int defaultValue) {
        if (hasAttribute(parser, attrName)) {
            return getComplexColor(a, theme, resId, defaultValue);
        }
        return ComplexColorCompat.from(defaultValue);
    }

    /**
     * Retrieves a complex color attribute value. First tries to parse it as a simple color.
     * <p>
     * This is the same as
     * {@link #getNamedComplexColor(TypedArray, XmlPullParser, Resources.Theme, String, int, int)},
     * but does not check the attribute name.
     *
     * @return a complex color value from the {@link TypedArray} with the specified {@code resId},
     * or containing {@code defaultValue} if it does not exist.
     */
    public static @NonNull ComplexColorCompat getComplexColor(@NonNull TypedArray a,
            Resources.@Nullable Theme theme, @StyleableRes int resId, @ColorInt int defaultValue) {
        // first check if is a simple color
        final TypedValue value = new TypedValue();
        a.getValue(resId, value);
        if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT
                && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
            return ComplexColorCompat.from(value.data);
        }

        @ColorRes int colorRes = a.getResourceId(resId, 0);
        if (colorRes != Resources.ID_NULL) {
            // not a simple color, attempt to inflate complex types
            final ComplexColorCompat complexColor =
                    ComplexColorCompat.inflate(a.getResources(), colorRes, theme);
            if (complexColor != null) {
                return complexColor;
            }
        }

        return ComplexColorCompat.from(defaultValue);
    }

    /**
     * Retrieves a color state list object. In addition to the styleable resource ID, we also
     * make sure that the attribute name matches.
     *
     * @return a color state list object form the {@link TypedArray} with the specified
     * {@code resId}, or null if it does not exist.
     */
    public static @Nullable ColorStateList getNamedColorStateList(@NonNull TypedArray a,
            @NonNull XmlPullParser parser, Resources.@Nullable Theme theme,
            @NonNull String attrName, @StyleableRes int resId) {
        if (hasAttribute(parser, attrName)) {
            return getColorStateList(a, theme, resId);
        }
        return null;
    }

    /**
     * Retrieves a color state list object.
     * <p>
     * This is the same as
     * {@link #getNamedColorStateList(TypedArray, XmlPullParser, Resources.Theme, String, int)},
     * but does not check the attribute name.
     *
     * @return a color state list object form the {@link TypedArray} with the specified
     * {@code resId}, or null if it does not exist.
     */
    public static @Nullable ColorStateList getColorStateList(@NonNull TypedArray a,
            Resources.@Nullable Theme theme, int resId) {
        final TypedValue value = new TypedValue();
        a.getValue(resId, value);
        if (value.type == TypedValue.TYPE_ATTRIBUTE) {
            throw new UnsupportedOperationException(
                    "Failed to resolve attribute at index " + resId + ": " + value);
        } else if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT
                && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
            // Handle inline color definitions.
            return getNamedColorStateListFromInt(value);
        }
        return ColorStateListInflaterCompat.inflate(a.getResources(),
                a.getResourceId(resId, 0), theme);
    }

    private static @NonNull ColorStateList getNamedColorStateListFromInt(
            @NonNull TypedValue value) {
        // This is copied from ResourcesImpl#getNamedColorStateListFromInt in the platform, but the
        // ComplexColor caching mechanism has been removed. The practical implication of this is
        // minimal, since platform caching is only used by Zygote-preloaded resources.
        return ColorStateList.valueOf(value.data);
    }

    /**
     * Retrieves a resource ID attribute value. In addition to the styleable resource ID, we also
     * make sure that the attribute name matches.
     *
     * @return a resource ID value in the {@link TypedArray} with the specified {@code resId}, or
     * {@code defaultValue} if it does not exist.
     */
    @AnyRes
    public static int getNamedResourceId(@NonNull TypedArray a, @NonNull XmlPullParser parser,
            @NonNull String attrName, @StyleableRes int resId, @AnyRes int defaultValue) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return defaultValue;
        } else {
            return a.getResourceId(resId, defaultValue);
        }
    }

    /**
     * Retrieves a string attribute value. In addition to the styleable resource ID, we also
     * make sure that the attribute name matches.
     *
     * @return a string value in the {@link TypedArray} with the specified {@code resId}, or
     * null if it does not exist.
     */
    public static @Nullable String getNamedString(@NonNull TypedArray a,
            @NonNull XmlPullParser parser, @NonNull String attrName, @StyleableRes int resId) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return null;
        } else {
            return a.getString(resId);
        }
    }

    /**
     * Retrieve the raw TypedValue for the attribute at <var>index</var>
     * and return a temporary object holding its data.  This object is only
     * valid until the next call on to {@link TypedArray}.
     */
    public static @Nullable TypedValue peekNamedValue(@NonNull TypedArray a,
            @NonNull XmlPullParser parser, @NonNull String attrName, int resId) {
        final boolean hasAttr = hasAttribute(parser, attrName);
        if (!hasAttr) {
            return null;
        } else {
            return a.peekValue(resId);
        }
    }

    /**
     * Obtains styled attributes from the theme, if available, or unstyled
     * resources if the theme is null.
     */
    public static @NonNull TypedArray obtainAttributes(@NonNull Resources res,
            Resources.@Nullable Theme theme, @NonNull AttributeSet set, int @NonNull [] attrs) {
        if (theme == null) {
            return res.obtainAttributes(set, attrs);
        }
        return theme.obtainStyledAttributes(set, attrs, 0, 0);
    }

    /**
     * @return a boolean value of {@code index}. If it does not exist, a boolean value of
     * {@code fallbackIndex}. If it still does not exist, {@code defaultValue}.
     */
    public static boolean getBoolean(@NonNull TypedArray a, @StyleableRes int index,
            @StyleableRes int fallbackIndex, boolean defaultValue) {
        boolean val = a.getBoolean(fallbackIndex, defaultValue);
        return a.getBoolean(index, val);
    }

    /**
     * @return a drawable value of {@code index}. If it does not exist, a drawable value of
     * {@code fallbackIndex}. If it still does not exist, {@code null}.
     */
    public static @Nullable Drawable getDrawable(@NonNull TypedArray a, @StyleableRes int index,
            @StyleableRes int fallbackIndex) {
        Drawable val = a.getDrawable(index);
        if (val == null) {
            val = a.getDrawable(fallbackIndex);
        }
        return val;
    }

    /**
     * @return an int value of {@code index}. If it does not exist, an int value of
     * {@code fallbackIndex}. If it still does not exist, {@code defaultValue}.
     */
    public static int getInt(@NonNull TypedArray a, @StyleableRes int index,
            @StyleableRes int fallbackIndex, int defaultValue) {
        int val = a.getInt(fallbackIndex, defaultValue);
        return a.getInt(index, val);
    }

    /**
     * @return a resource ID value of {@code index}. If it does not exist, a resource ID value of
     * {@code fallbackIndex}. If it still does not exist, {@code defaultValue}.
     */
    @AnyRes
    public static int getResourceId(@NonNull TypedArray a, @StyleableRes int index,
            @StyleableRes int fallbackIndex, @AnyRes int defaultValue) {
        int val = a.getResourceId(fallbackIndex, defaultValue);
        return a.getResourceId(index, val);
    }

    /**
     * @return a string value of {@code index}. If it does not exist, a string value of
     * {@code fallbackIndex}. If it still does not exist, {@code null}.
     */
    public static @Nullable String getString(@NonNull TypedArray a, @StyleableRes int index,
            @StyleableRes int fallbackIndex) {
        String val = a.getString(index);
        if (val == null) {
            val = a.getString(fallbackIndex);
        }
        return val;
    }

    /**
     * Retrieves a text attribute value with the specified fallback ID.
     *
     * @return a text value of {@code index}. If it does not exist, a text value of
     * {@code fallbackIndex}. If it still does not exist, {@code null}.
     */
    public static @Nullable CharSequence getText(@NonNull TypedArray a, @StyleableRes int index,
            @StyleableRes int fallbackIndex) {
        CharSequence val = a.getText(index);
        if (val == null) {
            val = a.getText(fallbackIndex);
        }
        return val;
    }

    /**
     * Retrieves a string array attribute value with the specified fallback ID.
     *
     * @return a string array value of {@code index}. If it does not exist, a string array value
     * of {@code fallbackIndex}. If it still does not exist, {@code null}.
     */
    public static CharSequence @Nullable [] getTextArray(@NonNull TypedArray a,
            @StyleableRes int index, @StyleableRes int fallbackIndex) {
        CharSequence[] val = a.getTextArray(index);
        if (val == null) {
            val = a.getTextArray(fallbackIndex);
        }
        return val;
    }

    /**
     * @return The resource ID value in the {@code context} specified by {@code attr}. If it does
     * not exist, {@code fallbackAttr}.
     */
    public static int getAttr(@NonNull Context context, int attr, int fallbackAttr) {
        TypedValue value = new TypedValue();
        context.getTheme().resolveAttribute(attr, value, true);
        if (value.resourceId != 0) {
            return attr;
        }
        return fallbackAttr;
    }

    private TypedArrayUtils() {
    }
}
