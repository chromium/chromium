// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;
import android.os.Looper;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.JavaHandlerThread;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** This is the process launcher thread. It is available before native library is loaded. */
@JNINamespace("content::android")
public final class LauncherThread {
    private static final JavaHandlerThread sThread =
            new JavaHandlerThread("Chrome_ProcessLauncherThread", Process.THREAD_PRIORITY_DEFAULT);
    private static final Handler sThreadHandler;
    // Can be overritten in tests.
    private static Handler sHandler;
    static {
        sThread.maybeStart();
        sThreadHandler = new Handler(sThread.getLooper());
        sHandler = sThreadHandler;
    }

    public static void post(Runnable r) {
        sHandler.post(r);
    }

    public static void postDelayed(Runnable r, long delayMillis) {
        sHandler.postDelayed(r, delayMillis);
    }

    public static void removeCallbacks(Runnable r) {
        sHandler.removeCallbacks(r);
    }

    public static boolean runningOnLauncherThread() {
        return sHandler.getLooper() == Looper.myLooper();
    }

    public static Handler getHandler() {
        return sHandler;
    }

    @VisibleForTesting
    public static void setCurrentThreadAsLauncherThread() {
        sHandler = new Handler();
    }

    @VisibleForTesting
    public static void setLauncherThreadAsLauncherThread() {
        sHandler = sThreadHandler;
    }

    @CalledByNative
    private static JavaHandlerThread getHandlerThread() {
        return sThread;
    }

    private LauncherThread() {}
}
