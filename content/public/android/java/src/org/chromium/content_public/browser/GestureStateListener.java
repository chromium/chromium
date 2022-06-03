// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * An interface that is notified of events and state changes related to gesture processing
 * from content layer.
 */
public interface GestureStateListener {
    /**
     * Called when the pinch gesture starts.
     */
    public default void onPinchStarted() {}

    /**
     * Called when the pinch gesture ends.
     */
    public default void onPinchEnded() {}

    /**
     * Called when a fling starts.
     */
    public default void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called when a fling has ended.
     */
    public default void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called to indicate that a scroll update gesture had been consumed by the page.
     * This callback is called whenever any layer is scrolled (like a frame or div). It is
     * not called when a JS touch handler consumes the event (preventDefault), it is not called
     * for JS-initiated scrolling.
     */
    public default void onScrollUpdateGestureConsumed() {}

    /**
     * Called when a scroll gesture has started.
     */
    public default void onScrollStarted(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called when a scroll gesture has stopped.
     */
    public default void onScrollEnded(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called when the min or max scale factor may have been changed.
     */
    public default void onScaleLimitsChanged(float minPageScaleFactor, float maxPageScaleFactor) {}

    /**
     * Called at the beginning of any kind of touch event when the user's finger first touches down
     * onto the screen.  The resulting gesture may be a single tap, long-press, or scroll.
     */
    public default void onTouchDown() {}

    /**
     * Called after a single-tap gesture event was dispatched to the renderer,
     * indicating whether or not the gesture was consumed.
     */
    public default void onSingleTap(boolean consumed) {}

    /**
     * Called after a single-tap gesture event was processed by the renderer,
     * but was not handled.
     */
    public default void onShowUnhandledTapUIIfNeeded(int x, int y) {}

    /**
     * Called when the gesture source loses window focus.
     */
    public default void onWindowFocusChanged(boolean hasWindowFocus) {}

    /**
     * Called when a long press gesture event was processed by the rendereer.
     */
    public default void onLongPress() {}

    /**
     * Called when the gesture source is being destroyed.
     */
    public default void onDestroyed() {}
}
