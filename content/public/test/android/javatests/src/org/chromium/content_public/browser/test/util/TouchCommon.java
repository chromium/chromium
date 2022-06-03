// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Touch-related functionality reused across test cases.
 *
 * Differs from {@link TestTouchUtils} as this does not rely on injecting events via
 * Instrumentation. Injecting events is more brittle (e.g. it will fail if a dialog pops up in front
 * of the test), and simulating the touch events in the manner used here is just as effective.
 */
public class TouchCommon {
    // Prevent instantiation.
    private TouchCommon() {}

    /**
     * Synchronously perform a start-to-end drag event on the specified view with deterministic
     * timing (events do not use system time).
     *
     * @param view The view to dispatch events to.
     * @param fromX X coordinate of the initial touch, in screen coordinates.
     * @param toX X coordinate of the drag destination, in screen coordinates.
     * @param fromY X coordinate of the initial touch, in screen coordinates.
     * @param toY Y coordinate of the drag destination, in screen coordinates.
     * @param stepCount How many move steps to include in the drag.
     * @param duration The amount of time that will be simulated for the event stream in ms.
     */
    public static void performDrag(View view, float fromX, float toX, float fromY, float toY,
            int stepCount, long duration) {
        // Use the current time as the base to add to.
        final long downTime = SystemClock.uptimeMillis();
        float[] windowXY = screenToWindowCoordinates(view, fromX, fromY);

        // Start by sending the down event.
        dispatchTouchEvent(view,
                MotionEvent.obtain(
                        downTime, downTime, MotionEvent.ACTION_DOWN, windowXY[0], windowXY[1], 0));

        float x = fromX;
        float y = fromY;
        float yStep = (toY - fromY) / stepCount;
        float xStep = (toX - fromX) / stepCount;
        long eventTime = downTime;

        // Follow with a stream of motion events to simulate the drag.
        for (int i = 0; i < stepCount; ++i) {
            y += yStep;
            x += xStep;
            eventTime += i * duration / stepCount;
            windowXY = screenToWindowCoordinates(view, x, y);
            dispatchTouchEvent(view,
                    MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_MOVE, windowXY[0],
                            windowXY[1], 0));
        }

