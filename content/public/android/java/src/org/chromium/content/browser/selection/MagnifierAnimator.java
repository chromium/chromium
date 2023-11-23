// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.animation.ValueAnimator;
import android.view.animation.LinearInterpolator;

import org.chromium.base.Log;

/**
 * MagnifierAnimator adds animation to MagnifierWrapper when there is a change in y direction.
 * MagnifierWrapper class isolated P APIs out so we could write test for MagnifierAnimator.
 */
public class MagnifierAnimator {
    private static final boolean DEBUG = false;
    private static final String TAG = "Magnifier";

    private static final long DURATION_MS = 100;

    private MagnifierWrapper mMagnifier;
    private ValueAnimator mAnimator;

    private boolean mMagnifierIsShowing;

    // Current point for showing magnifier.
    private float mAnimationCurrentX;
    private float mAnimationCurrentY;

    // Start point for animation.
    private float mAnimationStartX;
    private float mAnimationStartY;

    // Target point for end of animation.
    private float mTargetX;
    private float mTargetY;

    /** Constructor. */
    public MagnifierAnimator(MagnifierWrapper magnifier) {
        mMagnifier = magnifier;

        createValueAnimator();

        mTargetX = -1.0f;
        mTargetY = -1.0f;
    }

    public void handleDragStartedOrMoved(float x, float y) {
        if (!mMagnifier.isAvailable()) return;
        if (DEBUG) {
            Log.i(TAG, "handleDragStartedOrMoved: " + "(" + x + ", " + y + ")");
        }
        // We only do animation if this is not the first time to show magnifier and y coordinate
        // is different from last target.
        if (mMagnifierIsShowing && y != mTargetY) {
            // If the animator is running, we cancel it and reset the starting point to current
            // point, then start a new animation. Otherwise set the starting point to previous
            // place.
            if (mAnimator.isRunning()) {
                mAnimator.cancel();
                // Create another ValueAnimator because the test uses
                // ValueAnimator#setCurrentFraction() to adjust animation fraction, the internal
                // fraction is not being reset in ValueAnimator#cancel(), so the next
                // ValueAnimator#start() will use this fraction first, which will cause a jump.
                createValueAnimator();
                mAnimationStartX = mAnimationCurrentX;
                mAnimationStartY = mAnimationCurrentY;
            } else {
                mAnimationStartX = mTargetX;
                mAnimationStartY = mTargetY;
            }
            mAnimator.start();
        } else {
            if (!mAnimator.isRunning()) mMagnifier.show(x, y);
            // if mAnimator is running, we simply change the target coordinate. x-coordinate won't
            // change dramatically, so let the previous animation finish to the target point.
        }

        mTargetX = x;
        mTargetY = y;

        mMagnifierIsShowing = true;
    }

    public void handleDragStopped() {
        mMagnifier.dismiss();
        mAnimator.cancel();
        mMagnifierIsShowing = false;
    }

    public void childLocalSurfaceIdChanged() {
        mMagnifier.childLocalSurfaceIdChanged();
    }

    /* package */ ValueAnimator getValueAnimatorForTesting() {
        return mAnimator;
    }

    private float currentValue(float start, float target, ValueAnimator animation) {
        return start + (target - start) * animation.getAnimatedFraction();
    }

    private void createValueAnimator() {
        mAnimator = ValueAnimator.ofFloat(0, 1);
        mAnimator.setDuration(DURATION_MS);
        mAnimator.setInterpolator(new LinearInterpolator());
        mAnimator.addUpdateListener(
                animation -> {
                    mAnimationCurrentX = currentValue(mAnimationStartX, mTargetX, animation);
                    mAnimationCurrentY = currentValue(mAnimationStartY, mTargetY, animation);
                    mMagnifier.show(mAnimationCurrentX, mAnimationCurrentY);
                });
    }
}
