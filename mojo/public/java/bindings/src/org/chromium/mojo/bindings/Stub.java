// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.mojo.bindings.interfacecontrol.InterfaceControlMessagesConstants;
import org.chromium.mojo.system.Core;

/**
 * Base implementation of Stub. Stubs are message receivers that deserialize the payload and call
 * the appropriate method in the implementation. If the method returns `result`, the stub serializes
 * the response and sends it back.
 *
 * @param <I> the type of the interface to delegate calls to.
 */
@NullMarked
public abstract class Stub<I extends Interface> implements MessageReceiverWithResponder {

    /** The {@link Core} implementation to use. */
    private final Core mCore;

    /** The implementation to delegate calls to. */
    private final I mImpl;

    /** The interface id associated with this stub. */
    private final int mInterfaceId;

    /** The version of the interface (as defined in the .mojom). */
    private final int mVersion;

    /**
     * Constructor.
     *
     * @param core the {@link Core} implementation to use.
     * @param implementation which will receive requests.
     * @param interfaceId the id that is associated with this stub.
     */
    public Stub(Core core, I impl, int interfaceId) {
        this(core, impl, interfaceId, 0);
    }

    /**
     * Constructor.
     *
     * @param core the {@link Core} implementation to use.
     * @param impl the implementation which will receive requests.
     * @param interfaceId the id that is associated with this stub.
     * @param version the version of the interface.
     */
    protected Stub(Core core, I impl, int interfaceId, int version) {
        mCore = core;
        mImpl = impl;
        mInterfaceId = interfaceId;
        mVersion = version;
    }

    /** Returns the Core implementation. */
    protected Core getCore() {
        return mCore;
    }

    /** Returns the implementation to delegate calls to. */
    protected I getImpl() {
        return mImpl;
    }

    /** Gets the interface id. */
    public int getInterfaceId() {
        return mInterfaceId;
    }

    @Override
    public void close() {
        getImpl().close();
    }

    @Override
    public boolean accept(Message message) throws BadMessageException {
        try {
            ServiceMessage messageWithHeader = message.asServiceMessage();
            MessageHeader header = messageWithHeader.getHeader();
            validateFlags(header, MessageHeader.NO_FLAG);
            int methodId = header.getMethodId();
            if (methodId == InterfaceControlMessagesConstants.RUN_OR_CLOSE_PIPE_MESSAGE_ID) {
                return InterfaceControlMessagesHelper.handleRunOrClosePipe(
                        mVersion, messageWithHeader);
            }
            if (!handleMessage(methodId, new Decoder(messageWithHeader.getPayload()))) {
                throw newUnknownMethodException(methodId);
            }
            return true;
        } catch (DeserializationException e) {
            throw new UnmarshalException(e);
        }
    }

    @Override
    public boolean acceptWithResponder(Message message, MessageReceiver receiver)
            throws BadMessageException {
        try {
            ServiceMessage messageWithHeader = message.asServiceMessage();
            MessageHeader header = messageWithHeader.getHeader();
            validateFlags(header, MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG);
            int methodId = header.getMethodId();
            if (methodId == InterfaceControlMessagesConstants.RUN_MESSAGE_ID) {
                return InterfaceControlMessagesHelper.handleRun(
                        getCore(), mVersion, messageWithHeader, receiver);
            }
            Decoder decoder = new Decoder(messageWithHeader.getPayload());
            if (!handleMessageWithResponder(methodId, decoder, receiver, header.getRequestId())) {
                throw newUnknownMethodException(methodId);
            }
            return true;
        } catch (DeserializationException e) {
            throw new UnmarshalException(e);
        }
    }

    /**
     * Deserializes and dispatches a message for a method without a response.
     *
     * @return false if |methodId| is not a method without a response.
     */
    protected boolean handleMessage(int methodId, Decoder decoder) {
        return false;
    }

    /**
     * Deserializes and dispatches a message for a method with a response.
     *
     * @return false if |methodId| is not a method with a response.
     */
    protected boolean handleMessageWithResponder(
            int methodId, Decoder decoder, MessageReceiver receiver, long requestId) {
        return false;
    }

    private static void validateFlags(MessageHeader header, int requiredFlags)
            throws InvalidHeaderException {
        int flags = requiredFlags;
        if (header.hasFlag(MessageHeader.MESSAGE_IS_SYNC_FLAG)) {
            flags |= MessageHeader.MESSAGE_IS_SYNC_FLAG;
        }
        if (!header.hasExactFlags(flags)) {
            throw new InvalidHeaderException(
                    "expected flags: " + flags + ", but got: " + header.getFlags());
        }
    }

    private static UnknownMethodException newUnknownMethodException(int methodId) {
        return new UnknownMethodException("unknown method: " + methodId);
    }
}
