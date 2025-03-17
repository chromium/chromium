/*
 * Copyright 2019 The Android Open Source Project
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

package androidx.compose.ui.graphics.vector.compat

import android.content.res.ColorStateList
import android.content.res.Resources
import android.content.res.TypedArray
import android.util.AttributeSet
import android.util.TypedValue
import androidx.annotation.ColorInt
import androidx.annotation.StyleableRes
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathFillType
import androidx.compose.ui.graphics.ShaderBrush
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.vector.DefaultPivotX
import androidx.compose.ui.graphics.vector.DefaultPivotY
import androidx.compose.ui.graphics.vector.DefaultRotation
import androidx.compose.ui.graphics.vector.DefaultScaleX
import androidx.compose.ui.graphics.vector.DefaultScaleY
import androidx.compose.ui.graphics.vector.DefaultTranslationX
import androidx.compose.ui.graphics.vector.DefaultTranslationY
import androidx.compose.ui.graphics.vector.EmptyPath
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.PathNode
import androidx.compose.ui.graphics.vector.PathParser
import androidx.compose.ui.unit.dp
import androidx.core.content.res.ComplexColorCompat
import androidx.core.content.res.TypedArrayUtils
import org.xmlpull.v1.XmlPullParser
import org.xmlpull.v1.XmlPullParserException

private const val LINECAP_BUTT = 0
private const val LINECAP_ROUND = 1
private const val LINECAP_SQUARE = 2

private const val LINEJOIN_MITER = 0
private const val LINEJOIN_ROUND = 1
private const val LINEJOIN_BEVEL = 2

private val FILL_TYPE_WINDING = 0

private const val SHAPE_CLIP_PATH = "clip-path"
private const val SHAPE_GROUP = "group"
private const val SHAPE_PATH = "path"

private fun getStrokeLineCap(id: Int, defValue: StrokeCap = StrokeCap.Butt): StrokeCap =
    when (id) {
        LINECAP_BUTT -> StrokeCap.Butt
        LINECAP_ROUND -> StrokeCap.Round
        LINECAP_SQUARE -> StrokeCap.Square
        else -> defValue
    }

private fun getStrokeLineJoin(id: Int, defValue: StrokeJoin = StrokeJoin.Miter): StrokeJoin =
    when (id) {
        LINEJOIN_MITER -> StrokeJoin.Miter
        LINEJOIN_ROUND -> StrokeJoin.Round
        LINEJOIN_BEVEL -> StrokeJoin.Bevel
        else -> defValue
    }

internal fun XmlPullParser.isAtEnd(): Boolean =
    eventType == XmlPullParser.END_DOCUMENT || (depth < 1 && eventType == XmlPullParser.END_TAG)

/**
 * @param nestedGroups The number of additionally nested VectorGroups to represent clip paths.
 * @return The number of nested VectorGroups that are not `<group>` in XML, but represented as
 *   VectorGroup in the [builder]. These are also popped when this function sees `</group>`.
 */
internal fun AndroidVectorParser.parseCurrentVectorNode(
    res: Resources,
    attrs: AttributeSet,
    theme: Resources.Theme? = null,
    builder: ImageVector.Builder,
    nestedGroups: Int
): Int {
    when (xmlParser.eventType) {
        XmlPullParser.START_TAG -> {
            when (xmlParser.name) {
                SHAPE_PATH -> {
                    parsePath(res, theme, attrs, builder)
                }
                SHAPE_CLIP_PATH -> {
                    parseClipPath(res, theme, attrs, builder)
                    return nestedGroups + 1
                }
                SHAPE_GROUP -> {
                    parseGroup(res, theme, attrs, builder)
                }
            }
        }
        XmlPullParser.END_TAG -> {
            if (SHAPE_GROUP == xmlParser.name) {
                repeat(nestedGroups + 1) { builder.clearGroup() }
                return 0
            }
        }
    }
    return nestedGroups
}

/** Helper method to seek to the first tag within the VectorDrawable xml asset */
@Throws(XmlPullParserException::class)
internal fun XmlPullParser.seekToStartTag(): XmlPullParser {
    var type = next()
    while (type != XmlPullParser.START_TAG && type != XmlPullParser.END_DOCUMENT) {
        // Empty loop
        type = next()
    }
    if (type != XmlPullParser.START_TAG) {
        throw XmlPullParserException("No start tag found")
    }
    return this
}

