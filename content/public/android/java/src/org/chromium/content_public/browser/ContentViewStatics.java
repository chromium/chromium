// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.ContentViewStaticsImpl;

/** Implementations of various static methods. */
public class ContentViewStatics {
    /**
     * Suspends Webkit timers in all renderers.
     * New renderers created after this call will be created with the
     * default options.
     *
     * @param suspend true if timers should be suspended.
     */
    public static void setWebKitSharedTimersSuspended(boolean suspend) {
        ContentViewStaticsImpl.setWebKitSharedTimersSuspended(suspend);
    }

    /**
     * Enables platform notifications of data state and proxy changes.
     * Notifications are enabled by default.
     */
    public static void enablePlatformNotifications() {
        ContentViewStaticsImpl.enablePlatformNotifications();
    }

    /**
     * Disables platform notifications of data state and proxy changes.
     * Notifications are enabled by default.
     */
    public static void disablePlatformNotifications() {
        ContentViewStaticsImpl.disablePlatformNotifications();
    }
}
