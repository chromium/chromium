// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.MotionEvent;

/**
 * Utility for creating MotionEvents for multi-touch event simulation. The events are created by
 * calling MotionEvent.obtain(...) with suitable parameters.
 */
public class TouchEventGenerator {
    /**
     * Stores the current set of held-down fingers. This is a sparse array since the indices of the
     * fingers are not contiguous. For example, if fingers 0, 1, 2 are pressed then finger 1 is
     * lifted, the device ids 0, 2 would correspond to the indexes 0, 1.
     */
    private SparseArray<PointF> mFingerPositions = new SparseArray<PointF>();

    /**
     * The event's DownTime. This is set to the system's current time when the first finger is
     * pressed.
     */
    private long mDownTime;

    /**
     * Creates a fake event with the given action and device id. The event is generated using the
     * information in |mFingerPositions|.
     *
     * @param actionMasked The (masked) action to generate, for example, ACTION_POINTER_DOWN.
     * @param id The device id of the event. There must already be an entry for |id| in
     *        |mFingerPositions|, so that |id| can be converted to an index and combined with
     *        |actionMasked| to set the new event's |action| property.
     */
    private MotionEvent obtainEvent(int actionMasked, int id) {
        int actionIndex = mFingerPositions.indexOfKey(id);
        assert actionIndex >= 0;
        int action = (actionIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT) | actionMasked;

        long eventTime = SystemClock.uptimeMillis();
        int size = mFingerPositions.size();

        // Generate the arrays of pointers and positions for the event.
        MotionEvent.PointerProperties[] pointers = new MotionEvent.PointerProperties[size];
        MotionEvent.PointerCoords[] positions = new MotionEvent.PointerCoords[size];
        for (int i = 0; i < size; i++) {
            int key = mFingerPositions.keyAt(i);
            PointF position = mFingerPositions.valueAt(i);

            pointers[i] = new MotionEvent.PointerProperties();
            pointers[i].id = key;

            positions[i] = new MotionEvent.PointerCoords();
            positions[i].x = position.x;
            positions[i].y = position.y;
        }

        return MotionEvent.obtain(mDownTime, eventTime, action, size, pointers, positions,
                0, 0, 1, 1, id, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
    }

    /**
     * Obtains a finger-down event.
     *
     * @param id The device id of the new finger that is pressed. The caller must ensure this is
     *        a finger not currently held down.
     * @param x The x-coordinate of the new finger position.
     * @param y The y-coordinate of the new finger position.
     */
    public MotionEvent obtainDownEvent(int id, float x, float y) {
        assert mFingerPositions.get(id) == null;
        mFingerPositions.put(id, new PointF(x, y));
        int actionMasked;
        if (mFingerPositions.size() == 1) {
            mDownTime = SystemClock.uptimeMillis();
            actionMasked = MotionEvent.ACTION_DOWN;
        } else {
            actionMasked = MotionEvent.ACTION_POINTER_DOWN;
        }
        return obtainEvent(actionMasked, id);
    }

    /**
     * Obtains a finger-up event.
     *
     * @param id The device id of the finger to be lifted. The caller must ensure this is a
     *        finger previously held down.
     */
    public MotionEvent obtainUpEvent(int id) {
        assert mFingerPositions.get(id) != null;

        int actionMasked = mFingerPositions.size() == 1
                ? MotionEvent.ACTION_UP : MotionEvent.ACTION_POINTER_UP;

        MotionEvent event = obtainEvent(actionMasked, id);
        mFingerPositions.remove(id);
        return event;
    }

    /**
     * Obtains a finger-move event. This moves a single finger, keeping any others in the same
     * same position.
     *
     * @param id The device id of the finger being moved. The caller must ensure this is a
     *        finger previously held down.
     * @param x The x-coordinate of the new finger position.
     * @param y The y-coordinate of the new finger position.
     */
    public MotionEvent obtainMoveEvent(int id, float x, float y) {
        PointF position = mFingerPositions.get(id);
        assert position != null;
        position.x = x;
        position.y = y;
        return obtainEvent(MotionEvent.ACTION_MOVE, id);
    }
}
