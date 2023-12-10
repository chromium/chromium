// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.app.Activity;

/**
 * An interface for ScreenOrientationProvider to notify other components that orientation
 * preferences may change.
 */
public interface ScreenOrientationDelegate {
    /**
     * Notify the delegate that ScreenOrientationProvider consumers would like to unlock orientation
     * for an activity. Returns true if ScreenOrientationProvider should unlock orientation, and
     * false if the delegate already handled it.
     */
    boolean canUnlockOrientation(Activity activity, int defaultOrientation);

    /**
     * Allows the delegate to control whether ScreenOrientationProvider clients
     * can lock orientation.
     */
    boolean canLockOrientation();
}
