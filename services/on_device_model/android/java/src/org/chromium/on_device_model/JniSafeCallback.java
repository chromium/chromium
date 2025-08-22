// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import androidx.annotation.GuardedBy;

import org.chromium.build.annotations.NullMarked;

/**
 * A helper to safely make a JNI callback from any thread. It ensures that the callback is not
 * executed after the native object has been destroyed.
 */
@NullMarked
class JniSafeCallback {
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private boolean mNativeDestroyed;

    /** Runs the given runnable if the native object has not been destroyed. */
    public void run(Runnable runnable) {
        synchronized (mLock) {
            if (mNativeDestroyed) return;
            runnable.run();
        }
    }

    /**
     * Called when the native object is destroyed.
     *
     * @param onNativeDestroyed The runnable to be executed when the native object is destroyed.
     */
    public void onNativeDestroyed(Runnable onNativeDestroyed) {
        synchronized (mLock) {
            mNativeDestroyed = true;
            onNativeDestroyed.run();
        }
    }
}
