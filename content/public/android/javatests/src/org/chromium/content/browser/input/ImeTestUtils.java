// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Handler;
import android.os.Looper;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/** Utilities for testing IME (input method editor). */
public class ImeTestUtils {
    private static final long MAX_WAIT_TIME_MILLIS = 5000;

    public static <T> T runBlockingOnHandlerNoException(Handler handler, Callable<T> callable) {
        try {
            return runBlockingOnHandler(handler, callable, MAX_WAIT_TIME_MILLIS);
        } catch (Exception e) {
            throw new RuntimeException("Error occurred waiting for callable", e);
        }
    }

    public static <T> T runBlockingOnHandler(Handler handler, Callable<T> callable)
            throws Exception {
        return runBlockingOnHandler(handler, callable, MAX_WAIT_TIME_MILLIS);
    }

    public static <T> T runBlockingOnHandler(
            Handler handler, Callable<T> callable, long waitTimeMillis) throws Exception {
        if (handler.getLooper() == Looper.myLooper()) {
            return callable.call();
        } else {
            FutureTask<T> task = new FutureTask<T>(callable);
            handler.post(task);
            return task.get(waitTimeMillis, TimeUnit.MILLISECONDS);
        }
    }
}
