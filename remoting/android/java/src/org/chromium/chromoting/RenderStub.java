// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
import android.graphics.PointF;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface with a set of functions to control the behavior of the remote host renderer.
 */
public interface RenderStub {
    /** Used to define the animation feedback shown when a user touches the screen. */
    @IntDef({InputFeedbackType.NONE, InputFeedbackType.SHORT_TOUCH_ANIMATION,
            InputFeedbackType.LONG_TOUCH_ANIMATION, InputFeedbackType.LONG_TRACKPAD_ANIMATION})
    @Retention(RetentionPolicy.SOURCE)
    @interface InputFeedbackType {
        int NONE = 0;
        int SHORT_TOUCH_ANIMATION = 1;
        int LONG_TOUCH_ANIMATION = 2;
        int LONG_TRACKPAD_ANIMATION = 3;
    }

    /**
     * Resets the stub with |view|. The stub should avoid holding strong reference to |view|.
     */
    void setDesktopView(DesktopView view);

    /** Triggers a brief animation to indicate the existence and location of an input event. */
    void showInputFeedback(@InputFeedbackType int feedbackToShow, PointF pos);

    /**
     * Informs the stub that its transformation matrix (for rendering the remote desktop bitmap)
     * has been changed, which requires repainting.
     */
    void setTransformation(Matrix matrix);

    /**
     * Informs the stub that the cursor position has been moved, which requires repainting.
     */
    void moveCursor(PointF pos);

    /**
     * Informs the stub that the cursor visibility has been changed (for different input mode),
     * which requires repainting.
     */
    void setCursorVisibility(boolean visible);

    /** An {@link Event} triggered when the client size is changed. */
    Event<SizeChangedEventParameter> onClientSizeChanged();

    /**
     * An {@link Event} triggered when the size of the host desktop is changed.
     */
    Event<SizeChangedEventParameter> onHostSizeChanged();

    /**
     * An {@link} triggered when a frame has been rendered.
     */
    Event<Void> onCanvasRendered();
}
