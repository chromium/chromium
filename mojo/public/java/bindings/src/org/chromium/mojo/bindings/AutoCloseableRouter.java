// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;

import java.util.concurrent.Executor;

/**
 * Wrapper around {@link Router}. Unlike the class name suggested, it doesn't auto-close the
 * connection without a try-with-resources statement. If the callsite isn't using try-with-resources
 * mechanism, it needs to call close() explicitly.
 */
class AutoCloseableRouter implements Router {
    /** The underlying router. */
    private final Router mRouter;

    /** The executor to close the underlying router. */
    private final Executor mExecutor;

    /**
     * Exception used to track AutoCloseableRouter's allocation location for debugging puproses when
     * leaked.
     */
    private final Exception mAllocationException;

    /** Flags to keep track if this router has been correctly closed. */
    private boolean mClosed;

    /** Constructor. */
    public AutoCloseableRouter(Core core, Router router) {
        mRouter = router;
        mExecutor = ExecutorFactory.getExecutorForCurrentThread(core);
        mAllocationException = new Exception("AutocloseableRouter allocated at:");
    }

    /**
     * @see Router#setIncomingMessageReceiver(MessageReceiverWithResponder)
     */
    @Override
    public void setIncomingMessageReceiver(MessageReceiverWithResponder incomingMessageReceiver) {
        mRouter.setIncomingMessageReceiver(incomingMessageReceiver);
    }

    /**
     * @see HandleOwner#passHandle()
     */
    @Override
    public MessagePipeHandle passHandle() {
        return mRouter.passHandle();
    }

    /**
     * @see MessageReceiver#accept(Message)
     */
    @Override
    public boolean accept(Message message) {
        return mRouter.accept(message);
    }

    /**
     * @see MessageReceiverWithResponder#acceptWithResponder(Message, MessageReceiver)
     */
    @Override
    public boolean acceptWithResponder(Message message, MessageReceiver responder) {
        return mRouter.acceptWithResponder(message, responder);
    }

    /**
     * @see Router#start()
     */
    @Override
    public void start() {
        mRouter.start();
    }

    /**
     * @see Router#setErrorHandler(ConnectionErrorHandler)
     */
    @Override
    public void setErrorHandler(ConnectionErrorHandler errorHandler) {
        mRouter.setErrorHandler(errorHandler);
    }

    /**
     * @see java.io.Closeable#close()
     */
    @Override
    public void close() {
        mRouter.close();
        mClosed = true;
    }

    /**
     * @see Object#finalize()
     */
    @Override
    @SuppressWarnings("Finalize") // TODO(crbug.com/40286193): Use LifetimeAssert instead.
    protected void finalize() throws Throwable {
        if (!mClosed) {
            mExecutor.execute(
                    new Runnable() {

                        @Override
                        public void run() {
                            close();
                        }
                    });
            throw new IllegalStateException(
                    "Warning: Router objects should be explicitly closed "
                            + "when no longer required otherwise you may leak handles.",
                    mAllocationException);
        }
        super.finalize();
    }
}
