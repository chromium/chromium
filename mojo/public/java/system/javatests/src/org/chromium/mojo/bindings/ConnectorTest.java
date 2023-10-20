// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.BindingsTestUtils.CapturingErrorHandler;
import org.chromium.mojo.bindings.BindingsTestUtils.RecordingMessageReceiver;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.ResultAnd;
import org.chromium.mojo.system.impl.CoreImpl;

import java.nio.ByteBuffer;
import java.util.ArrayList;

/** Testing the {@link Connector} class. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ConnectorTest {
    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    private static final int DATA_LENGTH = 1024;

    private MessagePipeHandle mHandle;
    private Connector mConnector;
    private Message mTestMessage;
    private RecordingMessageReceiver mReceiver;
    private CapturingErrorHandler mErrorHandler;

    /**
     * @see MojoTestCase#setUp()
     */
    @Before
    public void setUp() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles =
                core.createMessagePipe(new MessagePipeHandle.CreateOptions());
        mHandle = handles.first;
        mConnector = new Connector(handles.second);
        mReceiver = new RecordingMessageReceiver();
        mConnector.setIncomingMessageReceiver(mReceiver);
        mErrorHandler = new CapturingErrorHandler();
        mConnector.setErrorHandler(mErrorHandler);
        mConnector.start();
        mTestMessage = BindingsTestUtils.newRandomMessage(DATA_LENGTH);
        Assert.assertNull(mErrorHandler.getLastMojoException());
        Assert.assertEquals(0, mReceiver.messages.size());
    }

    /**
     * @see MojoTestCase#tearDown()
     */
    @After
    public void tearDown() {
        mConnector.close();
        mHandle.close();
    }

    /** Test sending a message through a {@link Connector}. */
    @Test
    @SmallTest
    public void testSendingMessage() {
        mConnector.accept(mTestMessage);
        Assert.assertNull(mErrorHandler.getLastMojoException());
        ResultAnd<MessagePipeHandle.ReadMessageResult> result =
                mHandle.readMessage(MessagePipeHandle.ReadFlags.NONE);
        Assert.assertEquals(MojoResult.OK, result.getMojoResult());
        Assert.assertEquals(DATA_LENGTH, result.getValue().mData.length);
        Assert.assertEquals(mTestMessage.getData(), ByteBuffer.wrap(result.getValue().mData));
    }

    /** Test receiving a message through a {@link Connector} */
    @Test
    @SmallTest
    public void testReceivingMessage() {
        mHandle.writeMessage(
                mTestMessage.getData(), new ArrayList<Handle>(), MessagePipeHandle.WriteFlags.NONE);
        mTestRule.runLoopUntilIdle();
        Assert.assertNull(mErrorHandler.getLastMojoException());
        Assert.assertEquals(1, mReceiver.messages.size());
        Message received = mReceiver.messages.get(0);
        Assert.assertEquals(0, received.getHandles().size());
        Assert.assertEquals(mTestMessage.getData(), received.getData());
    }

    /** Test receiving an error through a {@link Connector}. */
    @Test
    @SmallTest
    public void testErrors() {
        mHandle.close();
        mTestRule.runLoopUntilIdle();
        Assert.assertNotNull(mErrorHandler.getLastMojoException());
        Assert.assertEquals(
                MojoResult.FAILED_PRECONDITION,
                mErrorHandler.getLastMojoException().getMojoResult());
    }
}
