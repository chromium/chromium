/*
 * Copyright (C) 2014 The Android Open Source Project
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

package org.chromium.third_party.android.swiperefresh;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.OvalShape;
import android.view.View;
import android.view.animation.Animation;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

/**
 * Private class created to work around issues with AnimationListeners being
 * called before the animation is actually complete and support shadows on older
 * platforms.
 *
 * @hide
 */
public class CircleImageView extends ImageView {
    private static final int KEY_SHADOW_COLOR = 0x1E000000;
    private static final int FILL_SHADOW_COLOR = 0x3D000000;
    // PX
    private static final float X_OFFSET = 0f;
    private static final float Y_OFFSET = 1.75f;
    private static final float SHADOW_RADIUS = 3.5f;
    private static final int SHADOW_ELEVATION = 4;

    private Animation.AnimationListener mListener;
    private int mShadowRadius;
    private int mOuterRadius;
    private @ColorInt int mOuterColor;
    private float mDensity;
    private int mViewDimension;

    public CircleImageView(Context context, int color, final float radius) {
        this(context, color, radius, 0.f, 0);
    }

    @SuppressWarnings("deprecation")
    public CircleImageView(Context context, @ColorRes int color, float radius, float maxRadius,
            @ColorInt int outerColor) {
        super(context);
        final float density = getContext().getResources().getDisplayMetrics().density;

        mShadowRadius = (int) (density * SHADOW_RADIUS);
        mViewDimension = (int) (maxRadius > 0.f ? (maxRadius + radius) * density : mShadowRadius);
        mOuterColor = outerColor;

        ShapeDrawable circle;
        if (elevationSupported() && maxRadius == 0.f) {
            circle = initializeElevated(density);
        } else {
            circle = initializeNonElevated(radius, density);
        }
        circle.getPaint().setColor(color);
        if(android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.JELLY_BEAN) {
            setBackgroundDrawable(circle);
        } else {
            setBackground(circle);
        }
        mDensity = density;
    }

    ShapeDrawable initializeElevated(float density) {
        ShapeDrawable circle = new ShapeDrawable(new OvalShape());
        setElevation(SHADOW_ELEVATION * density);
        return circle;
    }

    ShapeDrawable initializeNonElevated(float radius, float density) {
        final int diameter = (int) (radius * density * 2);
        final int shadowYOffset = (int) (density * Y_OFFSET);
        final int shadowXOffset = (int) (density * X_OFFSET);
        OvalShape oval = new OvalShadow(mShadowRadius, diameter);
        ShapeDrawable circle = new ShapeDrawable(oval);
        setLayerType(View.LAYER_TYPE_SOFTWARE, circle.getPaint());
        circle.getPaint().setShadowLayer(mShadowRadius, shadowXOffset, shadowYOffset,
                KEY_SHADOW_COLOR);

        final int padding = mViewDimension;
        // set padding so the inner image sits correctly within the shadow.
        setPadding(padding, padding, padding, padding);
        return circle;
    }

    private boolean elevationSupported() {
        return android.os.Build.VERSION.SDK_INT >= 21;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        if (!elevationSupported() || mViewDimension > mShadowRadius) {
            setMeasuredDimension(getMeasuredWidth() + mViewDimension * 2,
                    getMeasuredHeight() + mViewDimension * 2);
        }
    }

    public void setAnimationListener(Animation.AnimationListener listener) {
        mListener = listener;
    }

    @Override
    public void onAnimationStart() {
        super.onAnimationStart();
        if (mListener != null) {
            mListener.onAnimationStart(getAnimation());
        }
    }

    @Override
    public void onAnimationEnd() {
        super.onAnimationEnd();
        if (mListener != null) {
            mListener.onAnimationEnd(getAnimation());
        }
    }

    /**
     * Update the background color of the circle image view.
     *
     * @param colorRes Id of a color resource.
     */
    public void setBackgroundColorRes(int colorRes) {
        setBackgroundColor(getContext().getResources().getColor(colorRes));
    }

    @Override
    public void setBackgroundColor(int color) {
        if (getBackground() instanceof ShapeDrawable) {
            ((ShapeDrawable) getBackground()).getPaint().setColor(color);
        }
    }

    public void setProgress(float progress) {
        mOuterRadius = (int) (progress * mViewDimension);
    }

    private class OvalShadow extends OvalShape {
        private RadialGradient mRadialGradient;
        private int mShadowRadius;
        private Paint mShadowPaint;
        private Paint mOuterPaint;
        private int mCircleDiameter;

        public OvalShadow(int shadowRadius, int circleDiameter) {
            super();
            mShadowPaint = new Paint();
            mShadowRadius = shadowRadius;
            mCircleDiameter = circleDiameter;
            mRadialGradient = new RadialGradient(mCircleDiameter / 2f, mCircleDiameter / 2f,
                    mShadowRadius, new int[] {
                            FILL_SHADOW_COLOR, Color.TRANSPARENT
                    }, null, Shader.TileMode.CLAMP);
            mShadowPaint.setShader(mRadialGradient);
        }

        @Override
        public void draw(Canvas canvas, Paint paint) {
            final int viewWidth = CircleImageView.this.getWidth();
            final int viewHeight = CircleImageView.this.getHeight();
            canvas.drawCircle(viewWidth / 2f, viewHeight / 2f, (mCircleDiameter / 2f + mShadowRadius),
                    mShadowPaint);
            if (mOuterRadius > 0.f) {
                if (mOuterPaint == null) {
                    mOuterPaint = new Paint();
                    mOuterPaint.setColor(getResources().getColor(mOuterColor));
                    mOuterPaint.setAlpha(0x80);
                }
                canvas.drawCircle(viewWidth / 2f, viewHeight / 2f, mOuterRadius, mOuterPaint);
            }
            canvas.drawCircle(viewWidth / 2f, viewHeight / 2f, (mCircleDiameter / 2f), paint);
        }
    }
}
