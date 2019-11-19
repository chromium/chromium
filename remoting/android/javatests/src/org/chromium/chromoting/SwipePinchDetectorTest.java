// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

/** Tests for {@link SwipePinchDetector}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class SwipePinchDetectorTest {
    private SwipePinchDetector mDetector;
    private float mTouchSlop;
    private MotionEvent.PointerProperties[] mPointers;

    // Stores the current finger positions, for convenience in writing tests. These values are
    // initialized during setUp().
    private MotionEvent.PointerCoords[] mCurrentPositions;

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getTargetContext();
        mDetector = new SwipePinchDetector(context);
        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        MotionEvent.PointerProperties pointer0 = new MotionEvent.PointerProperties();
        pointer0.id = 0;
        MotionEvent.PointerProperties pointer1 = new MotionEvent.PointerProperties();
        pointer1.id = 1;
        mPointers = new MotionEvent.PointerProperties[] {pointer0, pointer1};

        MotionEvent.PointerCoords position0 = new MotionEvent.PointerCoords();
        MotionEvent.PointerCoords position1 = new MotionEvent.PointerCoords();
        mCurrentPositions = new MotionEvent.PointerCoords[] {position0, position1};

        // The starting points are arbitrary, but non-zero to ensure that the tests detect relative,
        // not absolute, motion.
        mCurrentPositions[0].x = 100;
        mCurrentPositions[0].y = 200;
        mCurrentPositions[1].x = 300;
        mCurrentPositions[1].y = 400;
    }

    /**
     * Simulates a 2-finger event. The action parameter should be MotionEvent.ACTION_POINTER_DOWN,
     * MotionEvent.ACTION_MOVE or MotionEvent.ACTION_POINTER_UP.
     */
    private void injectEvent(int action) {
        final long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(eventTime, eventTime, action, 2, mPointers,
                mCurrentPositions, 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        mDetector.onTouchEvent(event);
    }

    /** Verifies that a simple swipe gesture is recognized as a swipe. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testSwipeRecognition() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertFalse(mDetector.isPinching());

        // Any distance greater than the touch-slop threshold should work.
        mCurrentPositions[0].y += mTouchSlop * 2;
        mCurrentPositions[1].y += mTouchSlop * 2;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertTrue(mDetector.isSwiping());
        Assert.assertFalse(mDetector.isPinching());
    }

    /** Verifies that a simple pinch gesture is recognized. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testPinchRecognition() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertFalse(mDetector.isPinching());

        // Any distance greater than the touch-slop threshold should work.
        mCurrentPositions[0].x -= mTouchSlop * 2;
        mCurrentPositions[1].x += mTouchSlop * 2;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertTrue(mDetector.isPinching());
    }

    /** Verifies that motion less than touch-slop does not trigger anything. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testNoMotion() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        mCurrentPositions[0].x += mTouchSlop / 2;
        mCurrentPositions[0].y += mTouchSlop / 2;
        mCurrentPositions[1].x -= mTouchSlop / 2;
        mCurrentPositions[1].y -= mTouchSlop / 2;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertFalse(mDetector.isPinching());
    }

    /** Verifies that a pinch with one finger stationary is detected. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerStationary() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);

        // The triggering threshold in this case (one finger stationary) is mTouchSlop * 2;
        mCurrentPositions[1].x += mTouchSlop * 3;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertTrue(mDetector.isPinching());

        // Do the same test for the other finger.
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        mCurrentPositions[0].x += mTouchSlop * 3;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertTrue(mDetector.isPinching());
    }

    /**
     * Verifies that a pinch is recognized, when the fingers cross the motion threshold at
     * different times.
     */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testUnevenPinch() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        for (int i = 0; i < 50; i++) {
            mCurrentPositions[0].x -= 2;
            mCurrentPositions[1].x += 3;
            injectEvent(MotionEvent.ACTION_MOVE);
        }

        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertTrue(mDetector.isPinching());
    }

    /** Same test as testUnevenPinch() except the slow/fast fingers are reversed. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testUnevenPinch2() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        for (int i = 0; i < 50; i++) {
            mCurrentPositions[0].x -= 3;
            mCurrentPositions[1].x += 2;
            injectEvent(MotionEvent.ACTION_MOVE);
        }

        Assert.assertFalse(mDetector.isSwiping());
        Assert.assertTrue(mDetector.isPinching());
    }

    /** Verifies that a swipe is recognized, even if the fingers move at different rates. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testUnevenSwipe() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        for (int i = 0; i < 50; i++) {
            // The fingers need to move similarly enough so that one finger moves a distance of
            // 2 * mTouchSlop after the other finger moves a distance of mTouchSlop.
            // Otherwise the gesture would be mis-detected as a one-finger-stationary pinch.
            mCurrentPositions[0].y += 2;
            mCurrentPositions[1].y += 3;
            injectEvent(MotionEvent.ACTION_MOVE);
        }

        Assert.assertTrue(mDetector.isSwiping());
        Assert.assertFalse(mDetector.isPinching());
    }

    /** Same test as testUnevenSwipe() except the slow/fast fingers are reversed. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testUnevenSwipe2() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        for (int i = 0; i < 50; i++) {
            // The fingers need to move similarly enough so that one finger moves a distance of
            // 2 * mTouchSlop after the other finger moves a distance of mTouchSlop.
            // Otherwise the gesture would be mis-detected as a one-finger-stationary pinch.
            mCurrentPositions[0].y += 3;
            mCurrentPositions[1].y += 2;
            injectEvent(MotionEvent.ACTION_MOVE);
        }

        Assert.assertTrue(mDetector.isSwiping());
        Assert.assertFalse(mDetector.isPinching());
    }

    /** Verifies that the detector is reset when a gesture terminates or a new gesture begins. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testDetectorReset() {
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        mCurrentPositions[0].x += mTouchSlop * 3;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertTrue(mDetector.isPinching());

        // ACTION_POINTER_UP should terminate the gesture.
        injectEvent(MotionEvent.ACTION_POINTER_UP);
        Assert.assertFalse(mDetector.isPinching());

        // Repeat the same test, but use ACTION_POINTER_DOWN to start a new gesture, which should
        // terminate the current one.
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        mCurrentPositions[0].x += mTouchSlop * 3;
        injectEvent(MotionEvent.ACTION_MOVE);
        Assert.assertTrue(mDetector.isPinching());
        injectEvent(MotionEvent.ACTION_POINTER_DOWN);
        Assert.assertFalse(mDetector.isPinching());
    }
}
