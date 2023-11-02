// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.PointF;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

/**
 * This class receives local touch events and translates them into the appropriate mouse based
 * events for the remote host.  The net result is that the local input method feels like a touch
 * interface but the remote host will be given mouse events to inject.
 */
public class SimulatedTouchInputStrategy implements InputStrategyInterface {
    /** Used to adjust the size of the region used for double tap detection. */
    private static final float DOUBLE_TAP_SLOP_SCALE_FACTOR = 0.25f;

    private final RenderData mRenderData;
    private final InputEventSender mInjector;

    /**
     * Stores the time of the most recent left button single tap processed.
     */
    private long mLastTapTimeInMs;

    /**
     * Stores the position of the last left button single tap processed.
     */
    private PointF mLastTapPoint;

    /**
     * The maximum distance, in pixels, between two points in order for them to be considered a
     * double tap gesture.
     */
    private final int mDoubleTapSlopSquareInPx;

    /**
     * The interval, measured in milliseconds, in which two consecutive left button taps must
     * occur in order to be considered a double tap gesture.
     */
    private final long mDoubleTapDurationInMs;

    /** Mouse-button currently held down, or BUTTON_UNDEFINED otherwise. */
    private int mHeldButton = InputStub.BUTTON_UNDEFINED;

    public SimulatedTouchInputStrategy(
            RenderData renderData, InputEventSender injector, Context context) {
        Preconditions.notNull(injector);
        mRenderData = renderData;
        mInjector = injector;

        mDoubleTapDurationInMs = ViewConfiguration.getDoubleTapTimeout();

        // In order to detect whether the user is attempting to double tap a target, we define a
        // region around the first point within which the second tap must occur.  The standard way
        // to do this in an Android UI (meaning a UI comprised of UI elements which conform to the
        // visual guidelines for the platform which are 'Touch Friendly') is to use the
        // getScaledDoubleTapSlop() value for checking this distance (or use a GestureDetector).
        // Our scenario is a bit different as our UI consists of an image of a remote machine where
        // the UI elements were probably designed for mouse and keyboard (meaning smaller targets)
        // and the image itself which can be zoomed to change the size of the targets.  Ths adds up
        // to the target to be invoked often being either larger or much smaller than a standard
        // Android UI element.  Our approach to this problem is to make double-tap detection
        // consistent regardless of the zoom level or remote target size so that the user can rely
        // on their muscle memory when interacting with our UI.  With respect to the original
        // problem, getScaledDoubleTapSlop() gives a value which is optimized for an Android based
        // UI however this value is too large for interacting with remote elements in our app.
        // Our solution is to use the original value from getScaledDoubleTapSlop() (which includes
        // scaling to account for display differences between devices) and apply a fudge/scale
        // factor to make the interaction more intuitive and useful for our scenario.
        ViewConfiguration config = ViewConfiguration.get(context);
        int scaledDoubleTapSlopInPx = config.getScaledDoubleTapSlop();
        scaledDoubleTapSlopInPx = (int) (scaledDoubleTapSlopInPx * DOUBLE_TAP_SLOP_SCALE_FACTOR);
        mDoubleTapSlopSquareInPx = scaledDoubleTapSlopInPx * scaledDoubleTapSlopInPx;

        mRenderData.drawCursor = false;
    }

    @Override
    public boolean onTap(int button) {
        PointF currentTapPoint = getCursorPosition();
        if (button == InputStub.BUTTON_LEFT) {
            // Left clicks are handled a little differently than the events for other buttons.
            // This is needed because translating touch events to mouse events has a problem with
            // location consistency for double clicks.  If you take the center location of each tap
            // and inject them as mouse clicks, the distance between those two points will often
            // cause the remote OS to recognize the gesture as two distinct clicks instead of a
            // double click.  In order to increase the success rate of double taps/clicks, we
            // squirrel away the time and coordinates of each single tap and if we detect the user
            // attempting a double tap, we use the original event's location for that second tap.
            long tapInterval = SystemClock.uptimeMillis() - mLastTapTimeInMs;
            if (isDoubleTap(currentTapPoint.x, currentTapPoint.y, tapInterval)) {
                currentTapPoint = new PointF(mLastTapPoint.x, mLastTapPoint.y);
                mLastTapPoint = null;
                mLastTapTimeInMs = 0;
            } else {
                mLastTapPoint = currentTapPoint;
                mLastTapTimeInMs = SystemClock.uptimeMillis();
            }
        } else {
            mLastTapPoint = null;
            mLastTapTimeInMs = 0;
        }

        mInjector.sendMouseClick(currentTapPoint, button);
        return true;
    }

    @Override
    public boolean onPressAndHold(int button) {
        mInjector.sendMouseDown(getCursorPosition(), button);
        mHeldButton = button;
        return true;
    }

    @Override
    public void onScroll(float distanceX, float distanceY) {
        mInjector.sendReverseMouseWheelEvent(distanceX, distanceY);
    }

    @Override
    public void onMotionEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP
                && mHeldButton != InputStub.BUTTON_UNDEFINED) {
            mInjector.sendMouseUp(getCursorPosition(), mHeldButton);
            mHeldButton = InputStub.BUTTON_UNDEFINED;
        }
    }

    @Override
    public void injectCursorMoveEvent(int x, int y) {
        mInjector.sendCursorMove(x, y);
    }

    @Override
    public @RenderStub.InputFeedbackType int getShortPressFeedbackType() {
        return RenderStub.InputFeedbackType.SHORT_TOUCH_ANIMATION;
    }

    @Override
    public @RenderStub.InputFeedbackType int getLongPressFeedbackType() {
        return RenderStub.InputFeedbackType.LONG_TOUCH_ANIMATION;
    }

    @Override
    public boolean isIndirectInputMode() {
        return false;
    }

    private PointF getCursorPosition() {
        return mRenderData.getCursorPosition();
    }

    private boolean isDoubleTap(float currentX, float currentY, long tapInterval) {
        if (tapInterval > mDoubleTapDurationInMs || mLastTapPoint == null) {
            return false;
        }

        // Convert the image based coordinates back to screen coordinates so the user experiences
        // consistent double tap behavior regardless of zoom level.
        //
        float[] currentValues = {currentX, currentY};
        float[] previousValues = {mLastTapPoint.x, mLastTapPoint.y};

        mRenderData.transform.mapPoints(currentValues);
        mRenderData.transform.mapPoints(previousValues);

        int deltaX = (int) (currentValues[0] - previousValues[0]);
        int deltaY = (int) (currentValues[1] - previousValues[1]);
        return ((deltaX * deltaX + deltaY * deltaY) <= mDoubleTapSlopSquareInPx);
    }
}
