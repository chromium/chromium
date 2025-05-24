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

/** Constants used to resolve VectorDrawable attributes during xml inflation */
internal object AndroidVectorResources {

    // Resources ID generated in the latest R.java for framework.
    val STYLEABLE_VECTOR_DRAWABLE_TYPE_ARRAY =
        intArrayOf(
            android.R.attr.name,
            android.R.attr.tint,
            android.R.attr.height,
            android.R.attr.width,
            android.R.attr.alpha,
            android.R.attr.autoMirrored,
            android.R.attr.tintMode,
            android.R.attr.viewportWidth,
            android.R.attr.viewportHeight
        )
    val STYLEABLE_VECTOR_DRAWABLE_ALPHA = 4
    val STYLEABLE_VECTOR_DRAWABLE_AUTO_MIRRORED = 5
    val STYLEABLE_VECTOR_DRAWABLE_HEIGHT = 2
    val STYLEABLE_VECTOR_DRAWABLE_NAME = 0
    val STYLEABLE_VECTOR_DRAWABLE_TINT = 1
    val STYLEABLE_VECTOR_DRAWABLE_TINT_MODE = 6
    val STYLEABLE_VECTOR_DRAWABLE_VIEWPORT_HEIGHT = 8
    val STYLEABLE_VECTOR_DRAWABLE_VIEWPORT_WIDTH = 7
    val STYLEABLE_VECTOR_DRAWABLE_WIDTH = 3
    val STYLEABLE_VECTOR_DRAWABLE_GROUP =
        intArrayOf(
            android.R.attr.name,
            android.R.attr.pivotX,
            android.R.attr.pivotY,
            android.R.attr.scaleX,
            android.R.attr.scaleY,
            android.R.attr.rotation,
            android.R.attr.translateX,
            android.R.attr.translateY
        )
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_NAME = 0
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_PIVOT_X = 1
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_PIVOT_Y = 2
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_ROTATION = 5
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_SCALE_X = 3
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_SCALE_Y = 4
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_TRANSLATE_X = 6
    val STYLEABLE_VECTOR_DRAWABLE_GROUP_TRANSLATE_Y = 7
    val STYLEABLE_VECTOR_DRAWABLE_PATH =
        intArrayOf(
            android.R.attr.name,
            android.R.attr.fillColor,
            android.R.attr.pathData,
            android.R.attr.strokeColor,
            android.R.attr.strokeWidth,
            android.R.attr.trimPathStart,
            android.R.attr.trimPathEnd,
            android.R.attr.trimPathOffset,
            android.R.attr.strokeLineCap,
            android.R.attr.strokeLineJoin,
            android.R.attr.strokeMiterLimit,
            android.R.attr.strokeAlpha,
            android.R.attr.fillAlpha,
            // android.R.attr.fillType is in API level 24+ use hardcoded value to extract the
            // attribute if it exists
            0x101051E
        )
    val STYLEABLE_VECTOR_DRAWABLE_PATH_FILL_ALPHA = 12
    val STYLEABLE_VECTOR_DRAWABLE_PATH_FILL_COLOR = 1
    val STYLEABLE_VECTOR_DRAWABLE_PATH_NAME = 0
    val STYLEABLE_VECTOR_DRAWABLE_PATH_PATH_DATA = 2
    val STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_ALPHA = 11
    val STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_COLOR = 3
    val STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_LINE_CAP = 8
    val STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_LINE_JOIN = 9
    val STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_MITER_LIMIT = 10
    val STYLEABLE_VECTOR_DRAWABLE_PATH_STROKE_WIDTH = 4
    val STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_END = 6
    val STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_OFFSET = 7
    val STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_START = 5
    val STYLEABLE_VECTOR_DRAWABLE_PATH_TRIM_PATH_FILLTYPE = 13
    val STYLEABLE_VECTOR_DRAWABLE_CLIP_PATH =
        intArrayOf(android.R.attr.name, android.R.attr.pathData)
    val STYLEABLE_VECTOR_DRAWABLE_CLIP_PATH_NAME = 0
    val STYLEABLE_VECTOR_DRAWABLE_CLIP_PATH_PATH_DATA = 1
}
