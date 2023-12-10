// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MessagePipeHandle.ReadMessageResult;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.ResultAnd;
import org.chromium.mojo.system.Watcher;
import org.chromium.mojo.system.Watcher.Callback;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * A factory which provides per-thread executors, which enable execution on the thread from which
 * they were obtained.
 */
public class ExecutorFactory {
    /**
     * A null buffer which is used to send messages without any data on the PipedExecutor's
     * signaling handles.
     */
    private static final ByteBuffer NOTIFY_BUFFER = null;

    /**
     * Implementation of the executor which uses a pair of {@link MessagePipeHandle} for signaling.
     * The executor will wait asynchronously on one end of a {@link MessagePipeHandle} on the thread
     * on which it was created. Other threads can call execute with a {@link Runnable}, and the
     * executor will queue the {@link Runnable} and write a message on the other end of the handle.
     * This will wake up the executor which is waiting on the handle, which will then dequeue the
     * {@link Runnable} and execute it on the original thread.
     */
    private static class PipedExecutor implements Executor, Callback {

        /** The handle which is written to. Access to this object must be protected with |mLock|. */
        private final MessagePipeHandle mWriteHandle;

        /** The handle which is read from. */
        private final MessagePipeHandle mReadHandle;

        /**
         * The list of actions left to be run. Access to this object must be protected with |mLock|.
         */
        private final List<Runnable> mPendingActions;

        /** Lock protecting access to |mWriteHandle| and |mPendingActions|. */
        private final Object mLock;

        /** The {@link Watcher} to get notified of new message availability on |mReadHandle|. */
        private final Watcher mWatcher;

        /** Constructor. */
        public PipedExecutor(Core core) {
            mWatcher = core.getWatcher();
            assert mWatcher != null;
            mLock = new Object();
            Pair<MessagePipeHandle, MessagePipeHandle> handles =
                    core.createMessagePipe(new MessagePipeHandle.CreateOptions());
            mReadHandle = handles.first;
            mWriteHandle = handles.second;
            mPendingActions = new ArrayList<Runnable>();
            mWatcher.start(mReadHandle, Core.HandleSignals.READABLE, this);
        }

        /**
         * @see Callback#onResult(int)
         */
        @Override
        public void onResult(int result) {
            if (result == MojoResult.OK && readNotifyBufferMessage()) {
                runNextAction();
            } else {
                close();
            }
        }

        /** Close the handles. Should only be called on the executor thread. */
        private void close() {
            synchronized (mLock) {
                mWriteHandle.close();
                mPendingActions.clear();
            }
            mWatcher.cancel();
            mWatcher.destroy();
            mReadHandle.close();
        }

        /**
         * Read the next message on |mReadHandle|, and return |true| if successful, |false|
         * otherwise.
         */
        private boolean readNotifyBufferMessage() {
            try {
                ResultAnd<ReadMessageResult> readMessageResult =
                        mReadHandle.readMessage(MessagePipeHandle.ReadFlags.NONE);
                if (readMessageResult.getMojoResult() == MojoResult.OK) {
                    return true;
                }
            } catch (MojoException e) {
                // Will be closed by the fall back at the end of this method.
            }
            return false;
        }

        /** Run the next action in the |mPendingActions| queue. */
        private void runNextAction() {
            Runnable toRun = null;
            synchronized (mLock) {
                toRun = mPendingActions.remove(0);
            }
            toRun.run();
        }

        /**
         * Execute the given |command| in the executor thread. This can be called on any thread.
         *
         * @see Executor#execute(Runnable)
         */
        @Override
        public void execute(Runnable command) {
            // Accessing the write handle must be protected by the lock, because it can be closed
            // from the executor's thread.
            synchronized (mLock) {
                if (!mWriteHandle.isValid()) {
                    throw new IllegalStateException(
                            "Trying to execute an action on a closed executor.");
                }
                mPendingActions.add(command);
                mWriteHandle.writeMessage(NOTIFY_BUFFER, null, MessagePipeHandle.WriteFlags.NONE);
            }
        }
    }

    /** Keep one executor per executor thread. */
    private static final ThreadLocal<Executor> EXECUTORS = new ThreadLocal<Executor>();

    /** Returns an {@link Executor} that will run all of its actions in the current thread. */
    public static Executor getExecutorForCurrentThread(Core core) {
        Executor executor = EXECUTORS.get();
        if (executor == null) {
            executor = new PipedExecutor(core);
            EXECUTORS.set(executor);
        }
        return executor;
    }
}
