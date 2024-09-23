// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.app.Instrumentation;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Collection of utilities for generating touch events.
 * Based on android.test.TouchUtils, but slightly more flexible (allows to
 * specify coordinates for longClick, splits drag operation in three stages, etc).
 */
public class TestTouchUtils {
    /**
     * Returns the absolute location in screen coordinates from location relative
     * to view.
     * @param v The view the coordinates are relative to.
     * @param x Relative x location.
     * @param y Relative y location.
     * @return the absolute x and y location in an array.
     */
    public static int[] getAbsoluteLocationFromRelative(View v, int x, int y) {
        int location[] = new int[2];
        v.getLocationOnScreen(location);
        location[0] += x;
        location[1] += y;
        return location;
    }

    private static void sendAction(
            Instrumentation instrumentation, int action, long downTime, float x, float y) {
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        instrumentation.sendPointerSync(event);
        instrumentation.waitForIdleSync();
    }

    /**
     * Sends (synchronously) a single click to an absolute screen coordinates.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param x Screen absolute x location.
     * @param y Screen absolute y location.
     */
    public static void singleClick(Instrumentation instrumentation, float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        sendAction(instrumentation, MotionEvent.ACTION_DOWN, downTime, x, y);
        sendAction(instrumentation, MotionEvent.ACTION_UP, downTime, x, y);
    }

    /**
     * Sends (synchronously) a single click to the View at the specified coordinates.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view the coordinates are relative to.
     * @param x Relative x location to the view.
     * @param y Relative y location to the view.
     */
    public static void singleClickView(Instrumentation instrumentation, View v, int x, int y) {
        int location[] = getAbsoluteLocationFromRelative(v, x, y);
        int absoluteX = location[0];
        int absoluteY = location[1];
        singleClick(instrumentation, absoluteX, absoluteY);
    }

    /**
     * Sends (synchronously) a single click to the center of the View.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view the coordinates are relative to.
     */
    public static void singleClickView(Instrumentation instrumentation, View v) {
        int x = v.getWidth() / 2;
        int y = v.getHeight() / 2;
        singleClickView(instrumentation, v, x, y);
    }

    /**
     * Sleeps for at least the length of the double tap timeout.
     *
     * @param instrumentation Instrumentation object used by the test.
     */
    public static void sleepForDoubleTapTimeout(Instrumentation instrumentation) {
        SystemClock.sleep((long) (ViewConfiguration.getDoubleTapTimeout() * 1.5));
    }

    /**
     * Sends (synchronously) a long click to the View at the specified coordinates.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view the coordinates are relative to.
     * @param x Relative x location to the view.
     * @param y Relative y location to the view.
     */
    public static void longClickView(Instrumentation instrumentation, View v, int x, int y) {
        int location[] = getAbsoluteLocationFromRelative(v, x, y);
        int absoluteX = location[0];
        int absoluteY = location[1];

        long downTime = SystemClock.uptimeMillis();
        sendAction(instrumentation, MotionEvent.ACTION_DOWN, downTime, absoluteX, absoluteY);
        SystemClock.sleep((long) (ViewConfiguration.getLongPressTimeout() * 1.5));
        sendAction(instrumentation, MotionEvent.ACTION_UP, downTime, absoluteX, absoluteY);
    }

    /**
     * Sends (synchronously) a long click to the View at its center.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view to long click.
     */
    public static void longClickView(Instrumentation instrumentation, View v) {
        int x = v.getWidth() / 2;
        int y = v.getHeight() / 2;
        longClickView(instrumentation, v, x, y);
    }

    /**
     * Starts (synchronously) a drag motion. Normally followed by dragTo() and dragEnd().
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param x The x location.
     * @param y The y location.
     * @return The downTime of the triggered event.
     */
    public static long dragStart(Instrumentation instrumentation, float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        sendAction(instrumentation, MotionEvent.ACTION_DOWN, downTime, x, y);
        return downTime;
    }

    /**
     * Drags / moves (synchronously) to the specified coordinates. Normally preceded by
     * dragStart() and followed by dragEnd()
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param fromX The relative x-coordinate of the start point of the drag.
     * @param toX The relative x-coordinate of the end point of the drag.
     * @param fromY The relative y-coordinate of the start point of the drag.
     * @param toY The relative y-coordinate of the end point of the drag.
     * @param stepCount The total number of motion events that should be generated during the drag.
     * @param downTime The initial time of the drag, in ms.
     */
    public static void dragTo(
            Instrumentation instrumentation,
            float fromX,
            float toX,
            float fromY,
            float toY,
            int stepCount,
            long downTime) {
        float x = fromX;
        float y = fromY;
        float yStep = (toY - fromY) / stepCount;
        float xStep = (toX - fromX) / stepCount;
        for (int i = 0; i < stepCount; ++i) {
            y += yStep;
            x += xStep;
            sendAction(instrumentation, MotionEvent.ACTION_MOVE, downTime, x, y);
        }
    }

    /**
     * Finishes (synchronously) a drag / move at the specified coordinate.
     * Normally preceded by dragStart() and dragTo().
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param x The x location.
     * @param y The y location.
     * @param downTime The initial time of the drag, in ms.
     */
    public static void dragEnd(Instrumentation instrumentation, float x, float y, long downTime) {
        sendAction(instrumentation, MotionEvent.ACTION_UP, downTime, x, y);
    }

    /**
     * Performs a drag between the given coordinates, specified relative to the given view.
     * This method makes calls to dragStart, dragTo and dragEnd.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param view The view the coordinates are relative to.
     * @param fromX The relative x-coordinate of the start point of the drag.
     * @param toX The relative x-coordinate of the end point of the drag.
     * @param fromY The relative y-coordinate of the start point of the drag.
     * @param toY The relative y-coordinate of the end point of the drag.
     * @param stepCount The total number of motion events that should be generated during the drag.
     */
    public static void dragCompleteView(
            Instrumentation instrumentation,
            View view,
            int fromX,
            int toX,
            int fromY,
            int toY,
            int stepCount) {
        int fromLocation[] = getAbsoluteLocationFromRelative(view, fromX, fromY);
        int toLocation[] = getAbsoluteLocationFromRelative(view, toX, toY);
        long downTime = dragStart(instrumentation, fromLocation[0], fromLocation[1]);
        dragTo(
                instrumentation,
                fromLocation[0],
                toLocation[0],
                fromLocation[1],
                toLocation[1],
                stepCount,
                downTime);
        dragEnd(instrumentation, toLocation[0], toLocation[1], downTime);
    }

    /**
     * Calls performClick on a View on the main UI thread.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view to call performClick on.
     */
    public static void performClickOnMainSync(Instrumentation instrumentation, final View v) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    v.performClick();
                });
    }

    /**
     * Calls performLongClick on a View on the main UI thread.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view to call performLongClick on.
     */
    public static void performLongClickOnMainSync(Instrumentation instrumentation, final View v)
            throws ExecutionException {
        ThreadUtils.runOnUiThreadBlocking(() -> v.performLongClick());
    }
}
