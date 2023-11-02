// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.content.browser.ScreenOrientationProviderImpl;
import org.chromium.ui.base.WindowAndroid;

/**
 * Interface providing the access to C++ ScreenOrientationProvider.
 * TODO(boliu): This interface working with WindowAndroid does not support the use case
 * when an Activity (and WindowAndroid) is recreated on rotation.
 */
public interface ScreenOrientationProvider {
    static ScreenOrientationProvider getInstance() {
        return ScreenOrientationProviderImpl.getInstance();
    }

    /**
     * Locks screen rotation to a given orientation.
     * @param window Window to lock rotation on.
     * @param webScreenOrientation Screen orientation.
     */
    void lockOrientation(@Nullable WindowAndroid window, byte webScreenOrientation);

    /**
     * Unlocks screen orientation.
     * @param window Window to unlock rotation on.
     */
    void unlockOrientation(@Nullable WindowAndroid window);

    /** Delays screen orientation requests for the given window. */
    void delayOrientationRequests(WindowAndroid window);

    /** Runs delayed screen orientation requests for the given window. */
    void runDelayedOrientationRequests(WindowAndroid window);

    void setOrientationDelegate(ScreenOrientationDelegate delegate);

    /**
     * Sets a default screen orientation for a given window.
     * @param window Window to lock rotation on.
     * @param defaultWebOrientation a default screen orientation for the window.
     */
    void setOverrideDefaultOrientation(WindowAndroid window, byte defaultWebOrientation);
}
