// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.mojo.system.RunLoop;

/** Implementation of {@link RunLoop} suitable for the base:: message loop implementation. */
@JNINamespace("mojo::android")
class BaseRunLoop implements RunLoop {
    /** Pointer to the C run loop. */
    private long mRunLoopID;

    private final CoreImpl mCore;

    BaseRunLoop(CoreImpl core) {
        this.mCore = core;
        this.mRunLoopID = BaseRunLoopJni.get().createBaseRunLoop(BaseRunLoop.this);
    }

    @Override
    public void run() {
        assert mRunLoopID != 0 : "The run loop cannot run once closed";
        BaseRunLoopJni.get().run(BaseRunLoop.this);
    }

    @Override
    public void runUntilIdle() {
        assert mRunLoopID != 0 : "The run loop cannot run once closed";
        BaseRunLoopJni.get().runUntilIdle(BaseRunLoop.this);
    }

    @Override
    public void postDelayedTask(Runnable runnable, long delay) {
        assert mRunLoopID != 0 : "The run loop cannot run tasks once closed";
        BaseRunLoopJni.get().postDelayedTask(BaseRunLoop.this, mRunLoopID, runnable, delay);
    }

    @Override
    public void close() {
        if (mRunLoopID == 0) {
            return;
        }
        // We don't want to de-register a different run loop!
        assert mCore.getCurrentRunLoop() == this : "Only the current run loop can be closed";
        mCore.clearCurrentRunLoop();
        BaseRunLoopJni.get().deleteMessageLoop(BaseRunLoop.this, mRunLoopID);
        mRunLoopID = 0;
    }

    @CalledByNative
    private static void runRunnable(Runnable runnable) {
        runnable.run();
    }

    @NativeMethods
    interface Natives {
        long createBaseRunLoop(BaseRunLoop caller);

        void run(BaseRunLoop caller);

        void runUntilIdle(BaseRunLoop caller);

        void postDelayedTask(BaseRunLoop caller, long runLoopID, Runnable runnable, long delay);

        void deleteMessageLoop(BaseRunLoop caller, long runLoopID);
    }
}
