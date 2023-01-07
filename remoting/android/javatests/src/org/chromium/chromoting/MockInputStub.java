// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;

import org.junit.Assert;

import org.chromium.chromoting.jni.TouchEventData;

import java.util.Arrays;
import java.util.LinkedList;

/**
 * A mock implementation of {@link InputInjector} for testing purpose.
 */
public final class MockInputStub extends Assert implements InputStub {
    /** The base class to store an event, which represents an user activity. */
    public abstract static class Event<T> {
        public void assertEventEquals(T other) {
            if (other == this) {
                return;
            }
            if (other == null) {
                fail();
            }

            assertContentMatch(other);
        }

        protected abstract void assertContentMatch(T other);
    }

    /** A mouse event. */
    public static final class MouseEvent extends Event<MouseEvent> {
        private final int mX;
        private final int mY;
        private final int mButton;
        private final boolean mDown;

        public MouseEvent(int x, int y, int button, boolean down) {
            mX = x;
            mY = y;
            mButton = button;
            mDown = down;
        }

        @Override
        protected void assertContentMatch(MouseEvent other) {
            assertEquals(mX, other.mX);
            assertEquals(mY, other.mY);
            assertEquals(mButton, other.mButton);
            assertEquals(mDown, other.mDown);
        }
    }

    /** A mouse wheel event. */
    public static final class WheelEvent extends Event<WheelEvent> {
        private final int mDeltaX;
        private final int mDeltaY;

        public WheelEvent(int deltaX, int deltaY) {
            mDeltaX = deltaX;
            mDeltaY = deltaY;
        }

        @Override
        protected void assertContentMatch(WheelEvent other) {
            assertEquals(mDeltaX, other.mDeltaX);
            assertEquals(mDeltaY, other.mDeltaY);
        }
    }

    /** A keyboard event. */
    public static final class KeyEvent extends Event<KeyEvent> {
        private final int mScanCode;
        private final int mKeyCode;
        private final boolean mKeyDown;

        public KeyEvent(int scanCode, int keyCode, boolean keyDown) {
            mScanCode = scanCode;
            mKeyCode = keyCode;
            mKeyDown = keyDown;
        }

        @Override
        protected void assertContentMatch(KeyEvent other) {
            assertEquals(mScanCode, other.mScanCode);
            assertEquals(mKeyCode, other.mKeyCode);
            assertEquals(mKeyDown, other.mKeyDown);
        }
    }

    /** A text event. */
    public static final class TextEvent extends Event<TextEvent> {
        public final String mText;

        public TextEvent(String text) {
            mText = text;
        }

        @Override
        protected void assertContentMatch(TextEvent other) {
            if (mText == null && other.mText == null) {
                return;
            }
            if (mText == null || other.mText == null) {
                fail();
            }
            assertEquals(mText, other.mText);
        }
    }

    /** A touch event. */
    public static final class TouchEvent extends Event<TouchEvent> {
        /**
         * A random number to represent an invalid touch id for {@link assertContentMatch}.
         * Consumers can use this number to ignore the comparison of
         * {@link TouchEventData#getTouchPointId()} when comparing.
         */
        public static final int INVALID_ID = -883;

        /**
         * A random number to represent an invalid position for {@link #assertContentMatch}.
         * Consumers can use this number to ignore certain float fields in an {@link TouchEventData}
         * when comparing.
         */
        public static final float INVALID_POSITION = -763.273f;

        /**
         * A number to represent an invalid angle in radians for {@link #assertContentMatch}.
         * Consumers can use this number to ignore the comparison of
         * {@link TouchEventData#getTouchPointAngle} when comparing.
         */
        public static final float INVALID_RADIANS = 6.289f * (float) Math.PI;

        // TouchEventData stores degrees instead of radians
        private static final float INVALID_DEGREES = (float) Math.toDegrees(INVALID_RADIANS);
        private final @TouchEventData.EventType int mEventType;
        private final TouchEventData[] mData;

        public TouchEvent(@TouchEventData.EventType int eventType, TouchEventData[] data) {
            mEventType = eventType;
            mData = (data == null ? null : Arrays.copyOf(data, data.length));
        }

        private static boolean idMatch(int left, int right) {
            return left == right || left == INVALID_ID || right == INVALID_ID;
        }

        private static boolean positionMatch(float left, float right) {
            return left == right || left == INVALID_POSITION || right == INVALID_POSITION;
        }

        private static boolean degreeMatch(float left, float right) {
            return left == right || left == INVALID_DEGREES || right == INVALID_DEGREES;
        }

