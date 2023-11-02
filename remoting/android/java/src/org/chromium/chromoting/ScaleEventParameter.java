// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * {@link Event} parameter for scale events, represents both {@link scaleFactor} and the focus
 * ({@link focusX} and {@link focusY}) of the gesture.
 *
 * {@link android.graphics.Point} and {@link android.graphics.PointF} are both mutable, so this
 * class uses two floats instead.
 */
public final class ScaleEventParameter {
    public final float scaleFactor;
    public final float focusX;
    public final float focusY;

    public ScaleEventParameter(float scaleFactor, float focusX, float focusY) {
        this.scaleFactor = scaleFactor;
        this.focusX = focusX;
        this.focusY = focusY;
    }
}
