// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Point;

import androidx.annotation.Nullable;

/**
 * An interface that is notified of events and state changes related to gesture processing
 * from content layer.
 */
public abstract class GestureStateListener {
    /**
     * Called when the pinch gesture starts.
     */
    public void onPinchStarted() {}

    /**
     * Called when the pinch gesture ends.
     */
    public void onPinchEnded() {}

    /**
     * Called when a fling starts.
     */
    public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {}

    /**
     * Called when a fling has ended.
     */
    public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called to indicate that a scroll update gesture had been consumed by the page.
     * This callback is called whenever any layer is scrolled (like a frame or div). It is
     * not called when a JS touch handler consumes the event (preventDefault), it is not called
     * for JS-initiated scrolling.
     *
     * @param rootScrollOffset Updated root scroll offset if the scroll was consumed by the
     *                         viewport, null otherwise.
     */
    public void onScrollUpdateGestureConsumed(@Nullable Point rootScrollOffset) {}

    /**
     * Called when a scroll gesture has started.
     */
    public void onScrollStarted(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {}

    /**
     * Called when the scroll direction changes.
     * @param directionUp Whether the scroll direction is up, i.e. swiping down.
     * @param currentScrollRatio The current scroll ratio of the page.
     */
    public void onVerticalScrollDirectionChanged(boolean directionUp, float currentScrollRatio) {}

    /**
     * Called when a scroll gesture has stopped.
     */
    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called when the min or max scale factor may have been changed.
     */
    public void onScaleLimitsChanged(float minPageScaleFactor, float maxPageScaleFactor) {}

    /**
     * Called at the beginning of any kind of touch event when the user's finger first touches down
     * onto the screen.  The resulting gesture may be a single tap, long-press, or scroll.
     */
    public void onTouchDown() {}

    /**
     * Called after a single-tap gesture event was dispatched to the renderer,
     * indicating whether or not the gesture was consumed.
     */
    public void onSingleTap(boolean consumed) {}

    /**
     * Called after a single-tap gesture event was processed by the renderer,
     * but was not handled.
     */
    public void onShowUnhandledTapUIIfNeeded(int x, int y) {}

    /**
     * Called when the gesture source loses window focus.
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {}

    /**
     * Called when a long press gesture event was processed by the rendereer.
     */
    public void onLongPress() {}

    /**
     * Called on overscroll. This happens when user tries to scroll beyond scroll bounds, or when
     * a fling animation hits scroll bounds.
     * @param accumulatedOverscrollX see `ui::DidOverscrollParams::accumulated_overscroll`.
     * @param accumulatedOverscrollY see `ui::DidOverscrollParams::accumulated_overscroll`.
     */
    public void didOverscroll(float accumulatedOverscrollX, float accumulatedOverscrollY) {}

    /**
     * Called when the gesture source is being destroyed.
     */
    public void onDestroyed() {}
}