internal fun AndroidVectorParser.createVectorImageBuilder(
    res: Resources,
    theme: Resources.Theme?,
    attrs: AttributeSet
): ImageVector.Builder {
    val vectorAttrs =
        obtainAttributes(
            res,
            theme,
            attrs,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_TYPE_ARRAY
        )

    val autoMirror =
        getBoolean(
            vectorAttrs,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_AUTO_MIRRORED,
            false
        )

    val viewportWidth =
        getFloat(vectorAttrs, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_VIEWPORT_WIDTH, 0.0f)

    val viewportHeight =
        getFloat(
            vectorAttrs,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_VIEWPORT_HEIGHT,
            0.0f
        )

    if (viewportWidth <= 0) {
        throw XmlPullParserException(
            vectorAttrs.positionDescription + "<VectorGraphic> tag requires viewportWidth > 0"
        )
    } else if (viewportHeight <= 0) {
        throw XmlPullParserException(
            vectorAttrs.positionDescription + "<VectorGraphic> tag requires viewportHeight > 0"
        )
    }

    val defaultWidth =
        getDimension(vectorAttrs, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_WIDTH, 0.0f)
    val defaultHeight =
        getDimension(vectorAttrs, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_HEIGHT, 0.0f)

    val tintColor =
        if (vectorAttrs.hasValue(AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_TINT)) {
            val value = TypedValue()
            vectorAttrs.getValue(AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_TINT, value)
            // Unable to parse theme attributes outside of the framework here.
            // This is a similar limitation to VectorDrawableCompat's parsing logic within
            // updateStateFromTypedArray as TypedArray#extractThemeAttrs is not a public API
            // ignore tint colors provided from the theme itself.
            if (value.type == TypedValue.TYPE_ATTRIBUTE) {
                Color.Unspecified
            } else {
                val tintColorStateList =
                    getColorStateList(
                        vectorAttrs,
                        theme,
                        AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_TINT
                    )
                if (tintColorStateList != null) {
                    Color(tintColorStateList.defaultColor)
                } else {
                    Color.Unspecified
                }
            }
        } else {
            Color.Unspecified
        }

    val blendModeValue =
        getInt(vectorAttrs, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_TINT_MODE, -1)
    val tintBlendMode =
        if (blendModeValue != -1) {
            when (blendModeValue) {
                3 -> BlendMode.SrcOver
                5 -> BlendMode.SrcIn
                9 -> BlendMode.SrcAtop
                // b/73224934 PorterDuff Multiply maps to Skia Modulate so actually
                // return BlendMode.MODULATE here
                14 -> BlendMode.Modulate
                15 -> BlendMode.Screen
                16 -> BlendMode.Plus
                else -> BlendMode.SrcIn
            }
        } else {
            BlendMode.SrcIn
        }

    val defaultWidthDp = (defaultWidth / res.displayMetrics.density).dp
    val defaultHeightDp = (defaultHeight / res.displayMetrics.density).dp

    vectorAttrs.recycle()

    return ImageVector.Builder(
        defaultWidth = defaultWidthDp,
        defaultHeight = defaultHeightDp,
        viewportWidth = viewportWidth,
        viewportHeight = viewportHeight,
        tintColor = tintColor,
        tintBlendMode = tintBlendMode,
        autoMirror = autoMirror
    )
}

