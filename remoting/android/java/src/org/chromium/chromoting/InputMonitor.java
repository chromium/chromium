// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Rect;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.ViewConfiguration;

import org.chromium.chromoting.InputState.DetectedAction;
import org.chromium.chromoting.InputState.StartAction;

/**
 * A combination of existing Android and chromium motion and touch detectors, and provide a set of
 * {@link Event} when each kind of touch behavior has been detected.
 */
public final class InputMonitor
        implements GestureDetector.OnGestureListener,
                   ScaleGestureDetector.OnScaleGestureListener,
                   TapGestureDetector.OnTapListener {
    /** Tap with one or more fingers. */
    private final Event.Raisable<TapEventParameter> mOnTap;

    /** Long press and hold with one or more fingers. */
    private final Event.Raisable<TapEventParameter> mOnPressAndHold;

    /** Any motion event received. */
    private final Event.Raisable<MotionEvent> mOnTouchEvent;

    /** Scroll with two fingers. */
    private final Event.Raisable<TwoPointsEventParameter> mOnScroll;

    /** Fling with two fingers. */
    private final Event.Raisable<TwoPointsEventParameter> mOnScrollFling;

    /** Fling with one finger. */
    private final Event.Raisable<TwoPointsEventParameter> mOnFling;

    /** Scale with two fingers. */
    private final Event.Raisable<ScaleEventParameter> mOnScale;

    /** Swipe with three or more fingers. */
    private final Event.Raisable<TwoPointsEventParameter> mOnSwipe;

    /** Move with one finger. */
    private final Event.Raisable<TwoPointsEventParameter> mOnMove;

    private final InputState.Settable mInputState;
    private final int mEdgeSlopInPx;
    private final float mSwipeThreshold;
    private final GestureDetector mScroller;
    private final ScaleGestureDetector mZoomer;
    private final TapGestureDetector mTapDetector;
    private final SwipePinchDetector mSwipePinchDetector;

    private Rect mPanGestureBounds;

    InputMonitor(DesktopView view, RenderStub renderStub, Context context) {
        mOnTap = new Event.Raisable<>();
        mOnPressAndHold = new Event.Raisable<>();
        mOnTouchEvent = new Event.Raisable<>();
        mOnScroll = new Event.Raisable<>();
        mOnScrollFling = new Event.Raisable<>();
        mOnFling = new Event.Raisable<>();
        mOnScale = new Event.Raisable<>();
        mOnSwipe = new Event.Raisable<>();
        mOnMove = new Event.Raisable<>();
        mInputState = new InputState.Settable();
        mEdgeSlopInPx = ViewConfiguration.get(context).getScaledEdgeSlop();
        mSwipeThreshold = 40 * context.getResources().getDisplayMetrics().density;
        mScroller = new GestureDetector(context, this, null, false);
        mScroller.setIsLongpressEnabled(false);
        mZoomer = new ScaleGestureDetector(context, this);
        mTapDetector = new TapGestureDetector(context, this);
        mSwipePinchDetector = new SwipePinchDetector(context);
        renderStub.onClientSizeChanged().add(
                new Event.ParameterRunnable<SizeChangedEventParameter>() {
                    @Override
                    public void run(SizeChangedEventParameter param) {
                        handleClientSizeChanged(param);
                    }
                });
        // Currently we support only touch events.
        view.onTouch().add(
                new Event.ParameterRunnable<TouchEventParameter>() {
                    @Override
                    public void run(TouchEventParameter param) {
                        handleTouch(param);
                    }
                });
    }

    // -------------- Getters -------------------------------------------------------------
    public Event<TapEventParameter> onTap() {
        return mOnTap;
    }

    public Event<TapEventParameter> onPressAndHold() {
        return mOnPressAndHold;
    }

    public Event<MotionEvent> onTouchEvent() {
        return mOnTouchEvent;
    }

    public Event<TwoPointsEventParameter> onScroll() {
        return mOnScroll;
    }

    public Event<TwoPointsEventParameter> onScrollFling() {
        return mOnScrollFling;
    }

    public Event<TwoPointsEventParameter> onFling() {
        return mOnFling;
    }

    public Event<ScaleEventParameter> onScale() {
        return mOnScale;
    }

    public Event<TwoPointsEventParameter> onSwipe() {
        return mOnSwipe;
    }

    public Event<TwoPointsEventParameter> onMove() {
        return mOnMove;
    }

    public InputState inputState() {
        return mInputState;
    }

    // -------------- Implementations of GestureDetector.OnGestureListener ----------------

    /** Called whenever a gesture starts. Always accepts the gesture so it isn't ignored. */
    @Override
    public boolean onDown(MotionEvent e) {
        return true;
    }

    /** Called when a fling gesture is recognized. */
    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        if (mInputState.shouldSuppressFling()) {
            return false;
        }

        if (mInputState.isScrollFling()) {
            mInputState.setDetectedAction(DetectedAction.AFTER_SCROLL_FLING);
            mOnScrollFling.raise(new TwoPointsEventParameter(e1, e2, velocityX, velocityY));
            return true;
        }

        if (mInputState.shouldSuppressCursorMovement()) {
            return false;
        }

        mInputState.setDetectedAction(DetectedAction.FLING);
        mOnFling.raise(new TwoPointsEventParameter(e1, e2, velocityX, velocityY));
        return true;
    }

    /** Called when a long-press is triggered for one or more fingers. */
    @Override
    public void onLongPress(MotionEvent e) {}

    /** Called when the user drags one or more fingers across the touchscreen. */
    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        if (!isInPanGestureBounds(e1)) {
            // The gesture of scrolling from edge to the center should be handled by Android OS.
            mInputState.setDetectedAction(InputState.DetectedAction.SCROLL_EDGE);
            return false;
        }

        if (e2.getPointerCount() >= 3 && !mInputState.swipeCompleted()) {
            if (distanceY > mSwipeThreshold || distanceY < -mSwipeThreshold) {
                mInputState.setDetectedAction(DetectedAction.SWIPE);
                mOnSwipe.raise(new TwoPointsEventParameter(e1, e2, distanceX, distanceY));
                return true;
            }
            return false;
        }

        if (e2.getPointerCount() == 2 && mSwipePinchDetector.isSwiping()) {
            mInputState.setDetectedAction(DetectedAction.SCROLL);
            mOnScroll.raise(new TwoPointsEventParameter(e1, e2, distanceX, distanceY));
            return true;
        }

        if (e2.getPointerCount() != 1 || mInputState.shouldSuppressCursorMovement()) {
            return false;
        }

        mInputState.setDetectedAction(DetectedAction.MOVE);
        mOnMove.raise(new TwoPointsEventParameter(e1, e2, distanceX, distanceY));
        return true;
    }

    /** Called by {@link GestureDetector}, does nothing. */
    @Override
    public void onShowPress(MotionEvent e) {}

    /** Called by {@link GestureDetector}, returns false to continue following gesture detection. */
    @Override
    public boolean onSingleTapUp(MotionEvent e) {
        return false;
    }

    // --------- Implementations of ScaleGestureDetector.OnScaleGestureListener -----------
    /** Called when the user is in the process of pinch-zooming. */
    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        if (!mSwipePinchDetector.isPinching()) {
            return false;
        }

        mInputState.setDetectedAction(DetectedAction.SCALE);
        mOnScale.raise(new ScaleEventParameter(detector.getScaleFactor(),
                                               detector.getFocusX(),
                                               detector.getFocusY()));
        return true;
    }

    /**
     * Called when the user starts to zoom. Always accepts the zoom so that
     * onScale() can decide whether to respond to it.
     */
    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        return true;
    }

    /** Called when the user is done zooming. Defers to onScale()'s judgement. */
    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        onScale(detector);
    }

    // -------------- Implementations of TapGestureDetector.OnTapListener ----------------
    /** Called when the user taps the screen with one or more fingers. */
    @Override
    public boolean onTap(int pointerCount, float x, float y) {
        TapEventParameter para = new TapEventParameter(pointerCount, x, y);
        mOnTap.raise(para);
        return para.handled;
    }

    /** Called when a long-press is triggered for one or more fingers. */
    @Override
    public void onLongPress(int pointerCount, float x, float y) {
        TapEventParameter para = new TapEventParameter(pointerCount, x, y);
        mOnPressAndHold.raise(para);
        if (para.handled) {
            mInputState.setStartAction(StartAction.LONG_PRESS);
        }
    }

    /**
     * Returns a boolean value to indicate whether the MotionEvent is in the range of
     * {@link mPanGestureBounds}
     */
    private boolean isInPanGestureBounds(MotionEvent e) {
        Preconditions.notNull(e);
        return mPanGestureBounds == null
               || mPanGestureBounds.contains((int) e.getX(), (int) e.getY());
    }

    // ---------------------------- Event handlers ---------------------------------------
    private void handleClientSizeChanged(SizeChangedEventParameter parameter) {
        mPanGestureBounds = new Rect(mEdgeSlopInPx,
                                     mEdgeSlopInPx,
                                     parameter.width - mEdgeSlopInPx,
                                     parameter.height - mEdgeSlopInPx);
    }

    private void handleTouch(TouchEventParameter parameter) {
        mOnTouchEvent.raise(parameter.event);

        boolean handled = mScroller.onTouchEvent(parameter.event);
        handled |= mZoomer.onTouchEvent(parameter.event);
        handled |= mTapDetector.onTouchEvent(parameter.event);
        mSwipePinchDetector.onTouchEvent(parameter.event);

        parameter.handled = handled;
    }
}
