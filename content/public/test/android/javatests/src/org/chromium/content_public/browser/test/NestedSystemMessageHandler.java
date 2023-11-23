// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import android.os.Handler;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.test.util.LooperUtils;

import java.lang.reflect.InvocationTargetException;

/**
 * Handles processing messages in nested run loops.
 *
 * Android does not support nested run loops by default. While running
 * in nested mode, we use reflection to retreive messages from the MessageQueue
 * and dispatch them.
 */
@JNINamespace("content")
public class NestedSystemMessageHandler {
    private static final int QUIT_MESSAGE = 10;
    private static final Handler sHandler = new Handler();

    private NestedSystemMessageHandler() {}

    /**
     * Dispatches the first message from the current MessageQueue, blocking
     * until a task becomes available if the queue is empty. Callbacks for
     * other event handlers registered to the thread's looper (e.g.,
     * MessagePumpAndroid) may also be processed as a side-effect.
     *
     * Returns true if task dispatching succeeded, or false if an exception was
     * thrown.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static boolean dispatchOneMessage() {
        try {
            LooperUtils.runSingleNestedLooperTask();
        } catch (IllegalArgumentException
                | IllegalAccessException
                | SecurityException
                | InvocationTargetException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    /*
     * Causes a previous call to dispatchOneMessage() to stop blocking and
     * return.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void postQuitMessage() {
        // Causes MessageQueue.next() to return in case it was blocking waiting
        // for more messages.
        sHandler.sendMessage(sHandler.obtainMessage(QUIT_MESSAGE));
    }
}
