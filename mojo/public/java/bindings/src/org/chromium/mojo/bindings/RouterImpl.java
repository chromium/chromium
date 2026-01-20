// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.annotation.SuppressLint;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.Watcher;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.Executor;

/** Implementation of {@link Router}. */
@NullMarked
@SuppressLint("UseSparseArrays") // https://crbug.com/600699
public class RouterImpl implements Router {

    /** {@link MessageReceiver} used as the {@link Connector} callback. */
    private class HandleIncomingMessageThunk implements MessageReceiver {

        /**
         * @see MessageReceiver#accept(Message)
         */
        @Override
        public boolean accept(Message message) throws BadMessageException {
            return handleIncomingMessage(message);
        }

        /**
         * @see MessageReceiver#close()
         */
        @Override
        public void close() {
            handleConnectorClose();
        }
    }

    /**
     *
     * {@link MessageReceiver} used to return responses to the caller.
     */
    class ResponderThunk implements MessageReceiver {
        private boolean mAcceptWasInvoked;

        /**
         * @see MessageReceiver#accept(Message)
         */
        @Override
        public boolean accept(Message message) throws BadMessageException {
            mAcceptWasInvoked = true;
            return RouterImpl.this.accept(message);
        }

        /**
         * @see MessageReceiver#close()
         */
        @Override
        public void close() {
            RouterImpl.this.close();
        }

        @Override
        @SuppressWarnings("Finalize") // TODO(crbug.com/40286193): Use LifetimeAssert instead.
        protected void finalize() throws Throwable {
            if (!mAcceptWasInvoked) {
                // We close the pipe here as a way of signaling to the calling application that an
                // error condition occurred. Without this the calling application would have no
                // way of knowing it should stop waiting for a response.
                RouterImpl.this.closeOnHandleThread();
            }
            super.finalize();
        }
    }

    /** The {@link Connector} which is connected to the handle. */
    private final Connector mConnector;

    /**
     * The {@link MessageReceiverWithResponder} that will consume the messages received from the
     * pipe.
     */
    private final Map<Integer, Stub> mStubs = new HashMap();

    /** The next id to use for a request id which needs a response. It is auto-incremented. */
    private long mNextRequestId = 1;

    /** The map from request ids to {@link MessageReceiver} of request currently in flight. */
    private final Map<Long, MessageReceiver> mResponders = new HashMap<Long, MessageReceiver>();

    /** A list of messages that cannot be dispatched yet. */
    private final Queue<Message> mEnqueuedMessages = new ArrayDeque<Message>();

    /**
     * An Executor that will run on the thread associated with the MessagePipe to which this Router
     * is bound. This may be {@code Null} if the MessagePipeHandle passed in to the constructor is
     * not valid.
     */
    private final @Nullable Executor mExecutor;

    /** Constructor that will use the default {@link Watcher}. */
    public RouterImpl(MessagePipeHandle messagePipeHandle) {
        this(messagePipeHandle, BindingsHelper.getWatcherForHandleNonNull(messagePipeHandle));
    }

    /**
     * Constructor.
     *
     * @param messagePipeHandle The {@link MessagePipeHandle} to route message for.
     * @param watcher the {@link Watcher} to use to get notification of new messages on the
     *            handle.
     */
    public RouterImpl(MessagePipeHandle messagePipeHandle, Watcher watcher) {
        mConnector = new Connector(messagePipeHandle, watcher);
        mConnector.setIncomingMessageReceiver(new HandleIncomingMessageThunk());
        Core core = messagePipeHandle.getCore();
        if (core != null) {
            mExecutor = ExecutorFactory.getExecutorForCurrentThread(core);
        } else {
            mExecutor = null;
        }
    }

    /**
     * @see org.chromium.mojo.bindings.Router#start()
     */
    @Override
    public void start() {
        mConnector.start();
    }

    @Override
    public void setPrimaryStub(Stub primaryStub) throws BadMessageException {
        if (primaryStub.getInterfaceId() != PRIMARY_INTERFACE_ID) {
            throw new IllegalArgumentException("primary stub must have an interface id of 0");
        }
        addStub(primaryStub);
    }

    private void addStub(Stub stub) throws BadMessageException {
        this.mStubs.put(stub.getInterfaceId(), stub);
        // Adding a new stub could allow some enqueued messages to be dispatched.
        this.dispatchMessages();
    }

    /**
     * @see MessageReceiver#accept(Message)
     */
    @Override
    public boolean accept(Message message) throws BadMessageException {
        // A message without responder is directly forwarded to the connector.
        return mConnector.accept(message);
    }

