// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.BindingsTestUtils.CapturingErrorHandler;
import org.chromium.mojo.bindings.BindingsTestUtils.RecordingStub;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Core.HandleSignals;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.ResultAnd;
import org.chromium.mojo.system.impl.CoreImpl;

import java.nio.ByteBuffer;
import java.util.ArrayList;

/** Testing {@link Router} */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "relies on mojo global singleton")
public class RouterTest {
    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    private MessagePipeHandle mHandle;
    private Router mRouter;
    private RecordingStub mStub;
    private CapturingErrorHandler mErrorHandler;

    /**
     * @see MojoTestCase#setUp()
     */
    @Before
    public void setUp() throws BadMessageException {
        Core core = CoreImpl.getInstance();
        mStub = new RecordingStub();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        mHandle = handles.first;
        mRouter = new RouterImpl(handles.second);
        mRouter.setPrimaryStub(mStub);
        mErrorHandler = new CapturingErrorHandler();
        mRouter.setErrorHandler(mErrorHandler);
        mRouter.start();
    }

    /** Testing sending a message via the router that expected a response. */
    /** TODO(crbug.com/469861566): add tests which exercises iface routing. */
    @Test
    @SmallTest
    public void testSendingToRouterWithResponse() throws BadMessageException {
        final int requestMethodId = 0xdead;
        final int responseMethodId = 0xbeaf;

        var responseReceiver =
                new MessageReceiver() {
                    public Message received;

                    @Override
                    public boolean accept(Message response) {
                        if (received != null) {
                            throw new RuntimeException("unexpectedly set twice");
                        }
                        received = response;
                        return true;
                    }

                    @Override
                    public void close() {}
                };

        // Sending a message expecting a response.
        MessageHeader header =
                new MessageHeader(
                        MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                        requestMethodId,
                        MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG,
                        0);
        Encoder encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        header.encode(encoder);
        mRouter.acceptWithResponder(encoder.getMessage(), responseReceiver);
        ResultAnd<MessagePipeHandle.ReadMessageResult> result =
                mHandle.readMessage(MessagePipeHandle.ReadFlags.NONE);

        Assert.assertEquals(MojoResult.OK, result.getMojoResult());
        MessageHeader receivedHeader =
                new Message(ByteBuffer.wrap(result.getValue().mData), new ArrayList<Handle>())
                        .asServiceMessage()
                        .getHeader();

        Assert.assertEquals(header.getMethodId(), receivedHeader.getMethodId());
        Assert.assertEquals(header.getFlags(), receivedHeader.getFlags());
        Assert.assertTrue(receivedHeader.getRequestId() != 0);

        // Sending the response.
        MessageHeader responseHeader =
                new MessageHeader(
                        MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                        responseMethodId,
                        MessageHeader.MESSAGE_IS_RESPONSE_FLAG,
                        receivedHeader.getRequestId());
        encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        responseHeader.encode(encoder);
        Message responseMessage = encoder.getMessage();
        mHandle.writeMessage(
                responseMessage.getData(),
                new ArrayList<Handle>(),
                MessagePipeHandle.WriteFlags.NONE);
        mTestRule.runLoopUntilIdle();

        Assert.assertNotNull(responseReceiver.received);
        ServiceMessage receivedResponseMessage = responseReceiver.received.asServiceMessage();
        Assert.assertEquals(
                MessageHeader.MESSAGE_IS_RESPONSE_FLAG,
                receivedResponseMessage.getHeader().getFlags());
        Assert.assertEquals(responseMessage.getData(), receivedResponseMessage.getData());
    }

    /**
     * Sends a message to the Router.
     *
     * @param messageIndex Used when sending multiple messages to indicate the index of this
     *     message.
     * @param requestMethodId The message type to use in the header of the sent message.
     * @param requestId The requestId to use in the header of the sent message.
     */
    private void sendMessageToRouter(int messageIndex, int requestMethodId, int requestId)
            throws BadMessageException {
        MessageHeader header =
                new MessageHeader(
                        MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                        requestMethodId,
                        MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG,
                        requestId);
        Encoder encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        header.encode(encoder);
        Message headerMessage = encoder.getMessage();
        mHandle.writeMessage(
                headerMessage.getData(),
                new ArrayList<Handle>(),
                MessagePipeHandle.WriteFlags.NONE);
        mTestRule.runLoopUntilIdle();

        Assert.assertEquals(messageIndex + 1, mStub.messagesWithReceivers.size());
        Pair<Message, MessageReceiver> receivedMessage =
                mStub.messagesWithReceivers.get(messageIndex);
        Assert.assertEquals(headerMessage.getData(), receivedMessage.first.getData());
    }

