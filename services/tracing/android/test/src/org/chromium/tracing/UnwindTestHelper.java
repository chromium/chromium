// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tracing;

import android.os.ConditionVariable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Helper to run code through JNI layer to test JNI unwinding. */
@JNINamespace("tracing")
public final class UnwindTestHelper {
    private static final ConditionVariable sBlock = new ConditionVariable();

    @CalledByNative
    public static void blockCurrentThread() {
        sBlock.block();
        sBlock.close();
    }

    @CalledByNative
    public static void unblockAllThreads() {
        sBlock.open();
    }
}
