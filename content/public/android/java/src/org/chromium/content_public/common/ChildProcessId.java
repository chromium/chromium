// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Type-safe wrapper for child process IDs on the Java side,
 * providing similar safety guards to C++ base::IdType.
 */
@JNINamespace("content")
@NullMarked
@DoNotMock("This is a simple value object.")
public final class ChildProcessId {
    private final int mProcessId;

    @CalledByNative
    private ChildProcessId(int processId) {
        mProcessId = processId;
    }

    @CalledByNative
    private int getProcessIdForSerialization() {
        return mProcessId;
    }

    /**
     * @return Whether this is a valid, initialized child process ID.
     */
    public boolean isValid() {
        return mProcessId > 0;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof ChildProcessId)) {
            return false;
        }
        return mProcessId == ((ChildProcessId) obj).mProcessId;
    }

    @Override
    public int hashCode() {
        return mProcessId;
    }
}
