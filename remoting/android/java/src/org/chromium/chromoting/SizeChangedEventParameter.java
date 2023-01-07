// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * The {@link Event} parameter for a size changed event.
 *
 * {@link android.graphics.Point} and {@link android.graphics.PointF} are both mutable, so this
 * class uses two floats instead.
 */
public final class SizeChangedEventParameter {
    public final int width;
    public final int height;

    public SizeChangedEventParameter(int width, int height) {
        this.width = width;
        this.height = height;
    }
}