        private static void assertContentMatch(TouchEventData left, TouchEventData right) {
            assertTrue(idMatch(left.getTouchPointId(), right.getTouchPointId()));
            assertTrue(positionMatch(left.getTouchPointX(), right.getTouchPointX()));
            assertTrue(positionMatch(left.getTouchPointY(), right.getTouchPointY()));
            assertTrue(positionMatch(left.getTouchPointRadiusX(), right.getTouchPointRadiusX()));
            assertTrue(positionMatch(left.getTouchPointRadiusY(), right.getTouchPointRadiusY()));
            assertTrue(degreeMatch(left.getTouchPointAngle(), right.getTouchPointAngle()));
            assertTrue(positionMatch(left.getTouchPointPressure(), right.getTouchPointPressure()));
        }

        private static void assertDataEquals(TouchEventData left, TouchEventData right) {
            if (left == null && right == null) {
                return;
            }
            if (left == null || right == null) {
                fail();
            }
            assertContentMatch(left, right);
        }

        private int dataSize() {
            return mData == null ? 0 : mData.length;
        }

        @Override
        protected void assertContentMatch(TouchEvent other) {
            assertEquals(mEventType, other.mEventType);
            // An event with an empty mData array will match an event with a non-empty mData array.
            // This is useful for tests which expect a TouchEvent with a certain EventType, but
            // don't care about the content of mData.
            if (dataSize() != 0 && other.dataSize() != 0) {
                assertEquals(dataSize(), other.dataSize());
                for (int i = 0; i < dataSize(); i++) {
                    assertDataEquals(mData[i], other.mData[i]);
                }
            }
        }
    }

    private final LinkedList<MouseEvent> mMouseEvents;
    private final LinkedList<WheelEvent> mWheelEvents;
    private final LinkedList<KeyEvent> mKeyEvents;
    private final LinkedList<TextEvent> mTextEvents;
    private final LinkedList<TouchEvent> mTouchEvents;

    public MockInputStub() {
        mMouseEvents = new LinkedList<>();
        mWheelEvents = new LinkedList<>();
        mKeyEvents = new LinkedList<>();
        mTextEvents = new LinkedList<>();
        mTouchEvents = new LinkedList<>();
    }

    /**
     * Compares the first |right|.length events with the ones in |left|, asserts they are equal, and
     * consumes these events from |left|.
     */
    private static <E extends Event<E>> void assertContains(
            LinkedList<E> left, E[] right) {
        if (left == null && right == null) {
            return;
        }
        if (left == null || right == null) {
            fail();
        }
        assertTrue("left.size() " + left.size() + " != right.length " + right.length,
                left.size() >= right.length);
        for (int i = 0; i < right.length; i++) {
            E leftEvent = left.removeFirst();
            if (leftEvent == null) {
                assertNull(right[i]);
            } else {
                leftEvent.assertEventEquals(right[i]);
            }
        }
    }

    public void clear() {
        mMouseEvents.clear();
        mWheelEvents.clear();
        mKeyEvents.clear();
        mTextEvents.clear();
        mTouchEvents.clear();
    }

    /**
     * Compares the first |other|.length events with {@link #mMouseEvents} in this instance,
     * asserts they are equal, and consumes these events in this instance.
     */
    public void assertContainsMouseEvents(MouseEvent[] other) {
        assertContains(mMouseEvents, other);
    }

    /**
     * Compares the first |other|.length events with {@link #mTouchEvents} in this instance,
     * asserts they are equal, and consumes these events in this instance.
     */
    public void assertContainsTouchEvents(TouchEvent[] other) {
        assertContains(mTouchEvents, other);
    }

    /** Asserts current instance is empty. */
    public void assertEmpty() {
        assertTrue(mMouseEvents.isEmpty());
        assertTrue(mWheelEvents.isEmpty());
        assertTrue(mKeyEvents.isEmpty());
        assertTrue(mTextEvents.isEmpty());
        assertTrue(mTouchEvents.isEmpty());
    }

    /**
     * Checks whether the first two events in {@link #mTouchEvents} are representing a down and
     * an up action at specified position |x|, |y|, and consumes these events.
     */
    public void assertTapInjected(float x, float y) {
        assertTouchEventInjected(TouchEventData.EventType.START, x, y);
        assertTouchEventInjected(TouchEventData.EventType.END, x, y);
    }

    /**
     * Checks whether the first event in {@link #mTouchEvents} is representing an event with type
     * |eventType| and at specified position |x|, |y|, and consumes this event.
     */
    public void assertTouchEventInjected(
            @TouchEventData.EventType int eventType, float x, float y) {
        assertContainsTouchEvents(new TouchEvent[] {
                new TouchEventBuilder()
                        .withEventType(eventType)
                        .withX(x)
                        .withY(y)
                        .append()
                        .build(),
        });
    }

