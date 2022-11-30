// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.MotionEvent;

/**
 * An {@link Event} parameter for a touch event.
 */
public final class TouchEventParameter {
    public final MotionEvent event;
    public boolean handled;

    public TouchEventParameter(MotionEvent event) {
        this.event = event;
        this.handled = false;
    }
}
