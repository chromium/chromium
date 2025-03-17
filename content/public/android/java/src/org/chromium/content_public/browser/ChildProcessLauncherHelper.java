// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content.browser.ChildProcessLauncherHelperImpl;

/** Interface for helper launching child processes. */
@NullMarked
public final class ChildProcessLauncherHelper {
    private ChildProcessLauncherHelper() {}

    /**
     * Creates a ready to use sandboxed child process. Should be called early during startup so the
     * child process is created while other startup work is happening.
     *
     * @param context the application context used for the connection.
     */
    public static void warmUpOnAnyThread(Context context) {
        ChildProcessLauncherHelperImpl.warmUpOnAnyThread(context);
    }

    /**
     * Starts the binding management that adjust a process priority in response to various signals
     * (app sent to background/foreground for example). Note: WebAPKs and non WebAPKs share the same
     * binding pool, so the size of the shared binding pool is always set based on the number of
     * sandboxes processes used by Chrome.
     *
     * @param context Android's context.
     */
    public static void startBindingManagement(Context context) {
        ChildProcessLauncherHelperImpl.startBindingManagement(context);
    }

    /** Changes priority setting to ignore main frame visibility to determine process importance. */
    public static void setIgnoreMainFrameVisibilityForImportance() {
        ChildProcessLauncherHelperImpl.setIgnoreMainFrameVisibilityForImportance();
    }
}
