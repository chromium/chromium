// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Pair;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

/**
 * Represents the MessageChannel MessagePort object. Inspired from
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/web-messaging.html#message-channels
 *
 * State management:
 *
 * A message port can be in transferred state while a transfer is pending or complete. An
 * application cannot use a transferred port to post messages. If a transferred port
 * receives messages, they will be queued. This state is not visible to embedder app.
 *
 * A message port should be closed by the app when it is not needed any more. This will free
 * any resources used by it. A closed port cannot receive/send messages and cannot be transferred.
 * close() can be called multiple times. A transferred port cannot be closed by the application,
 * since the ownership is also transferred during the transfer. Closing a transferred port will
 * throw an exception.
 *
 * All methods are called on the UI thread, except for MessageHandler.handleMessage, which is
 * used to dispatch messages on a potentially separate thread.
 *
 * Restrictions:
 * The HTML5 message protocol is very flexible in transferring ports. However, this
 * sometimes leads to surprising behavior. For example, in current version of chrome (m41)
 * the code below
 *  1.  var c1 = new MessageChannel();
 *  2.  var c2 = new MessageChannel();
 *  3.  c1.port2.onmessage = function(e) { console.log("1"); }
 *  4.  c2.port2.onmessage = function(e) {
 *  5.     e.ports[0].onmessage = function(f) {
 *  6.          console.log("3");
 *  7.      }
 *  8.  }
 *  9.  c1.port1.postMessage("test");
 *  10. c2.port1.postMessage("test2",[c1.port2])
 *
 * prints 1 or 3 depending on whether or not line 10 is included in code. Further if
 * it gets executed with a timeout, depending on timeout value, the printout value
 * changes.
 *
 * To prevent such problems, this implementation limits the transfer of ports
 * as below:
 * A port is put to a "started" state if:
 * 1. The port is ever used to post a message, or
 * 2. The port was ever registered a handler to receive a message.
 * A started port cannot be transferred.
 *
 * This restriction should not impact postmessage functionality in a big way,
 * because an app can still create as many channels as it wants to and use it for
 * transferring data. As a return, it simplifies implementation and prevents hard
 * to debug, racy corner cases while receiving/sending data.
 *
 * This object is not thread safe but public methods may be called from any thread.
 */
@JNINamespace("content::android")
public class AppWebMessagePort implements MessagePort {
    private static final String TAG = "AppWebMessagePort";

    private static class MessageHandler extends Handler {
        // The |what| value for handleMessage.
        private static final int MESSAGE_RECEIVED = 1;

        @NonNull private final MessageCallback mMessageCallback;

        MessageHandler(@NonNull MessageCallback callback, @Nullable Handler handler) {
            super(handler == null ? Looper.getMainLooper() : handler.getLooper());
            mMessageCallback = callback;
        }

        @Override
        public void handleMessage(@NonNull final Message msg) {
            if (msg.what == MESSAGE_RECEIVED) {
                final Pair<MessagePayload, MessagePort[]> obj =
                        (Pair<MessagePayload, MessagePort[]>) msg.obj;
                mMessageCallback.onMessage(obj.first, obj.second);
                return;
            }
            throw new IllegalStateException("undefined message");
        }

        @MainThread
        public void onMessage(final MessagePayload messagePayload, final MessagePort[] sentPorts) {
            ThreadUtils.assertOnUiThread();
            sendMessage(obtainMessage(MESSAGE_RECEIVED, Pair.create(messagePayload, sentPorts)));
        }
    }

    // Accessed on UI thread only.
    private long mNativeAppWebMessagePort;
    private MessageHandler mMessageHandler;

    // Can be accessed from any thread, client needs to keep thread safe. Need volatile since they
    // may be accessed concurrently from UI thread and client thread, which may be different.
    private volatile boolean mClosed;
    private volatile boolean mTransferred;
    private volatile boolean mStarted;

    @MainThread
    @CalledByNative
    private AppWebMessagePort(long nativeAppWebMessagePort) {
        mNativeAppWebMessagePort = nativeAppWebMessagePort;
    }

    // Called to create an entangled pair of ports.
    @MainThread
    public static AppWebMessagePort[] createPair() {
        return AppWebMessagePortJni.get().createPair();
    }

