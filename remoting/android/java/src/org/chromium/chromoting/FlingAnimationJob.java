// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.widget.Scroller;

/**
 * This is the abstract class that implements a fling animation job.
 * This class uses an android scroller to calculate the fling physics.
 * Subclass will only need to implement the abstracted processAction(deltaX, deltaY) method to
 * consume the calculated change of x and y.
 */
public abstract class FlingAnimationJob implements AnimationJob {
    /** Used to calculate the flinging physics. */
    // Note that the implementation of scroller.isFinished() may not be reliable
    // since it (mFinished) will only be updated when computeScrollOffset() is called.
    private Scroller mScroller;

    /**
     * Consumes deltaX and deltaY calculated by the scroller.
     * This method will be called in processAnimation(). Subclasses should implement this method
     * to consume the calculated change of x and y.
     * @param deltaX change of x
     * @param deltaY change of y
     */
    protected abstract void processAction(float deltaX, float deltaY);

    /**
     * Constructs a FlingAnimationJob (used by subclasses for initialization).
     * @param context the context to initialize the scroller
     */
    protected FlingAnimationJob(Context context) {
        mScroller = new Scroller(context);
    }

    /**
     * Initializes the scroller and start the scrolling animation.
     * @param velocityX fling start velocity of x
     * @param velocityY fling start velocity of y
     */
    public void startAnimation(float velocityX, float velocityY) {
        // The fling physics calculation is based on screen coordinates, so that it will
        // behave consistently at different zoom levels (and will work nicely at high zoom
        // levels, since |mScroller| outputs integer coordinates). However, the desktop
        // will usually be panned as the cursor is moved across the desktop, which means the
        // transformation mapping from screen to desktop coordinates will change. To deal with
        // this, the cursor movement is computed from relative coordinate changes from
        // |mScroller|. This means the fling can be started at (0, 0) with no bounding
        // constraints - the cursor is already constrained by the desktop size.
        mScroller.fling(0, 0, (int) velocityX, (int) velocityY,
                Integer.MIN_VALUE, Integer.MAX_VALUE,
                Integer.MIN_VALUE, Integer.MAX_VALUE);
        mScroller.computeScrollOffset();
    }

    @Override
    public boolean processAnimation() {
        int previousX = mScroller.getCurrX();
        int previousY = mScroller.getCurrY();
        if (!mScroller.computeScrollOffset()) {
            return false;
        }
        int deltaX = mScroller.getCurrX() - previousX;
        int deltaY = mScroller.getCurrY() - previousY;
        processAction(deltaX, deltaY);
        return true;
    }

    @Override
    public void abortAnimation() {
        mScroller.abortAnimation();
    }
}