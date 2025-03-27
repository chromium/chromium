// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A helper class to return outcomes to C++ that either hold a Java object when successful, or an
 * exception when failed.
 *
 * @param <T> the type of result. Could be {@link Void} to hold result without content.
 */
@JNINamespace("device")
@NullMarked
public class Outcome<T> {
    private @Nullable T mResult;
    private @Nullable Exception mException;

    /** Creates an Outcome with the given result. */
    Outcome(@Nullable T result) {
        mResult = result;
    }

    /** Creates an Outcome with the given exception. */
    Outcome(Exception exception) {
        assert exception != null;
        mException = exception;
    }

    /** Returns {@code true} if successful, or {@code false} if failed. */
    @CalledByNative
    public boolean isSuccessful() {
        return mException == null;
    }

    /** Returns the result if it is a successful outcome. */
    @CalledByNative
    @Nullable
    public Object getResult() {
        return mResult;
    }

    /** Returns the result if it is a successful outcome. */
    @CalledByNative
    public int getIntResult() {
        assert mResult instanceof Integer;
        return (Integer) mResult;
    }

    /** Returns the exception message if it is a failed outcome. */
    @CalledByNative
    @JniType("std::string")
    public String getExceptionMessage() {
        assert mException != null;
        String exceptionMessage = mException.getMessage();
        if (exceptionMessage != null) {
            return exceptionMessage;
        } else {
            return mException.getClass().getName();
        }
    }
}
