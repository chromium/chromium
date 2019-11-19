// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A state machine to indicate user input actions. It stores the start action (tap or long tap),
 * finger count, detected action, etc.
 */
public class InputState {
    /**
     * A settable {@link InputState}.
     */
    public static final class Settable extends InputState {
        public void setFingerCount(int fingerCount) {
            mFingerCount = fingerCount;
            if (mFingerCount == 0) {
                mStartAction = StartAction.UNDEFINED;
                mDetectedAction = DetectedAction.UNDEFINED;
            }
        }

        public void setStartAction(@StartAction int startAction) {
            Preconditions.isTrue(startAction != StartAction.UNDEFINED);
            mStartAction = startAction;
        }

        public void setDetectedAction(@DetectedAction int detectedAction) {
            Preconditions.isTrue(detectedAction != DetectedAction.UNDEFINED);
            mDetectedAction = detectedAction;
        }
    }

    @IntDef({StartAction.UNDEFINED, StartAction.LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StartAction {
        int UNDEFINED = 0;
        // The action started from a long press. Note, a tap won't need to impact InputState.
        int LONG_PRESS = 1;
    }

    @IntDef({DetectedAction.UNDEFINED, DetectedAction.SCROLL, DetectedAction.SCROLL_FLING,
            DetectedAction.AFTER_SCROLL_FLING, DetectedAction.FLING, DetectedAction.SCALE,
            DetectedAction.SWIPE, DetectedAction.MOVE, DetectedAction.SCROLL_EDGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DetectedAction {
        int UNDEFINED = 0;
        int SCROLL = 1;
        int SCROLL_FLING = 2;
        // AFTER_SCROLL_FLING is a fake action to indicate the state after a scroll fling has been
        // performed.
        int AFTER_SCROLL_FLING = 3;
        int FLING = 4;
        int SCALE = 5;
        int SWIPE = 6;
        int MOVE = 7;
        int SCROLL_EDGE = 8;
    }

    protected int mFingerCount;
    protected @StartAction int mStartAction;
    protected @DetectedAction int mDetectedAction;

    public InputState() {
        mStartAction = StartAction.UNDEFINED;
        mFingerCount = 0;
        mDetectedAction = DetectedAction.UNDEFINED;
    }

    public int getFingerCount() {
        return mFingerCount;
    }

    public @StartAction int getStartAction() {
        return mStartAction;
    }

    public @DetectedAction int getDetectedAction() {
        return mDetectedAction;
    }

    public boolean shouldSuppressCursorMovement() {
        return mDetectedAction == DetectedAction.SWIPE
                || mDetectedAction == DetectedAction.SCROLL_FLING
                || mDetectedAction == DetectedAction.SCROLL_EDGE;
    }

    public boolean shouldSuppressFling() {
        return mDetectedAction == DetectedAction.SWIPE
                || mStartAction == StartAction.LONG_PRESS;
    }

    public boolean isScrollFling() {
        return mDetectedAction == DetectedAction.SCROLL_FLING;
    }

    public boolean swipeCompleted() {
        return mDetectedAction == DetectedAction.SWIPE;
    }

    public boolean isDragging() {
        return mStartAction == StartAction.LONG_PRESS;
    }
}
