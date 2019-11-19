// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.content.browser.ScreenOrientationProviderImpl;
import org.chromium.ui.base.WindowAndroid;

/**
 * Interface providing the access to C++ ScreenOrientationProvider.
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

    void unlockOrientation(@Nullable WindowAndroid window);

    /** Delays screen orientation requests for the given window. */
    void delayOrientationRequests(WindowAndroid window);

    /** Runs delayed screen orientation requests for the given window. */
    void runDelayedOrientationRequests(WindowAndroid window);

    void setOrientationDelegate(ScreenOrientationDelegate delegate);
}