        // Finally send the up event.
        windowXY = screenToWindowCoordinates(view, toX, toY);
        dispatchTouchEvent(view,
                MotionEvent.obtain(
                        downTime, eventTime, MotionEvent.ACTION_UP, windowXY[0], windowXY[1], 0));
    }

    /**
     * Synchronously perform a start-to-end drag event on the specified view with deterministic
     * timing (events do not use system time).
     *
     * @param activity The main activity to dispatch events to.
     * @param fromX X coordinate of the initial touch, in screen coordinates.
     * @param toX X coordinate of the drag destination, in screen coordinates.
     * @param fromY X coordinate of the initial touch, in screen coordinates.
     * @param toY Y coordinate of the drag destination, in screen coordinates.
     * @param stepCount How many move steps to include in the drag.
     * @param duration The amount of time that will be simulated for the event stream in ms.
     */
    public static void performDrag(Activity activity, float fromX, float toX, float fromY,
            float toY, int stepCount, long duration) {
        performDrag(getRootViewForActivity(activity), fromX, toX, fromY, toY, stepCount, duration);
    }

    /**
     * Starts (synchronously) a drag motion. Normally followed by dragTo() and dragEnd().
     *
     * @activity activity The activity where the touch action is being performed.
     * @param x X coordinate, in screen coordinates.
     * @param y Y coordinate, in screen coordinates.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    @Deprecated
    public static void dragStart(Activity activity, float x, float y, long downTime) {
        View view = getRootViewForActivity(activity);
        float[] windowXY = screenToWindowCoordinates(view, x, y);
        float windowX = windowXY[0];
        float windowY = windowXY[1];
        MotionEvent event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN, windowX, windowY, 0);
        dispatchTouchEvent(view, event);
    }

    /**
     * Drags / moves (synchronously) to the specified coordinates. Normally preceeded by
     * dragStart() and followed by dragEnd()
     *
     * @activity activity The activity where the touch action is being performed.
     * @param fromX X coordinate of the initial touch, in screen coordinates.
     * @param toX X coordinate of the drag destination, in screen coordinates.
     * @param fromY X coordinate of the initial touch, in screen coordinates.
     * @param toY Y coordinate of the drag destination, in screen coordinates.
     * @param stepCount How many move steps to include in the drag.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    @Deprecated
    public static void dragTo(Activity activity, float fromX, float toX, float fromY, float toY,
            int stepCount, long downTime) {
        View view = getRootViewForActivity(activity);
        float x = fromX;
        float y = fromY;
        float yStep = (toY - fromY) / stepCount;
        float xStep = (toX - fromX) / stepCount;
        for (int i = 0; i < stepCount; ++i) {
            y += yStep;
            x += xStep;
            long eventTime = SystemClock.uptimeMillis();
            float[] windowXY = screenToWindowCoordinates(view, x, y);
            float windowX = windowXY[0];
            float windowY = windowXY[1];
            MotionEvent event = MotionEvent.obtain(
                    downTime, eventTime, MotionEvent.ACTION_MOVE, windowX, windowY, 0);
            dispatchTouchEvent(view, event);
        }
    }

    /**
     * Finishes (synchronously) a drag / move at the specified coordinate.
     * Normally preceded by dragStart() and dragTo().
     *
     * @activity activity The activity where the touch action is being performed.
     * @param x X coordinate, in screen coordinates.
     * @param y Y coordinate, in screen coordinates.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    @Deprecated
    public static void dragEnd(Activity activity, float x, float y, long downTime) {
        View view = getRootViewForActivity(activity);
        float[] windowXY = screenToWindowCoordinates(view, x, y);
        float windowX = windowXY[0];
        float windowY = windowXY[1];
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent event =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP, windowX, windowY, 0);
        dispatchTouchEvent(view, event);
    }

    /**
     * Sends (synchronously) a single click to the View at the specified view-relative coordinates.
     *
     * @param v The view to be clicked.
     * @param x X coordinate, relative to v.
     * @param y Y coordinate, relative to v.
     */
    public static boolean singleClickView(View v, int x, int y) {
        return singleClickViewThroughTarget(v, v.getRootView(), x, y);
    }

    /**
     * Sends a click event to the specified view, not going through the root view.
     *
     * This is mostly useful for tests in VR, where inputs to the root view are (in a sense)
     * consumed by the platform, but the java test still wants to interact with, say, WebContents.
     *
     * @param view The view to be clicked.
     * @param target The view to inject the input into.
     * @param x X coordinate, relative to view.
     * @param y Y coordinate, relative to view.
     */
    /* package */ static boolean singleClickViewThroughTarget(
            View view, View target, int x, int y) {
        int windowXY[] = viewToWindowCoordinates(view, x, y);
        int windowX = windowXY[0];
        int windowY = windowXY[1];
        return singleClickInternal(target, windowX, windowY);
    }

    /**
     * Sends (synchronously) a single click to the center of the View.
     */
    public static void singleClickView(View v) {
        singleClickView(v, v.getWidth() / 2, v.getHeight() / 2);
    }

    private static boolean singleClickInternal(View view, float windowX, float windowY) {
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_DOWN, windowX, windowY, 0);
        if (!dispatchTouchEvent(view, event)) return false;

        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP, windowX, windowY, 0);
        return dispatchTouchEvent(view, event);
    }

    /**
     * Sends (synchronously) a long press to the View at the specified coordinates.
     *
     * @param v The view to receive the long press.
     * @param x X coordinate, relative to v.
     * @param y Y coordinate, relative to v.
     */
    public static void longPressView(View v, int x, int y) {
        int windowXY[] = viewToWindowCoordinates(v, x, y);
        int windowX = windowXY[0];
        int windowY = windowXY[1];
        longPressInternal(v.getRootView(), windowX, windowY);
    }

    /**
     * Sends (synchronously) a long press to the center of the View.
     * <p>Note that view should be located in the current position for a foreseeable
     * amount because this involves sleep to simulate touch to long press transition.
     *
     * @param v The view to receive the long press.
     */
    public static void longPressView(View v) {
        longPressView(v, v.getWidth() / 2, v.getHeight() / 2);
    }

    /**
     * Sends (synchronously) a long press to the View at the specified coordinates, without release.
     *
     * @param v The view to receive the long press.
     * @param x X coordinate, relative to v.
     * @param y Y coordinate, relative to v.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    public static void longPressViewWithoutUp(View v, int x, int y, long downTime) {
        int windowXY[] = viewToWindowCoordinates(v, x, y);
        int windowX = windowXY[0];
        int windowY = windowXY[1];
        longPressWithoutUpInternal(v.getRootView(), windowX, windowY, downTime);
    }

    private static void longPressWithoutUpInternal(
            View view, float windowX, float windowY, long downTime) {
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_DOWN, windowX, windowY, 0);
        dispatchTouchEvent(view, event);

        int longPressTimeout = ViewConfiguration.getLongPressTimeout();

        // Long press is flaky with just longPressTimeout. Doubling the time to be safe.
        SystemClock.sleep(longPressTimeout * 2);
    }

    private static void longPressInternal(View view, float windowX, float windowY) {
        long downTime = SystemClock.uptimeMillis();
        longPressWithoutUpInternal(view, windowX, windowY, downTime);

        long eventTime = SystemClock.uptimeMillis();
        MotionEvent event =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP, windowX, windowY, 0);
        dispatchTouchEvent(view, event);
    }

    private static View getRootViewForActivity(final Activity activity) {
        try {
            View view = TestThreadUtils.runOnUiThreadBlocking(new Callable<View>() {
                @Override
                public View call() {
                    return activity.findViewById(android.R.id.content).getRootView();
                }
            });
            assert view != null : "Failed to find root view for activity";
            return view;
        } catch (ExecutionException e) {
            throw new RuntimeException("Dispatching touch event failed", e);
        }
    }

    /**
     * Sends a MotionEvent to the specified view.
     * @param view The view that should receive the event.
     * @param event The view to be dispatched.
     */
    private static boolean dispatchTouchEvent(final View view, final MotionEvent event) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return view.dispatchTouchEvent(event);
                }
            });
        } catch (Throwable e) {
            throw new RuntimeException("Dispatching touch event failed", e);
        }
    }

    /**
     * Converts view-relative coordinates into window coordinates.
     * @param v The view the coordinates are relative to.
     * @param x X coordinate, relative to the view.
     * @param y Y coordinate, relative to the view.
     * @return The coordinates relative to the window as a 2-element array.
     */
    private static int[] viewToWindowCoordinates(View v, int x, int y) {
        int windowXY[] = new int[2];
        v.getLocationInWindow(windowXY);
        windowXY[0] += x;
        windowXY[1] += y;
        return windowXY;
    }

    /**
     * Converts screen coordinates into window coordinates.
     * @param view Any view in the window.
     * @param screenX X coordinate relative to the screen.
     * @param screenY Y coordinate relative to the screen.
     * @return The coordinates relative to the window as a 2-element array.
     */
    private static float[] screenToWindowCoordinates(View view, float screenX, float screenY) {
        View root = view.getRootView();
        int[] rootScreenXY = new int[2];
        int[] rootWindowXY = new int[2];
        root.getLocationOnScreen(rootScreenXY);
        root.getLocationInWindow(rootWindowXY);
        float windowX = screenX - rootScreenXY[0] + rootWindowXY[0];
        float windowY = screenY - rootScreenXY[1] + rootWindowXY[1];
        return new float[] {windowX, windowY};
    }
}
