// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * The parameter for the OnSystemUiVisibilityChanged event.
 *
 * {@link android.graphics.Rect} is mutable, so this class owns four integers to represent the
 * inset between the System UI elements which is used for our viewable content.
 */
public final class SystemUiVisibilityChangedEventParameter {
    public final int left;
    public final int top;
    public final int right;
    public final int bottom;

    public SystemUiVisibilityChangedEventParameter(int left, int top, int right, int bottom) {
        this.left = left;
        this.top = top;
        this.right = right;
        this.bottom = bottom;
    }
}