    @Override
    public void close() {
        if (isTransferred()) {
            throw new IllegalStateException("Port is already transferred");
        }
        if (isClosed()) return;
        mClosed = true;
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeAppWebMessagePort == 0L) return;
                    AppWebMessagePortJni.get().closeAndDestroy(mNativeAppWebMessagePort);
                });
    }

    @Override
    public boolean isClosed() {
        return mClosed;
    }

    @Override
    public boolean isTransferred() {
        return mTransferred;
    }

    @Override
    public boolean isStarted() {
        return mStarted;
    }

    @Override
    public void setMessageCallback(MessageCallback messageCallback, Handler handler) {
        if (isClosed() || isTransferred()) {
            throw new IllegalStateException("Port is already closed or transferred");
        }
        mStarted = true;
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeAppWebMessagePort == 0L) return;
                    mMessageHandler =
                            messageCallback == null
                                    ? null
                                    : new MessageHandler(messageCallback, handler);
                    AppWebMessagePortJni.get()
                            .setShouldReceiveMessages(
                                    mNativeAppWebMessagePort, messageCallback != null);
                });
    }

    @Override
    public void postMessage(MessagePayload messagePayload, MessagePort[] sentPorts)
            throws IllegalStateException {
        if (isClosed() || isTransferred()) {
            throw new IllegalStateException("Port is already closed or transferred");
        }
        if (sentPorts != null) {
            for (MessagePort port : sentPorts) {
                if (port.equals(this)) {
                    throw new IllegalStateException("Source port cannot be transferred");
                }
                if (port.isClosed() || port.isTransferred()) {
                    throw new IllegalStateException("Port is already closed or transferred");
                }
                if (port.isStarted()) {
                    throw new IllegalStateException("Port is already started");
                }
                // It's safe to cast since AppWebMessagePort is the only impl.
                // Port may be transferred over other MessageChannels (Like
                // WebContents#PostMessageToMainFrame).
                ((AppWebMessagePort) port).setTransferred();
            }
        }
        mStarted = true;
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeAppWebMessagePort == 0L) return;
                    AppWebMessagePortJni.get()
                            .postMessage(mNativeAppWebMessagePort, messagePayload, sentPorts);
                });
    }

    /**
     * A finalizer is required to ensure that the native object associated with this descriptor gets
     * torn down, otherwise there would be a memory leak.
     *
     * <p>This is safe because posting a task is fast.
     *
     * <p>TODO(chrisha): Chase down the existing offenders that don't call close, and flip this to
     * use LifetimeAssert. (also: https://crbug.com/40286193)
     *
     * @see java.lang.Object#finalize()
     */
    @Override
    @SuppressWarnings("Finalize")
    protected void finalize() throws Throwable {
        try {
            if (mNativeAppWebMessagePort == 0L) return;
            Log.d(TAG, "AppWebMessagePort was not closed before finalization");
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (mNativeAppWebMessagePort == 0L) return;
                        mClosed = true;
                        AppWebMessagePortJni.get().closeAndDestroy(mNativeAppWebMessagePort);
                    });
        } finally {
            super.finalize();
        }
    }

    @MainThread
    @CalledByNative
    private long getNativeObj() {
        ThreadUtils.assertOnUiThread();
        assert mNativeAppWebMessagePort != 0L;
        return mNativeAppWebMessagePort;
    }

    @MainThread
    @CalledByNative
    private void onMessage(@NonNull MessagePayload payload, @Nullable MessagePort[] ports) {
        ThreadUtils.assertOnUiThread();
        if (mMessageHandler != null) {
            mMessageHandler.onMessage(payload, ports);
        } else {
            // Their will be a case that the Java listener is cleared, but listeners in C++ is not
            // cleared yet. We can safely ignore those messages and close the ports to avoid
            // relaying on GC.
            if (ports != null) {
                for (final MessagePort port : ports) {
                    port.close();
                }
            }
        }
    }

    // Called when native object is destroyed.
    @MainThread
    @CalledByNative
    private void nativeDestroyed() {
        ThreadUtils.assertOnUiThread();
        assert mNativeAppWebMessagePort != 0L;
        // When calling this method, the port must be closed or transferred.
        assert mClosed || mTransferred;
        mNativeAppWebMessagePort = 0L;
    }

    // Called when MessagePort is transferred. The native object will be destroyed later.
    @CalledByNative
    private void setTransferred() {
        assert !mStarted;
        mTransferred = true;
    }

    @NativeMethods
    @MainThread
    interface Natives {
        @NonNull
        AppWebMessagePort[] createPair();

        void postMessage(
                long nativeAppWebMessagePort,
                MessagePayload messagePayload,
                MessagePort[] sentPorts);

        void setShouldReceiveMessages(long nativeAppWebMessagePort, boolean shouldReceiveMessage);

        void closeAndDestroy(long nativeAppWebMessagePort);
    }
}