    /**
     * Sends a response message from the Router.
     *
     * @param messageIndex Used when sending responses to multiple messages to indicate the index of
     *     the message that this message is a response to.
     * @param responseMethodId The message type to use in the header of the response message.
     */
    private void sendResponseFromRouter(int messageIndex, int responseMethodId)
            throws BadMessageException {
        Pair<Message, MessageReceiver> receivedMessage =
                mStub.messagesWithReceivers.get(messageIndex);

        long requestId = receivedMessage.first.asServiceMessage().getHeader().getRequestId();

        MessageHeader responseHeader =
                new MessageHeader(
                        MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                        responseMethodId,
                        MessageHeader.MESSAGE_IS_RESPONSE_FLAG,
                        requestId);
        Encoder encoder = new Encoder(CoreImpl.getInstance(), responseHeader.getSize());
        responseHeader.encode(encoder);
        Message message = encoder.getMessage();
        receivedMessage.second.accept(message);

        ResultAnd<MessagePipeHandle.ReadMessageResult> result =
                mHandle.readMessage(MessagePipeHandle.ReadFlags.NONE);

        Assert.assertEquals(MojoResult.OK, result.getMojoResult());
        Assert.assertEquals(message.getData(), ByteBuffer.wrap(result.getValue().mData));
    }

    /**
     * Clears {@code mStub.messagesWithReceivers} allowing all message receivers to be finalized.
     *
     * <p>Since there is no way to force the Garbage Collector to actually call finalize and we want
     * to test the effects of the finalize() method, we explicitly call finalize() on all of the
     * message receivers. We do this in a custom thread to better approximate what the JVM does.
     */
    private void clearAllMessageReceivers() {
        Thread myFinalizerThread =
                new Thread() {
                    @Override
                    public void run() {
                        for (Pair<Message, MessageReceiver> receivedMessage :
                                mStub.messagesWithReceivers) {
                            RouterImpl.ResponderThunk thunk =
                                    (RouterImpl.ResponderThunk) receivedMessage.second;
                            try {
                                thunk.finalize();
                            } catch (Throwable e) {
                                throw new RuntimeException(e);
                            }
                        }
                    }
                };
        myFinalizerThread.start();
        try {
            myFinalizerThread.join();
        } catch (InterruptedException e) {
            // ignore.
        }
        mStub.messagesWithReceivers.clear();
    }

    /** Testing receiving a message via the router that expected a response. */
    @Test
    @SmallTest
    public void testReceivingViaRouterWithResponse() throws BadMessageException {
        final int requestMethodId = 0xdead;
        final int responseMethodId = 0xbeef;
        final int requestId = 0xdeadbeaf;

        // Send a message expecting a response.
        sendMessageToRouter(0, requestMethodId, requestId);

        // Sending the response.
        sendResponseFromRouter(0, responseMethodId);
    }

    /**
     * Tests that if a callback is dropped (i.e. becomes unreachable and is finalized without being
     * used), then the message pipe will be closed.
     */
    @Test
    @SmallTest
    public void testDroppingReceiverWithoutUsingIt() throws BadMessageException {
        // Send 10 messages to the router without sending a response.
        for (int i = 0; i < 10; i++) {
            sendMessageToRouter(i, i, i);
        }

        // Now send the 10 responses. This should work fine.
        for (int i = 0; i < 10; i++) {
            sendResponseFromRouter(i, i);
        }

        // Clear all MessageRecievers so that the ResponderThunks will
        // be finalized.
        clearAllMessageReceivers();

        // Send another  message to the router without sending a response.
        sendMessageToRouter(0, 0, 0);

        // Clear the MessageReciever so that the ResponderThunk will
        // be finalized. Since the RespondeThunk was never used, this
        // should close the pipe.
        clearAllMessageReceivers();
        // The close() occurs asynchronously on this thread.
        mTestRule.runLoopUntilIdle();

        // Confirm that the pipe was closed on the Router side.
        HandleSignals closedFlag = HandleSignals.none().setPeerClosed(true);
        Assert.assertEquals(closedFlag, mHandle.querySignalsState().getSatisfiedSignals());
    }

    @Test
    @SmallTest
    public void testMessageEnqueuedIfNoStub() throws BadMessageException {
        // Set up the channel and router.
        Core core = CoreImpl.getInstance();
        var stub = new RecordingStub();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        var handle = handles.first;
        var router = new RouterImpl(handles.second);
        router.start();

        // Send a message with no registered stub, this should cause the router to enqueue the
        // message.
        {
            MessageHeader header =
                    new MessageHeader(
                            MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                            /** method id */
                            0);
            Encoder encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
            header.encode(encoder);
            Message headerMessage = encoder.getMessage();
            handle.writeMessage(
                    headerMessage.getData(),
                    new ArrayList<Handle>(),
                    MessagePipeHandle.WriteFlags.NONE);
            mTestRule.runLoopUntilIdle();
        }

        Assert.assertEquals(0, stub.messages.size());

        // This should cause immediate dispatch.
        router.setPrimaryStub(stub);

        Assert.assertEquals(1, stub.messages.size());

        // Send another message, this should cause immediate dispatch.
        {
            MessageHeader header =
                    new MessageHeader(
                            MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                            /** method id */
                            0);
            Encoder encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
            header.encode(encoder);
            Message headerMessage = encoder.getMessage();
            handle.writeMessage(
                    headerMessage.getData(),
                    new ArrayList<Handle>(),
                    MessagePipeHandle.WriteFlags.NONE);
            mTestRule.runLoopUntilIdle();
        }

        Assert.assertEquals(2, stub.messages.size());
    }
}
