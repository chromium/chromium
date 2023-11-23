// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.view.View;

import org.chromium.content.browser.MotionEventSynthesizerImpl;

/** Injects synthetic touch events. All the coordinates are of physical unit. */
public interface MotionEventSynthesizer {
    public static MotionEventSynthesizer create(View target) {
        return MotionEventSynthesizerImpl.create(target);
    }

    /**
     * Sets the coordinate of the point at which a touch event takes place.
     *
     * @param index Index of the point when there are multiple points.
     * @param x X coordinate of the point.
     * @param x Y coordinate of the point.
     * @param id Id property of the point.
     * @param toolType ToolType property of the point.
     */
    void setPointer(int index, float x, float y, int id, int toolType);

    /**
     * Injects a synthetic action with the preset points and delta.
     *
     * @param action Type of the action to inject.
     * @param pointerCount The number of points associated with the event.
     * @param pointerIndex The index of the event to send. In the case of
     *        START and END, eg, we send a separate event as each pointer starts
     *        or ends, respectively.
     * @param timeInMs Timestamp for the event.
     */
    void inject(int action, int pointerCount, int pointerIndex, long timeInMs);
}
