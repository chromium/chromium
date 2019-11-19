// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.mojo.system.RunLoop;

/**
 * Implementation of {@link RunLoop} suitable for the base:: message loop implementation.
 */
@JNINamespace("mojo::android")
class BaseRunLoop implements RunLoop {
    /**
     * Pointer to the C run loop.
     */
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
    public void quit() {
        assert mRunLoopID != 0 : "The run loop cannot be quitted run once closed";
        BaseRunLoopJni.get().quit(BaseRunLoop.this);
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
        void quit(BaseRunLoop caller);
        void postDelayedTask(BaseRunLoop caller, long runLoopID, Runnable runnable, long delay);
        void deleteMessageLoop(BaseRunLoop caller, long runLoopID);
    }
}
