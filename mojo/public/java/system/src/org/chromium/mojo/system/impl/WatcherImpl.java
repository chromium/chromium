// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Watcher;

@JNINamespace("mojo::android")
class WatcherImpl implements Watcher {
    private long mImplPtr = WatcherImplJni.get().createWatcher(WatcherImpl.this);
    private Callback mCallback;

    @Override
    public int start(Handle handle, Core.HandleSignals signals, Callback callback) {
        if (mImplPtr == 0) {
            return MojoResult.INVALID_ARGUMENT;
        }
        if (!(handle instanceof HandleBase)) {
            return MojoResult.INVALID_ARGUMENT;
        }
        int result = WatcherImplJni.get().start(WatcherImpl.this, mImplPtr,
                ((HandleBase) handle).getMojoHandle(), signals.getFlags());
        if (result == MojoResult.OK) mCallback = callback;
        return result;
    }

    @Override
    public void cancel() {
        if (mImplPtr == 0) {
            return;
        }
        mCallback = null;
        WatcherImplJni.get().cancel(WatcherImpl.this, mImplPtr);
    }

    @Override
    public void destroy() {
        if (mImplPtr == 0) {
            return;
        }
        WatcherImplJni.get().delete(WatcherImpl.this, mImplPtr);
        mImplPtr = 0;
    }

    @CalledByNative
    private void onHandleReady(int result) {
        mCallback.onResult(result);
    }

    @NativeMethods
    interface Natives {
        long createWatcher(WatcherImpl caller);
        int start(WatcherImpl caller, long implPtr, int mojoHandle, int flags);
        void cancel(WatcherImpl caller, long implPtr);
        void delete(WatcherImpl caller, long implPtr);
    }
}
