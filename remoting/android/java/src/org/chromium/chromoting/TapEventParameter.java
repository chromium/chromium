// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.MotionEvent;

/**
 * {@link Event} parameter for tap events, represents both {@link pointerCount} and position of
 * the first touch point ({@link x} and {@link y}). {@link android.graphics.Point} and
 * {@link android.graphics.PointF} are both mutable, so this class uses two floats instead.
 */
public final class TapEventParameter {
    public final int pointerCount;
    public final float x;
    public final float y;
    public boolean handled;

    public TapEventParameter(int pointerCount, float x, float y) {
        this.pointerCount = pointerCount;
        this.x = x;
        this.y = y;
        this.handled = false;
    }

    public TapEventParameter(MotionEvent event) {
        this.pointerCount = event.getPointerCount();
        int pointerIndex = 0;
        if (event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
            pointerIndex = event.getActionIndex();
        }
        this.x = event.getX(pointerIndex);
        this.y = event.getY(pointerIndex);
    }
}
