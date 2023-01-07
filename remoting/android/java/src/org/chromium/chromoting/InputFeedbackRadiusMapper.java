// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.View;

import org.chromium.chromoting.RenderStub.InputFeedbackType;

/**
 * Helper class for mapping a feedback type to the max radius of the feedback animation circle.
 */
public final class InputFeedbackRadiusMapper {
    private final int mTinyFeedbackPixelRadius;
    private final int mSmallFeedbackPixelRadius;
    private final int mLargeFeedbackPixelRadius;

    public InputFeedbackRadiusMapper(View view) {
        mTinyFeedbackPixelRadius = view.getResources()
                .getDimensionPixelSize(R.dimen.feedback_animation_radius_tiny);

        mSmallFeedbackPixelRadius = view.getResources()
                .getDimensionPixelSize(R.dimen.feedback_animation_radius_small);

        mLargeFeedbackPixelRadius = view.getResources()
                .getDimensionPixelSize(R.dimen.feedback_animation_radius_large);
    }

    /**
     * @param feedbackToShow the feedback type to be mapped to the radius of the feedback circle.
     * @param scaleFactor Current scale factor of the desktop canvas.
     * @return the radius of the given feedback type. It may be 0, in which case nothing needs to
     *         be shown.
     */
    public float getFeedbackRadius(@InputFeedbackType int feedbackToShow, float scaleFactor) {
        switch (feedbackToShow) {
            case InputFeedbackType.NONE:
                return 0.0f;
            case InputFeedbackType.SHORT_TOUCH_ANIMATION:
                return mSmallFeedbackPixelRadius / scaleFactor;
            case InputFeedbackType.LONG_TOUCH_ANIMATION:
                return mLargeFeedbackPixelRadius / scaleFactor;
            case InputFeedbackType.LONG_TRACKPAD_ANIMATION:
                // The size of the longpress trackpad animation is supposed to be close to the
                // size of the cursor so it doesn't need to be normalized and should be scaled
                // with the canvas.
                return mTinyFeedbackPixelRadius;
            default:
                // Unreachable, but required by Google Java style and findbugs.
                assert false : "Unreached";
                return 0.0f;
        }
    }
}