    /**
     * @see MessageReceiverWithResponder#acceptWithResponder(Message, MessageReceiver)
     */
    @Override
    public boolean acceptWithResponder(Message message, MessageReceiver responder)
            throws BadMessageException {
        // The message must have a header.
        ServiceMessage messageWithHeader = message.asServiceMessage();
        // Checking the message expects a response.
        assert messageWithHeader.getHeader().hasFlag(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG);

        // Compute a request id for being able to route the response.
        long requestId = mNextRequestId++;
        // Reserve 0 in case we want it to convey special meaning in the future.
        if (requestId == 0) {
            requestId = mNextRequestId++;
        }
        if (mResponders.containsKey(requestId)) {
            throw new IllegalStateException("Unable to find a new request identifier.");
        }
        messageWithHeader.setRequestId(requestId);
        if (!mConnector.accept(messageWithHeader)) {
            return false;
        }
        // Only keep the responder is the message has been accepted.
        mResponders.put(requestId, responder);
        return true;
    }

    /**
     * @see org.chromium.mojo.bindings.HandleOwner#passHandle()
     */
    @Override
    public MessagePipeHandle passHandle() {
        return mConnector.passHandle();
    }

    /**
     * @see java.io.Closeable#close()
     */
    @Override
    public void close() {
        mConnector.close();
    }

    /**
     * @see Router#setErrorHandler(ConnectionErrorHandler)
     */
    @Override
    public void setErrorHandler(ConnectionErrorHandler errorHandler) {
        mConnector.setErrorHandler(errorHandler);
    }

    /** Receive a message from the connector. Returns |true| if the message has been handled. */
    private boolean handleIncomingMessage(Message message) throws BadMessageException {
        mEnqueuedMessages.add(message);
        return dispatchMessages();
    }

    // TODO(crbug.com/469861566): Clean up the logic here. This is quite complicated, because the
    // original documentation for unhandled messages does not match the actual behaviour. In the
    // original impl, unhandled messages are dropped (but the pipe remains open). In actuality,
    // a false return in message receiver will cause the pipe to close, because the connector
    // will close the pipe for any MojoResult that isn't a SHOULD_WAIT.
    // This is not ideal because things like Proxy will always return true, which means a bad
    // might never closer a pipe in that case. Instead, pipe closure should be handled at a
    // higher level that the pipe read method (ie: it shouldn't assume that a read result that
    // isn't a SHOULD_WAIT must mean that the pipe needs to be closed).
    private boolean dispatchMessages() throws BadMessageException {
        while (!mEnqueuedMessages.isEmpty()) {
            var message = mEnqueuedMessages.element();
            var result = dispatchMessage(message);
            if (result == DispatchResult.NOT_YET_ABLE_TO_DISPATCH) {
                break;
            }

            mEnqueuedMessages.remove();
            if (result == DispatchResult.DISPATCHED_AND_FAILED_TO_HANDLE) {
                return false;
            }
            assert result == DispatchResult.DISPATCHED_AND_SUCCESSFULLY_HANDLED;
        }
        return true;
    }

    @IntDef({
        DispatchResult.DISPATCHED_AND_SUCCESSFULLY_HANDLED,
        DispatchResult.DISPATCHED_AND_FAILED_TO_HANDLE,
        DispatchResult.NOT_YET_ABLE_TO_DISPATCH,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface DispatchResult {
        int DISPATCHED_AND_SUCCESSFULLY_HANDLED = 0;
        int DISPATCHED_AND_FAILED_TO_HANDLE = 1;
        int NOT_YET_ABLE_TO_DISPATCH = 2;
    }

    private int dispatchMessage(Message message) throws BadMessageException {
        MessageHeader header = message.asServiceMessage().getHeader();

        var stub = mStubs.get(header.getInterfaceId());
        if (header.hasFlag(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG)) {
            if (stub != null) {
                var status = stub.acceptWithResponder(message, new ResponderThunk());
                return statusToResult(status);
            }
            // Not yet ready to handle the message.
            return DispatchResult.NOT_YET_ABLE_TO_DISPATCH;
        } else if (header.hasFlag(MessageHeader.MESSAGE_IS_RESPONSE_FLAG)) {
            long requestId = header.getRequestId();
            MessageReceiver responder = mResponders.get(requestId);
            if (responder == null) {
                throw new BadMessageException(
                        "no responder for the given request id: " + requestId);
            }
            mResponders.remove(requestId);

            var status = responder.accept(message);
            return statusToResult(status);
        } else {
            if (stub != null) {
                var status = stub.accept(message);
                return statusToResult(status);
            }
            // Not yet ready to handle the message.
            return DispatchResult.NOT_YET_ABLE_TO_DISPATCH;
        }
    }

    private static int statusToResult(boolean success) {
        return success
                ? DispatchResult.DISPATCHED_AND_SUCCESSFULLY_HANDLED
                : DispatchResult.DISPATCHED_AND_FAILED_TO_HANDLE;
    }

    private void handleConnectorClose() {
        var primaryStub = mStubs.get(PRIMARY_INTERFACE_ID);
        if (primaryStub != null) {
            primaryStub.close();
        }
    }

    /**
     * Invokes {@link #close()} asynchronously on the thread associated with
     * this Router's Handle. If this Router was constructed with an invalid
     * handle then this method does nothing.
     */
    private void closeOnHandleThread() {
        if (mExecutor != null) {
            mExecutor.execute(
                    new Runnable() {

                        @Override
                        public void run() {
                            close();
                        }
                    });
        }
    }
}
