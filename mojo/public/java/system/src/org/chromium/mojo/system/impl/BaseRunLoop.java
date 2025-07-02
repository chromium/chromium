// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.mojo.system.RunLoop;

/** Implementation of {@link RunLoop} suitable for the base:: message loop implementation. */
@JNINamespace("mojo::android")
@NullMarked
class BaseRunLoop implements RunLoop {
    /** Pointer to the C run loop. */
    private long mRunLoopID;

    private final CoreImpl mCore;

    BaseRunLoop(CoreImpl core) {
        this.mCore = core;
        this.mRunLoopID = BaseRunLoopJni.get().createBaseRunLoop();
    }

    @Override
    public void run() {
        assert mRunLoopID != 0 : "The run loop cannot run once closed";
        BaseRunLoopJni.get().run();
    }

    @Override
    public void runUntilIdle() {
        assert mRunLoopID != 0 : "The run loop cannot run once closed";
        BaseRunLoopJni.get().runUntilIdle();
    }

    @Override
    public void postDelayedTask(Runnable runnable, long delay) {
        assert mRunLoopID != 0 : "The run loop cannot run tasks once closed";
        BaseRunLoopJni.get().postDelayedTask(mRunLoopID, runnable, delay);
    }

    @Override
    public void close() {
        if (mRunLoopID == 0) {
            return;
        }
        // We don't want to de-register a different run loop!
        assert mCore.getCurrentRunLoop() == this : "Only the current run loop can be closed";
        mCore.clearCurrentRunLoop();
        BaseRunLoopJni.get().deleteMessageLoop(mRunLoopID);
        mRunLoopID = 0;
    }

    @CalledByNative
    private static void runRunnable(Runnable runnable) {
        runnable.run();
    }

    @NativeMethods
    interface Natives {
        long createBaseRunLoop();

        void run();

        void runUntilIdle();

        void postDelayedTask(long runLoopID, Runnable runnable, long delay);

        void deleteMessageLoop(long runLoopID);
    }
}