@Throws(IllegalArgumentException::class)
internal fun AndroidVectorParser.parsePath(
    res: Resources,
    theme: Resources.Theme?,
    attrs: AttributeSet,
    builder: ImageVector.Builder
) {
    val a =
        obtainAttributes(res, theme, attrs, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH)

    val name: String =
        getString(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_NAME) ?: ""

    val pathStr = getString(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_PATH_DATA)
    val pathData: List<PathNode> =
        if (pathStr == null) {
            EmptyPath
        } else {
            pathParser.pathStringToNodes(pathStr)
        }

    val fillColor =
        getComplexColor(
            a,
            theme,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_FILL_COLOR,
            0
        )
    val fillAlpha =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_FILL_ALPHA, 1.0f)
    val lineCap =
        getInt(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_LINE_CAP, -1)
    val strokeLineCap = getStrokeLineCap(lineCap, StrokeCap.Butt)
    val lineJoin =
        getInt(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_LINE_JOIN, -1)
    val strokeLineJoin = getStrokeLineJoin(lineJoin, StrokeJoin.Bevel)
    val strokeMiterLimit =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_MITER_LIMIT, 1.0f)
    val strokeColor =
        getComplexColor(
            a,
            theme,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_COLOR,
            0
        )
    val strokeAlpha =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_ALPHA, 1.0f)
    val strokeLineWidth =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_WIDTH, 1.0f)

    val trimPathEnd =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_END, 1.0f)
    val trimPathOffset =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_OFFSET, 0.0f)
    val trimPathStart =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_START, 0.0f)

    val fillRule =
        getInt(
            a,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_FILLTYPE,
            FILL_TYPE_WINDING
        )

    a.recycle()

    val fillBrush = obtainBrushFromComplexColor(fillColor)
    val strokeBrush = obtainBrushFromComplexColor(strokeColor)
    val fillPathType = if (fillRule == 0) PathFillType.NonZero else PathFillType.EvenOdd

    builder.addPath(
        pathData,
        fillPathType,
        name,
        fillBrush,
        fillAlpha,
        strokeBrush,
        strokeAlpha,
        strokeLineWidth,
        strokeLineCap,
        strokeLineJoin,
        strokeMiterLimit,
        trimPathStart,
        trimPathEnd,
        trimPathOffset
    )
}

private fun obtainBrushFromComplexColor(complexColor: ComplexColorCompat): Brush? =
    if (complexColor.willDraw()) {
        val shader = complexColor.shader
        if (shader != null) {
            ShaderBrush(shader)
        } else {
            SolidColor(Color(complexColor.color))
        }
    } else {
        null
    }

internal fun AndroidVectorParser.parseClipPath(
    res: Resources,
    theme: Resources.Theme?,
    attrs: AttributeSet,
    builder: ImageVector.Builder
) {
    val a =
        obtainAttributes(
            res,
            theme,
            attrs,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_CLIP_PATH,
        )

    val name: String =
        getString(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_CLIP_PATH_NAME) ?: ""
    val pathStr = getString(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_CLIP_PATH_PATH_DATA)
    val pathData = if (pathStr == null) EmptyPath else pathParser.pathStringToNodes(pathStr)
    a.recycle()

    // <clip-path> is parsed out as an additional VectorGroup.
    // This allows us to replicate the behavior of VectorDrawable where <clip-path> only affects
    // <path> that comes after it in <group>.
    builder.addGroup(name = name, clipPathData = pathData)
}

internal fun AndroidVectorParser.parseGroup(
    res: Resources,
    theme: Resources.Theme?,
    attrs: AttributeSet,
    builder: ImageVector.Builder
) {
    val a =
        obtainAttributes(res, theme, attrs, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP)

    // Account for any configuration changes.
    // mChangingConfigurations |= Utils.getChangingConfigurations(a);

    // Extract the theme attributes, if any.
    // mThemeAttrs = null // TODO TINT THEME Not supported yet a.extractThemeAttrs();

    // This is added in API 11
    val rotate =
        getFloat(
            a,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_ROTATION,
            DefaultRotation
        )

    val pivotX =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_PIVOT_X, DefaultPivotX)
    val pivotY =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_PIVOT_Y, DefaultPivotY)

    // This is added in API 11
    val scaleX =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_SCALE_X, DefaultScaleX)

    // This is added in API 11
    val scaleY =
        getFloat(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_SCALE_Y, DefaultScaleY)

    val translateX =
        getFloat(
            a,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_TRANSLATE_X,
            DefaultTranslationX
        )
    val translateY =
        getFloat(
            a,
            AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_TRANSLATE_Y,
            DefaultTranslationY
        )

    val name: String =
        getString(a, AndroidVectorResources.STYLEABLE_VECTOR_DRAWABLE_GROUP_NAME) ?: ""

    a.recycle()

    builder.addGroup(
        name,
        rotate,
        pivotX,
        pivotY,
        scaleX,
        scaleY,
        translateX,
        translateY,
        EmptyPath
    )
}

