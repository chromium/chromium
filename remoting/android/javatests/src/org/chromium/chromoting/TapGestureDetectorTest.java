// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

/** Tests for {@link TapGestureDetector}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TapGestureDetectorTest {
    private static class MockListener implements TapGestureDetector.OnTapListener {
        private static final float COMPARISON_DELTA = 0.01f;
        int mTapCount = -1;
        int mLongPressCount = -1;
        float mTapX = -1;
        float mTapY = -1;

        @Override
        public boolean onTap(int pointerCount, float x, float y) {
            Assert.assertEquals(-1, mTapCount);
            Assert.assertEquals(-1, mTapX, COMPARISON_DELTA);
            Assert.assertEquals(-1, mTapY, COMPARISON_DELTA);
            mTapCount = pointerCount;
            mTapX = x;
            mTapY = y;
            return true;
        }

        @Override
        public void onLongPress(int pointerCount, float x, float y) {
            Assert.assertEquals(-1, mLongPressCount);
            Assert.assertEquals(-1, mTapX, COMPARISON_DELTA);
            Assert.assertEquals(-1, mTapY, COMPARISON_DELTA);
            mLongPressCount = pointerCount;
            mTapX = x;
            mTapY = y;
        }

        public void assertTapDetected(int expectedCount, float expectedX, float expectedY) {
            Assert.assertEquals(expectedCount, mTapCount);
            Assert.assertEquals(expectedX, mTapX, COMPARISON_DELTA);
            Assert.assertEquals(expectedY, mTapY, COMPARISON_DELTA);
            Assert.assertEquals(-1, mLongPressCount);
        }

        public void assertLongPressDetected(int expectedCount, float expectedX, float expectedY) {
            Assert.assertEquals(expectedCount, mLongPressCount);
            Assert.assertEquals(expectedX, mTapX, COMPARISON_DELTA);
            Assert.assertEquals(expectedY, mTapY, COMPARISON_DELTA);
            Assert.assertEquals(-1, mTapCount);
        }

        public void assertNothingDetected() {
            Assert.assertEquals(-1, mTapCount);
            Assert.assertEquals(-1, mLongPressCount);
            Assert.assertEquals(-1, mTapX, COMPARISON_DELTA);
            Assert.assertEquals(-1, mTapY, COMPARISON_DELTA);
        }
    }

    private TapGestureDetector mDetector;
    private MockListener mListener;
    private TouchEventGenerator mEventGenerator;

    /** Injects movement of a single finger (keeping other fingers in place). */
    private void injectMoveEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainMoveEvent(id, x, y);
        mDetector.onTouchEvent(event);
        event.recycle();
    }

    /** Injects a finger-down event (keeping other fingers in place). */
    private void injectDownEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainDownEvent(id, x, y);
        mDetector.onTouchEvent(event);
        event.recycle();
    }

    /** Injects a finger-up event (keeping other fingers in place). */
    private void injectUpEvent(int id) {
        MotionEvent event = mEventGenerator.obtainUpEvent(id);
        mDetector.onTouchEvent(event);
        event.recycle();
    }

    @Before
    public void setUp() {
        mListener = new MockListener();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDetector = new TapGestureDetector(
                        InstrumentationRegistry.getTargetContext(), mListener);
            }
        });
        mEventGenerator = new TouchEventGenerator();
    }

    /** Verifies that a simple down/up is detected as a tap. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerDownUp() {
        injectDownEvent(0, 0, 0);
        injectUpEvent(0);
        mListener.assertTapDetected(1, 0, 0);
    }

    /** Verifies that a simple multi-finger down/up is detected as a tap. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testMultipleFingerDownUp() {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectUpEvent(0);
        injectUpEvent(1);
        injectUpEvent(2);
        mListener.assertTapDetected(3, 0, 0);
    }

    /** Verifies that a multi-finger tap is detected when lifting the fingers in reverse order. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testMultipleFingerDownUpReversed() {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectUpEvent(2);
        injectUpEvent(1);
        injectUpEvent(0);
        mListener.assertTapDetected(3, 0, 0);
    }

    /** Verifies that small movement of multiple fingers is still detected as a tap. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testMultipleFingerSmallMovements() {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectMoveEvent(0, 1, 1);
        injectMoveEvent(1, 101, 101);
        injectMoveEvent(2, 202, 202);
        injectUpEvent(0);
        injectUpEvent(1);
        injectUpEvent(2);
        mListener.assertTapDetected(3, 0, 0);
    }

    /** Verifies that large motion of a finger prevents a tap being detected. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testLargeMotion() {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectMoveEvent(1, 300, 300);
        injectUpEvent(0);
        injectUpEvent(1);
        injectUpEvent(2);
        mListener.assertNothingDetected();
    }

    /** Verifies that a long-press is detected. */
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testLongPress() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure the gesture-detector is created on the UI thread, so that it uses the
                // Handler for the UI thread for LongPress notifications.
                mDetector = new TapGestureDetector(
                        InstrumentationRegistry.getTargetContext(), mListener);

                injectDownEvent(0, 0, 0);
            }
        });

        Thread.sleep(2 * ViewConfiguration.getLongPressTimeout());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                injectUpEvent(0);
            }
        });

        mListener.assertLongPressDetected(1, 0, 0);
    }
}
