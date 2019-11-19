// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;
import android.support.test.filters.SmallTest;
import android.view.MotionEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chromoting.jni.TouchEventData;

/** Tests for {@link TouchInputStrategy}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TouchInputStrategyTest {
    // Tests are run using a screen which is smaller than the size of the remote desktop and is
    // translated to the middle of the remote desktop area.  This allows us to verify that the
    // remote events which are 'injected' are correctly mapped and represent the remote coordinates.
    private static final int SCREEN_SIZE_PX = 100;
    private static final int REMOTE_DESKTOP_SIZE_PX = 300;
    private static final int TRANSLATE_OFFSET_PX = 100;

    private RenderData mRenderData;
    private TouchInputStrategy mInputStrategy;
    private MockInputStub mInputInjector;
    private TouchEventGenerator mEventGenerator;

    /** Injects movement of a single finger (keeping other fingers in place). */
    private void injectMoveEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainMoveEvent(id, x, y);
        mInputStrategy.onMotionEvent(event);
        event.recycle();
    }

    /** Injects a finger-down event (keeping other fingers in place). */
    private void injectDownEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainDownEvent(id, x, y);
        mInputStrategy.onMotionEvent(event);
        event.recycle();
    }

    /** Injects a finger-up event (keeping other fingers in place). */
    private void injectUpEvent(int id) {
        MotionEvent event = mEventGenerator.obtainUpEvent(id);
        mInputStrategy.onMotionEvent(event);
        event.recycle();
    }

    @Before
    public void setUp() {
        mRenderData = new RenderData();
        mInputInjector = new MockInputStub();

        mInputStrategy =
                new TouchInputStrategy(mRenderData, new InputEventSender(mInputInjector));
        mEventGenerator = new TouchEventGenerator();
        mRenderData.screenWidth = SCREEN_SIZE_PX;
        mRenderData.screenHeight = SCREEN_SIZE_PX;
        mRenderData.imageWidth = REMOTE_DESKTOP_SIZE_PX;
        mRenderData.imageHeight = REMOTE_DESKTOP_SIZE_PX;
        mRenderData.transform.postTranslate(-TRANSLATE_OFFSET_PX, -TRANSLATE_OFFSET_PX);
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOnTapWithNoEvents() {
        Assert.assertFalse(mInputStrategy.onTap(InputStub.BUTTON_LEFT));
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerTap() {
        injectDownEvent(0, 0, 0);
        injectUpEvent(0);
        mInputInjector.assertEmpty();

        Assert.assertTrue(mInputStrategy.onTap(InputStub.BUTTON_LEFT));

        mInputInjector.assertTapInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testLifoTwoFingerTap() {
        // Verify that the right click coordinates occur at the point of the first tap when the
        // initial finger is lifted up last.
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectUpEvent(1);
        injectUpEvent(0);
        mInputInjector.assertEmpty();

        Assert.assertTrue(mInputStrategy.onTap(InputStub.BUTTON_RIGHT));

        mInputInjector.assertRightClickInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testFifoTwoFingerTap() {
        // Verify that the right click coordinates occur at the point of the first tap when the
        // initial finger is lifted up first.
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectUpEvent(0);
        injectUpEvent(1);
        mInputInjector.assertEmpty();

        Assert.assertTrue(mInputStrategy.onTap(InputStub.BUTTON_RIGHT));

        mInputInjector.assertRightClickInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testThreeFingerTap() {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectDownEvent(2, 50, 50);
        injectUpEvent(2);
        injectUpEvent(1);
        injectUpEvent(0);
        mInputInjector.assertEmpty();

        Assert.assertFalse(mInputStrategy.onTap(InputStub.BUTTON_MIDDLE));
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerTapSequence() {
        int tapSequenceCount = 10;
        for (int i = 0; i < tapSequenceCount; i++) {
            injectDownEvent(0, i, i);
            injectUpEvent(0);
            mInputInjector.assertEmpty();

            Assert.assertTrue(mInputStrategy.onTap(InputStub.BUTTON_LEFT));

            int remoteOffsetPx = TRANSLATE_OFFSET_PX + i;
            mInputInjector.assertTapInjected(remoteOffsetPx, remoteOffsetPx);
        }
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testInvalidThenValidTap() {
        // First an invalid tap, verify it is ignored.
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectDownEvent(2, 50, 50);
        injectUpEvent(2);
        injectUpEvent(1);
        injectUpEvent(0);
        mInputInjector.assertEmpty();

        Assert.assertFalse(mInputStrategy.onTap(InputStub.BUTTON_MIDDLE));
        mInputInjector.assertEmpty();

        // Next a valid tap, verify it is handled.
        injectDownEvent(0, 0, 0);
        injectUpEvent(0);
        mInputInjector.assertEmpty();

        Assert.assertTrue(mInputStrategy.onTap(InputStub.BUTTON_LEFT));

        mInputInjector.assertTapInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOnPressAndHoldWithNoEvents() {
        Assert.assertFalse(mInputStrategy.onPressAndHold(InputStub.BUTTON_LEFT));
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerLongPress() {
        injectDownEvent(0, 0, 0);
        mInputInjector.assertEmpty();

        Assert.assertTrue(mInputStrategy.onPressAndHold(InputStub.BUTTON_LEFT));
        mInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.START, TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);

        injectUpEvent(0);
        mInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.END, TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerLongPressThenPan() {
        injectDownEvent(0, 0, 0);
        mInputInjector.assertEmpty();

        Assert.assertTrue(mInputStrategy.onPressAndHold(InputStub.BUTTON_LEFT));
        mInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.START, TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);

        final int panEventCount = 50;
        for (int i = 0; i <= panEventCount; i++) {
            injectMoveEvent(0, 0, i);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);
        }

        injectUpEvent(0);
        mInputInjector.assertTouchEventInjected(TouchEventData.EventType.END, TRANSLATE_OFFSET_PX,
                TRANSLATE_OFFSET_PX + panEventCount);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testTwoFingerLongPress() {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 1, 1);
        mInputInjector.assertEmpty();

        Assert.assertFalse(mInputStrategy.onPressAndHold(InputStub.BUTTON_RIGHT));
        mInputInjector.assertEmpty();

        injectUpEvent(0);
        injectUpEvent(1);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerPan() {
        injectDownEvent(0, 0, 0);

        // Inject a few move events to simulate a pan.
        injectMoveEvent(0, 1, 1);
        injectMoveEvent(0, 2, 2);
        injectMoveEvent(0, 3, 3);
        mInputInjector.assertEmpty();

        injectUpEvent(0);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testVerticalTwoFingerPan() {
        final int fingerOnePosX = 0;
        final int fingerTwoPosX = 10;
        injectDownEvent(0, fingerOnePosX, 0);
        injectDownEvent(1, fingerTwoPosX, 0);

        final int eventNum = 10;
        for (int i = 0; i < eventNum; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            injectMoveEvent(1, fingerTwoPosX, i);
        }
        mInputInjector.assertEmpty();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mInputInjector.assertTouchMoveEventInjected(
                new PointF[] {
                        new PointF(fingerOnePosX + TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX),
                        new PointF(fingerTwoPosX + TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX),
                },
                0, 1, eventNum);

        // Verify events are sent in realtime now.
        for (int i = eventNum; i < eventNum + 5; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);

            injectMoveEvent(1, fingerTwoPosX, i);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);
        }

        injectUpEvent(0);
        mInputInjector.assertTouchEventInjected(TouchEventData.EventType.END);

        injectUpEvent(1);
        mInputInjector.assertTouchEventInjected(TouchEventData.EventType.END);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testHorizontalTwoFingerPan() {
        final int fingerOnePosY = 0;
        final int fingerTwoPosY = 10;
        injectDownEvent(0, 0, fingerOnePosY);
        injectDownEvent(1, 0, fingerTwoPosY);

        final int eventNum = 10;
        for (int i = 0; i < eventNum; i++) {
            injectMoveEvent(0, i, fingerOnePosY);
            injectMoveEvent(1, i, fingerTwoPosY);
        }
        mInputInjector.assertEmpty();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mInputInjector.assertTouchMoveEventInjected(
                new PointF[] {
                        new PointF(TRANSLATE_OFFSET_PX, fingerOnePosY + TRANSLATE_OFFSET_PX),
                        new PointF(TRANSLATE_OFFSET_PX, fingerTwoPosY + TRANSLATE_OFFSET_PX),
                },
                1, 0, eventNum);

        // Verify events are sent in realtime now.
        for (int i = eventNum; i < eventNum + 5; i++) {
            injectMoveEvent(0, i, fingerOnePosY);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);

            injectMoveEvent(1, i, fingerTwoPosY);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);
        }

        injectUpEvent(0);
        mInputInjector.assertTouchEventInjected(TouchEventData.EventType.END);

        injectUpEvent(1);
        mInputInjector.assertTouchEventInjected(TouchEventData.EventType.END);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testCancelledTwoFingerPan() {
        final int fingerOnePosX = 0;
        final int fingerTwoPosX = 10;
        injectDownEvent(0, fingerOnePosX, 0);
        injectDownEvent(1, fingerTwoPosX, 0);

        final int eventNum = 10;
        for (int i = 0; i < eventNum; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            injectMoveEvent(1, fingerTwoPosX, i);
        }
        mInputInjector.assertEmpty();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mInputInjector.assertTouchMoveEventInjected(
                new PointF[] {
                        new PointF(fingerOnePosX + TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX),
                        new PointF(fingerTwoPosX + TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX),
                },
                0, 1, eventNum);

        // Verify events are sent in realtime now.
        for (int i = eventNum; i < eventNum + 5; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);

            injectMoveEvent(1, fingerTwoPosX, i);
            mInputInjector.assertTouchEventInjected(TouchEventData.EventType.MOVE);
        }

        // Once a third finger goes down, no more events should be sent.
        injectDownEvent(2, 0, 0);
        mInputInjector.assertEmpty();

        injectMoveEvent(0, 0, 0);
        injectMoveEvent(1, 0, 0);
        injectMoveEvent(2, 0, 0);
        mInputInjector.assertEmpty();

        injectUpEvent(2);
        mInputInjector.assertEmpty();

        injectMoveEvent(0, 5, 5);
        injectMoveEvent(1, 5, 5);
        mInputInjector.assertEmpty();
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testTooManyEventsCancelsGesture() {
        final int fingerOnePosX = 0;
        final int fingerTwoPosX = 10;
        injectDownEvent(0, fingerOnePosX, 0);
        injectDownEvent(1, fingerTwoPosX, 0);

        for (int i = 0; i < 10000; i++) {
            injectMoveEvent(0, fingerOnePosX, i % 10);
            injectMoveEvent(1, fingerTwoPosX, i % 10);
        }
        mInputInjector.assertEmpty();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mInputInjector.assertEmpty();
    }
}
