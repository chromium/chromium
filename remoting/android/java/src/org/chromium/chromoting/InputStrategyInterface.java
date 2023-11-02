// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.MotionEvent;

/**
 * This interface defines the methods used to customize input handling for
 * a particular strategy.  The implementing class is responsible for sending
 * remote input events and defining implementation specific behavior.
 */
public interface InputStrategyInterface {
    /**
     * Called when a user tap has been detected.
     *
     * @param button The button value for the tap event.
     * @return A boolean representing whether the event was handled.
     */
    boolean onTap(int button);

    /**
     * Called when the user has put one or more fingers down on the screen for a period of time.
     *
     * @param button The button value for the tap event.
     * @return A boolean representing whether the event was handled.
     */
    boolean onPressAndHold(int button);

    /**
     * Called when a MotionEvent is received.  This method allows the input strategy to store or
     * react to specific MotionEvents as needed.
     *
     * @param event The original event for the current touch motion.
     */
    void onMotionEvent(MotionEvent event);

    /**
     * Called when the user is attempting to scroll/pan the remote UI.
     *
     * @param distanceX The distance moved along the x-axis.
     * @param distanceY The distance moved along the y-axis.
     */
    void onScroll(float distanceX, float distanceY);

    /**
     * Called to update the remote cursor position.
     *
     * @param x The new x coordinate of the cursor.
     * @param y The new y coordinate of the cursor.
     */
    void injectCursorMoveEvent(int x, int y);

    /**
     * Returns the feedback animation type to use for a short press.
     *
     * @return The feedback to display when a short press occurs.
     */
    @RenderStub.InputFeedbackType
    int getShortPressFeedbackType();

    /**
     * Returns the feedback animation type to use for a long press.
     *
     * @return The feedback to display when a long press occurs.
     */
    @RenderStub.InputFeedbackType
    int getLongPressFeedbackType();

    /**
     * Indicates whether this input mode is an indirect input mode.  Indirect input modes manipulate
     * the cursor in a detached fashion (such as a trackpad) and direct input modes will update the
     * cursor/screen position to match the location of the touch point.
     *
     * @return A boolean representing whether this input mode is indirect (true) or direct (false).
     */
    boolean isIndirectInputMode();
}
