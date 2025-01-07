// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * An interface that is notified of events and state changes related to gesture processing
 * from content layer.
 */
public abstract class GestureStateListener {
    /** Called when the pinch gesture starts. */
    public void onPinchStarted() {}

    /** Called when the pinch gesture ends. */
    public void onPinchEnded() {}

    /** Called when a fling starts. */
    public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {}

    /** Called when a fling has ended. */
    public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called to indicate that a scroll update gesture had been consumed by the page. This callback
     * is called whenever any layer is scrolled (like a frame or div). It is not called when a JS
     * touch handler consumes the event (preventDefault), it is not called for JS-initiated
     * scrolling.
     */
    public void onScrollUpdateGestureConsumed() {}

    /** Called when a scroll gesture has started. */
    public void onScrollStarted(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {}

    /**
     * Called when the scroll direction changes.
     * @param directionUp Whether the scroll direction is up, i.e. swiping down.
     * @param currentScrollRatio The current scroll ratio of the page.
     */
    public void onVerticalScrollDirectionChanged(boolean directionUp, float currentScrollRatio) {}

    /** Called when a scroll gesture has stopped. */
    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {}

    /** Called when a gesture begin event has been processed. */
    public void onGestureBegin() {}

    /** Called when a gesture end event has been processed. */
    public void onGestureEnd() {}

    /** Called when the min or max scale factor may have been changed. */
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

    /** Called when the gesture source loses window focus. */
    public void onWindowFocusChanged(boolean hasWindowFocus) {}

    /** Called when the scroll offsets or extents may have changed. */
    public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {}

    /** Called when the gesture source is being destroyed. */
    public void onDestroyed() {}
}
