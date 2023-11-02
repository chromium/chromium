// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
import android.graphics.PointF;
import android.view.MotionEvent;

import java.util.LinkedList;
import java.util.Queue;

/**
 * This class receives local touch input events and forwards them to the remote host.
 * A queue of MotionEvents is built up and then either transmitted to the remote host if one of its
 * remote gesture handler methods is called (such as onScroll) or it is cleared if the current
 * stream of events does not represent a remote gesture.
 * NOTE: Not all touch gestures are remoted.  Touch input and gestures outside the supported ones
 *       (which includes tapping and 2 finger panning) will either affect the local canvas or
 *       will be dropped/ignored.
 */
public class TouchInputStrategy implements InputStrategyInterface {
    /**
     * Contains the maximum number of MotionEvents to store before cancelling the current gesture.
     * The size is ~3x the largest number of events seen during any remotable gesture sequence.
     */
    private static final int QUEUED_EVENT_THRESHOLD = 50;

    /**
     * Contains the set of MotionEvents received for the current gesture candidate.  If one of the
     * gesture handling methods is called, these queued events will be transmitted to the remote
     * host for injection.  The queue has a maximum size determined by |QUEUED_EVENT_THRESHOLD| to
     * prevent a live memory leak where the queue grows unbounded during a local gesture (such as
     * someone panning the local canvas continuously for several seconds/minutes).
     */
    private Queue<MotionEvent> mQueuedEvents;

    /**
     * Indicates that the events received should be treated as part of an active remote gesture.
     */
    private boolean mInRemoteGesture;

    /**
     * Indicates whether MotionEvents and gestures should be acted upon or ignored.  This flag is
     * set when we believe that the current sequence of events is not something we should remote.
     */
    private boolean mIgnoreTouchEvents;

    private final RenderData mRenderData;
    private final InputEventSender mInjector;

    public TouchInputStrategy(RenderData renderData, InputEventSender injector) {
        Preconditions.notNull(injector);
        mRenderData = renderData;
        mInjector = injector;
        mQueuedEvents = new LinkedList<MotionEvent>();

        mRenderData.drawCursor = false;
    }

    @Override
    public boolean onTap(int button) {
        if (mQueuedEvents.isEmpty() || mIgnoreTouchEvents) {
            return false;
        }

        switch (button) {
            case InputStub.BUTTON_LEFT:
                injectQueuedEvents();
                return true;

            case InputStub.BUTTON_RIGHT:
                // Using the mouse for right-clicking is consistent across all host platforms.
                // Right-click gestures are often platform specific and can be tricky to simulate.

                // Grab the first queued event which should be the initial ACTION_DOWN event.
                MotionEvent downEvent = mQueuedEvents.peek();
                assert downEvent.getActionMasked() == MotionEvent.ACTION_DOWN;

                mInjector.sendMouseClick(new PointF(downEvent.getX(), downEvent.getY()),
                        InputStub.BUTTON_RIGHT);
                clearQueuedEvents();
                return true;

            default:
                // Tap gestures for > 2 fingers are not supported.
                return false;
        }
    }

    @Override
    public boolean onPressAndHold(int button) {
        if (button != InputStub.BUTTON_LEFT || mQueuedEvents.isEmpty()
                || mIgnoreTouchEvents) {
            return false;
        }

        mInRemoteGesture = true;
        injectQueuedEvents();
        return true;
    }

    @Override
    public void onScroll(float distanceX, float distanceY) {
        if (mIgnoreTouchEvents || mInRemoteGesture) {
            return;
        }

        mInRemoteGesture = true;
        injectQueuedEvents();
    }

    @Override
    public void onMotionEvent(MotionEvent event) {
        // MotionEvents received are stored in a queue.  This queue is added to until one of the
        // gesture handling methods is called to indicate that a remote gesture is in progress.  At
        // that point, each enqueued MotionEvent is dequeued and transmitted to the remote machine
        // and the class will now forward all MotionEvents received in real time until the gesture
        // has been completed.  If we receive too many events without having been notified to start
        // a remote gesture, then the queue is cleared and we will wait until the start of the next
        // gesture to begin queueing again.
        int action = event.getActionMasked();
        if (mIgnoreTouchEvents && action != MotionEvent.ACTION_DOWN) {
            return;
        } else if (mQueuedEvents.size() > QUEUED_EVENT_THRESHOLD) {
            // Since we maintain a queue of events to replay once the gesture is known, we need to
            // ensure that we do not continue to queue events when we are reasonably sure that the
            // user action is not going to be sent to the remote host.
            mIgnoreTouchEvents = true;
            clearQueuedEvents();
            return;
        }

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                resetStateData();
                mQueuedEvents.add(transformToRemoteCoordinates(event));
                break;

            case MotionEvent.ACTION_POINTER_DOWN:
                if (mInRemoteGesture) {
                    // Cancel the current gesture if a pointer down action is seen during it.
                    // We do this because a new pointer down means that we are no longer performing
                    // the old gesture.
                    mIgnoreTouchEvents = true;
                    clearQueuedEvents();
                } else {
                    mQueuedEvents.add(transformToRemoteCoordinates(event));
                }
                break;

            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_UP:
                event = transformToRemoteCoordinates(event);
                if (mInRemoteGesture) {
                    mInjector.sendTouchEvent(event);
                    event.recycle();
                } else {
                    mQueuedEvents.add(event);
                }
                break;

            default:
                break;
        }
    }

    @Override
    public void injectCursorMoveEvent(int x, int y) {}

    @Override
    public @RenderStub.InputFeedbackType int getShortPressFeedbackType() {
        return RenderStub.InputFeedbackType.NONE;
    }

    @Override
    public @RenderStub.InputFeedbackType int getLongPressFeedbackType() {
        return RenderStub.InputFeedbackType.LONG_TOUCH_ANIMATION;
    }

    @Override
    public boolean isIndirectInputMode() {
        return false;
    }

    private void injectQueuedEvents() {
        while (!mQueuedEvents.isEmpty()) {
            MotionEvent event = mQueuedEvents.remove();
            mInjector.sendTouchEvent(event);
            event.recycle();
        }
    }

    private void clearQueuedEvents() {
        while (!mQueuedEvents.isEmpty()) {
            mQueuedEvents.remove().recycle();
        }
    }

    // NOTE: MotionEvents generated from this method should be recycled.
    private MotionEvent transformToRemoteCoordinates(MotionEvent event) {
        // Use a copy of the original event so the original event can be passed to other
        // detectors/handlers in an unmodified state.
        event = MotionEvent.obtain(event);

        // Transform the event coordinates so they represent the remote screen coordinates
        // instead of the local touch display.
        Matrix inverted = new Matrix();
        mRenderData.transform.invert(inverted);
        event.transform(inverted);

        return event;
    }

    private void resetStateData() {
        clearQueuedEvents();
        mInRemoteGesture = false;
        mIgnoreTouchEvents = false;
    }
}