    /**
     * Checks whether the first event in {@link #mTouchEvents} is representing an event with type
     * |eventType|, and consumes this event.
     */
    public void assertTouchEventInjected(@TouchEventData.EventType int eventType) {
        assertContainsTouchEvents(new TouchEvent[] {
                new TouchEventBuilder().withEventType(eventType).build(),
        });
    }

    /**
     * Checks whether the first two events in {@link #mMouseEvents} are representing a down and an
     * up action with |button| at specified position |x|, |y|, and consumes these events.
     */
    public void assertClickInjected(int button, int x, int y) {
        assertContainsMouseEvents(new MouseEvent[] {
                new MouseEvent(x, y, button, true), new MouseEvent(x, y, button, false),
        });
    }

    public void assertTouchMoveEventInjected(
            PointF[] initPositions, int stepX, int stepY, int moveCount) {
        LinkedList<TouchEvent> events = new LinkedList<>();
        assertTrue(initPositions != null && initPositions.length > 0);
        for (int i = 0; i < initPositions.length; i++) {
            assertNotNull(initPositions[i]);
            events.add(new TouchEventBuilder()
                               .withEventType(TouchEventData.EventType.START)
                               .withX(initPositions[i].x)
                               .withY(initPositions[i].y)
                               .append()
                               .build());
        }

        // These tests send a single event for each finger that is moved.  E.g. if we are injecting
        // a pan event with two fingers, then every two TouchEvents represents one 'frame' of the
        // motion sequence.  Here we determine which iteration this event represents so we can use
        // that to accurately compare the locations with the expected values.
        for (int i = 0; i < moveCount; i++) {
            for (int j = 0; j < initPositions.length; j++) {
                TouchEventBuilder builder = new TouchEventBuilder();
                builder.withEventType(TouchEventData.EventType.MOVE);
                for (int k = 0; k < initPositions.length; k++) {
                    // We inject one finger at a time which means that finger will have the correct
                    // value but other fingers may still be stepSize behind it.
                    int currentStep = i - 1;
                    if (j >= k) {
                        currentStep++;
                    }
                    if (currentStep < 0) {
                        currentStep = 0;
                    }
                    builder.withX(initPositions[k].x + currentStep * stepX)
                            .withY(initPositions[k].y + currentStep * stepY)
                            .append();
                }
                events.add(builder.build());
            }
        }

        assertContainsTouchEvents(events.toArray(new TouchEvent[0]));
    }

    /**
     * Checks whether the first two events in {@link #mMouseEvents} are representing a down and an
     * up action with right button at specified position |x|, |y|, and consumes these events.
     */
    public void assertRightClickInjected(int x, int y) {
        assertClickInjected(BUTTON_RIGHT, x, y);
    }

    // ---------------- Implementations of InputInjector ----------------------
    @Override
    public void sendMouseEvent(int x, int y, int whichButton, boolean buttonDown) {
        mMouseEvents.add(new MouseEvent(x, y, whichButton, buttonDown));
    }

    @Override
    public void sendMouseWheelEvent(int deltaX, int deltaY) {
        mWheelEvents.add(new WheelEvent(deltaX, deltaY));
    }

    @Override
    public boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown) {
        mKeyEvents.add(new KeyEvent(scanCode, keyCode, keyDown));

        // Note: This implementation is not consistent with jni.Client, which may return false when
        // scanCode and keyCode cannot be mapped to a usb key code.
        return true;
    }

    @Override
    public void sendTextEvent(String text) {
        mTextEvents.add(new TextEvent(text));
    }

    @Override
    public void sendTouchEvent(@TouchEventData.EventType int eventType, TouchEventData[] data) {
        assertNotNull(data);
        assertTrue(data.length != 0);
        for (int i = 0; i < data.length; i++) {
            assertTrue(data[i].getTouchPointId() != TouchEvent.INVALID_ID);
            assertTrue(data[i].getTouchPointX() != TouchEvent.INVALID_POSITION);
            assertTrue(data[i].getTouchPointY() != TouchEvent.INVALID_POSITION);
            assertTrue(data[i].getTouchPointRadiusX() != TouchEvent.INVALID_POSITION);
            assertTrue(data[i].getTouchPointRadiusY() != TouchEvent.INVALID_POSITION);
            assertTrue(data[i].getTouchPointAngle() != TouchEvent.INVALID_DEGREES);
            assertTrue(data[i].getTouchPointPressure() != TouchEvent.INVALID_POSITION);
        }
        mTouchEvents.add(new TouchEvent(eventType, data));
    }
}
