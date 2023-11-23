// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.view.View;

import org.chromium.content_public.browser.MotionEventSynthesizer;

/** Injects synthetic touch events. All the coordinates are of physical unit. */
public class MotionEventSynthesizerImpl implements MotionEventSynthesizer {
    private static final int MAX_NUM_POINTERS = 16;

    private final PointerProperties[] mPointerProperties;
    private final PointerCoords[] mPointerCoords;
    private final View mTarget;
    private long mDownTimeInMs;

    public static MotionEventSynthesizerImpl create(View target) {
        return new MotionEventSynthesizerImpl(target);
    }

    private MotionEventSynthesizerImpl(View target) {
        assert target != null;
        mTarget = target;
        mPointerProperties = new PointerProperties[MAX_NUM_POINTERS];
        mPointerCoords = new PointerCoords[MAX_NUM_POINTERS];
    }

    /**
     * Sets the coordinate of the point at which a touch event takes place.
     *
     * @param index Index of the point when there are multiple points.
     * @param x X coordinate of the point.
     * @param x Y coordinate of the point.
     * @param id Id property of the point.
     * @param toolType ToolType property of the point.
     */
    @Override
    public void setPointer(int index, float x, float y, int id, int toolType) {
        assert (0 <= index && index < MAX_NUM_POINTERS);

        PointerCoords coords = new PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.pressure = 1.0f;
        mPointerCoords[index] = coords;

        PointerProperties properties = new PointerProperties();
        properties.id = id;
        properties.toolType = toolType;
        mPointerProperties[index] = properties;
    }

    /**
     * Sets the coordinate of the point at which a touch event takes place.
     *
     * @param index Index of the point when there are multiple points.
     * @param x X coordinate of the point.
     * @param x Y coordinate of the point.
     * @param id Id property of the point.
     */
    public void setPointer(int index, float x, float y, int id) {
        setPointer(index, x, y, id, MotionEvent.TOOL_TYPE_UNKNOWN);
    }

    /**
     * Sets the scroll delta against the origin point of a touch event.
     *
     * @param x X coordinate of the point.
     * @param x Y coordinate of the point.
     * @param dx Delta along the X coordinate.
     * @param dy Delta along the Y coordinate.
     */
    public void setScrollDeltas(float x, float y, float dx, float dy) {
        setPointer(0, x, y, 0);
        mPointerCoords[0].setAxisValue(MotionEvent.AXIS_HSCROLL, dx);
        mPointerCoords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, dy);
    }

    /**
     * Injects a synthetic action with the preset points and delta.
     *
     * @param action Type of the action to inject.
     * @param pointerCount The number of points associated with the event.
     * @param pointerIndex The index of the event to send. In the case of
     *        START and END, eg, we send a separate event as each pointer starts
     *        or ends, respectively.
     * @param timeInMs Timestamp for the event.
     */
    @Override
    public void inject(int action, int pointerCount, int pointerIndex, long timeInMs) {
        switch (action) {
            case MotionEventAction.START:
                {
                    // We currently only handle two fingers.
                    assert pointerIndex == 0 || pointerIndex == 1;
                    assert pointerCount <= 2;
                    int action_bitfield =
                            pointerIndex == 0
                                    ? MotionEvent.ACTION_DOWN
                                    : MotionEvent.ACTION_POINTER_DOWN
                                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
                    mDownTimeInMs = timeInMs;
                    MotionEvent event =
                            MotionEvent.obtain(
                                    mDownTimeInMs,
                                    timeInMs,
                                    action_bitfield,
                                    pointerIndex + 1,
                                    mPointerProperties,
                                    mPointerCoords,
                                    0,
                                    0,
                                    1,
                                    1,
                                    0,
                                    0,
                                    0,
                                    0);
                    mTarget.dispatchTouchEvent(event);
                    event.recycle();
                    break;
                }
            case MotionEventAction.MOVE:
                {
                    MotionEvent event =
                            MotionEvent.obtain(
                                    mDownTimeInMs,
                                    timeInMs,
                                    MotionEvent.ACTION_MOVE,
                                    pointerCount,
                                    mPointerProperties,
                                    mPointerCoords,
                                    0,
                                    0,
                                    1,
                                    1,
                                    0,
                                    0,
                                    0,
                                    0);
                    mTarget.dispatchTouchEvent(event);
                    event.recycle();
                    break;
                }
            case MotionEventAction.CANCEL:
                {
                    MotionEvent event =
                            MotionEvent.obtain(
                                    mDownTimeInMs,
                                    timeInMs,
                                    MotionEvent.ACTION_CANCEL,
                                    1,
                                    mPointerProperties,
                                    mPointerCoords,
                                    0,
                                    0,
                                    1,
                                    1,
                                    0,
                                    0,
                                    0,
                                    0);
                    mTarget.dispatchTouchEvent(event);
                    event.recycle();
                    break;
                }
            case MotionEventAction.END:
                {
                    // We currently only handle two fingers.
                    assert pointerIndex == 0 || pointerIndex == 1;
                    assert pointerCount <= 2;
                    int action_bitfield =
                            pointerIndex == 0
                                    ? MotionEvent.ACTION_UP
                                    : MotionEvent.ACTION_POINTER_UP
                                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
                    MotionEvent event =
                            MotionEvent.obtain(
                                    mDownTimeInMs,
                                    timeInMs,
                                    action_bitfield,
                                    pointerIndex + 1,
                                    mPointerProperties,
                                    mPointerCoords,
                                    0,
                                    0,
                                    1,
                                    1,
                                    0,
                                    0,
                                    0,
                                    0);
                    mTarget.dispatchTouchEvent(event);
                    event.recycle();
                    break;
                }
            case MotionEventAction.SCROLL:
                {
                    assert pointerCount == 1;
                    MotionEvent event =
                            MotionEvent.obtain(
                                    mDownTimeInMs,
                                    timeInMs,
                                    MotionEvent.ACTION_SCROLL,
                                    pointerCount,
                                    mPointerProperties,
                                    mPointerCoords,
                                    0,
                                    0,
                                    1,
                                    1,
                                    0,
                                    0,
                                    InputDevice.SOURCE_CLASS_POINTER,
                                    0);
                    mTarget.dispatchGenericMotionEvent(event);
                    event.recycle();
                    break;
                }
            case MotionEventAction.HOVER_ENTER:
            case MotionEventAction.HOVER_EXIT:
            case MotionEventAction.HOVER_MOVE:
                {
                    injectHover(action, pointerCount, timeInMs);
                    break;
                }
            default:
                {
                    assert false : "Unreached";
                    break;
                }
        }
    }

    private void injectHover(int action, int pointerCount, long timeInMs) {
        assert pointerCount == 1;
        int androidAction = MotionEvent.ACTION_HOVER_ENTER;
        if (MotionEventAction.HOVER_EXIT == action) androidAction = MotionEvent.ACTION_HOVER_EXIT;
        if (MotionEventAction.HOVER_MOVE == action) androidAction = MotionEvent.ACTION_HOVER_MOVE;
        MotionEvent event =
                MotionEvent.obtain(
                        mDownTimeInMs,
                        timeInMs,
                        androidAction,
                        pointerCount,
                        mPointerProperties,
                        mPointerCoords,
                        0,
                        0,
                        1,
                        1,
                        0,
                        0,
                        InputDevice.SOURCE_CLASS_POINTER,
                        0);
        mTarget.dispatchGenericMotionEvent(event);
        event.recycle();
    }
}
