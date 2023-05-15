// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.os.Process;

import org.chromium.base.BuildInfo;

/**
 * Handler that immediately kills the current process on an uncaught exception.
 * This is intended to override Android's default exception handler, which pops up a dialog
 * and does not finish until user dismisses that dialog.
 *
 * Notes:
 * This does not chain the existing handler.
 * This does not have any exception handling or crash reporting. Such handlers should be
 * chained before this handler.
 */
class KillChildUncaughtExceptionHandler implements Thread.UncaughtExceptionHandler {
    private boolean mCrashing;

    // Should be called early on start up.
    static void maybeInstallHandler() {
        // Only suppress default dialog on release builds. This matches behavior for native crashes
        // in breakpad::FinalizeCrashDoneAndroid, where the dialog is suppressed on release builds
        // due to bad user experience. Note this is also the reason the exception stack is not
        // printed here, to avoid the stack being printed twice in release builds where breakpad
        // is also enabled.
        if (BuildInfo.isDebugAndroid()) return;
        Thread.setDefaultUncaughtExceptionHandler(new KillChildUncaughtExceptionHandler());
    }

    @Override
    @SuppressWarnings("checkstyle:SystemExitCheck") // Allowed since the goal is to mimic Android.
    public void uncaughtException(Thread t, Throwable e) {
        // Never re-enter.
        if (mCrashing) return;
        mCrashing = true;

        // Copied from Android KillApplicationHandler in RuntimeInit.java. This is how the default
        // Android handler kills this process.
        Process.killProcess(Process.myPid());
        System.exit(10);
    }
}