/**
 * Class responsible for parsing vector graphics attributes and keeping track of which attributes
 * depend on a configuration parameter. This is used to determine which cached vector graphics
 * objects can be pruned during a configuration change as the vector graphic would need to be
 * reloaded if a corresponding configuration parameter changed.
 *
 * For example, if the fill color for a path was dependent on the orientation of the device the
 * config flag would include the value [android.content.pm.ActivityInfo.CONFIG_ORIENTATION]
 */
internal data class AndroidVectorParser(val xmlParser: XmlPullParser, var config: Int = 0) {
    @JvmField internal val pathParser = PathParser()

    private fun updateConfig(resConfig: Int) {
        config = config or resConfig
    }

    /**
     * Helper method to parse the attributre set update the configuration flags this that these
     * attributes may depend on
     */
    fun obtainAttributes(
        res: Resources,
        theme: Resources.Theme?,
        set: AttributeSet,
        attrs: IntArray
    ): TypedArray {
        val typedArray = TypedArrayUtils.obtainAttributes(res, theme, set, attrs)
        updateConfig(typedArray.changingConfigurations)
        return typedArray
    }

    /**
     * Helper method to parse a boolean with the given resource identifier and update the
     * configuration flags this boolean may depend on.
     */
    fun getBoolean(
        typedArray: TypedArray,
        @StyleableRes resId: Int,
        defaultValue: Boolean
    ): Boolean {
        with(typedArray) {
            val result = getBoolean(resId, defaultValue)
            updateConfig(changingConfigurations)
            return result
        }
    }

    /**
     * Helper method to parse a float with the given resource identifier and update the
     * configuration flags this float may depend on.
     */
    fun getFloat(typedArray: TypedArray, @StyleableRes index: Int, defaultValue: Float): Float {
        with(typedArray) {
            val result = getFloat(index, defaultValue)
            updateConfig(changingConfigurations)
            return result
        }
    }

    /**
     * Helper method to parse an int with the given resource identifier and update the configuration
     * flags this int may depend on.
     */
    fun getInt(typedArray: TypedArray, index: Int, defaultValue: Int): Int {
        with(typedArray) {
            val result = getInt(index, defaultValue)
            updateConfig(changingConfigurations)
            return result
        }
    }

    /**
     * Helper method to parse a String with the given resource identifier and update the
     * configuration flags this String may depend on.
     */
    fun getString(typedArray: TypedArray, index: Int): String? {
        with(typedArray) {
            val result = getString(index)
            updateConfig(changingConfigurations)
            return result
        }
    }

    /**
     * Helper method to parse a dimension with the given resource identifier and update the
     * configuration flags this dimension may depend on.
     */
    fun getDimension(typedArray: TypedArray, index: Int, defValue: Float): Float {
        with(typedArray) {
            val result = getDimension(index, defValue)
            updateConfig(changingConfigurations)
            return result
        }
    }

    /**
     * Helper method to parse a ComplexColor with the given resource identifier and update the
     * configuration flags this ComplexColor may depend on.
     */
    fun getComplexColor(
        typedArray: TypedArray,
        theme: Resources.Theme?,
        @StyleableRes resId: Int,
        @ColorInt defaultValue: Int
    ): ComplexColorCompat {
        with(typedArray) {
            val result = TypedArrayUtils.getComplexColor(this, theme, resId, defaultValue)
            updateConfig(changingConfigurations)
            return result
        }
    }

    /**
     * Helper method to parse a ColorStateList with the given resource identifier and update the
     * configuration flags this ColorStateList may depend on.
     */
    fun getColorStateList(
        typedArray: TypedArray,
        theme: Resources.Theme?,
        @StyleableRes resId: Int
    ): ColorStateList? {
        with(typedArray) {
            val result = TypedArrayUtils.getColorStateList(typedArray, theme, resId)
            updateConfig(changingConfigurations)
            return result
        }
    }
}
