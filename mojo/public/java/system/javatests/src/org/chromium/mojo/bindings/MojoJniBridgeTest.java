// Copyright 2026 The Chromium Authors
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
import org.chromium.base.test.util.Batch;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.jni.MojoJniTransport;
import org.chromium.mojo.bindings.test.mojom.jni.JniResultIfaceBridge;
import org.chromium.mojo.bindings.test.mojom.jni.JniTestError;
import org.chromium.mojo.bindings.test.mojom.test_structs.Rect;

import java.nio.ByteBuffer;

/**
 * End-to-end tests for {@link MojoJniTransport}, the native {@code mojo::MojoJniBridge} runtime,
 * and generated Mojom JNI bridges.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MojoJniBridgeTest {
    private static final int METHOD_TRANSPOSE_RECT = 0;
    private static final int METHOD_ONE_WAY_RECT = 1;

    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    private long mStub;
    private long mBridge;
    private long mResultStub;
    private long mResultBridge;

    @Before
    public void setUp() {
        mStub = MojoJniBridgeTestUtilJni.get().createStub();
        mBridge = MojoJniBridgeTestUtilJni.get().createBridge(mStub);
        mResultStub = MojoJniBridgeTestUtilJni.get().createResultStub();
        mResultBridge = MojoJniBridgeTestUtilJni.get().createResultBridge(mResultStub);
    }

    @After
    public void tearDown() {
        if (mResultBridge != 0) {
            MojoJniBridgeTestUtilJni.get().destroyBridge(mResultBridge);
            mResultBridge = 0;
        }
        if (mResultStub != 0) {
            MojoJniBridgeTestUtilJni.get().destroyResultStub(mResultStub);
            mResultStub = 0;
        }
        if (mBridge != 0) {
            MojoJniBridgeTestUtilJni.get().destroyBridge(mBridge);
            mBridge = 0;
        }
        if (mStub != 0) {
            MojoJniBridgeTestUtilJni.get().destroyStub(mStub);
            mStub = 0;
        }
    }

    private static ByteBuffer serializeWithHeader(Struct struct, int methodId) {
        return struct.serializeWithHeader(
                        null,
                        new MessageHeader(MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID, methodId))
                .getData();
    }

    @Test
    @SmallTest
    public void testInvoke_requestResponse() {
        Rect input = new Rect();
        input.x = 10;
        input.y = 20;
        input.width = 30;
        input.height = 40;

        byte[] responseBytes =
                MojoJniTransport.invoke(mBridge, serializeWithHeader(input, METHOD_TRANSPOSE_RECT));
        Assert.assertNotNull(responseBytes);

        ServiceMessage response = new Message(ByteBuffer.wrap(responseBytes)).asServiceMessage();
        MessageHeader header = response.getHeader();
        Assert.assertEquals(MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID, header.getInterfaceId());
        Assert.assertEquals(0, header.getMethodId());
        Assert.assertTrue(header.hasExactFlags(MessageHeader.NO_FLAG));
        Assert.assertFalse(header.hasRequestId());

        Rect output = Rect.deserialize(response.getPayload());
        Assert.assertEquals(20, output.x);
        Assert.assertEquals(10, output.y);
        Assert.assertEquals(40, output.width);
        Assert.assertEquals(30, output.height);
    }

    @Test
    @SmallTest
    public void testInvoke_oneWayReturnsNull() {
        Rect input = new Rect();
        input.x = 77;
        input.y = 1;
        input.width = 2;
        input.height = 3;

        byte[] responseBytes =
                MojoJniTransport.invoke(mBridge, serializeWithHeader(input, METHOD_ONE_WAY_RECT));
        Assert.assertNull(responseBytes);
        Assert.assertEquals(77, MojoJniBridgeTestUtilJni.get().getLastOneWayX(mStub));
    }

    @Test
    @SmallTest
    public void testGeneratedBridge_syncSuccessAndFailure() {
        JniResultIfaceBridge bridge = new JniResultIfaceBridge(mResultBridge);

        Result<Integer, JniTestError> ok = bridge.add(20, 22);
        Assert.assertTrue(ok.isSuccess());
        Assert.assertEquals(Integer.valueOf(42), ok.get());

        Result<Integer, JniTestError> err = bridge.add(-1, 5);
        Assert.assertFalse(err.isSuccess());
        Assert.assertEquals(400, err.getError().code);
        Assert.assertEquals("negative input", err.getError().message);
    }

    @Test
    @SmallTest
    public void testGeneratedBridge_stringSuccessAndFailure() {
        JniResultIfaceBridge bridge = new JniResultIfaceBridge(mResultBridge);

        Result<String, JniTestError> ok = bridge.echo("hello");
        Assert.assertTrue(ok.isSuccess());
        Assert.assertEquals("echo: hello", ok.get());

        Result<String, JniTestError> err = bridge.echo("");
        Assert.assertFalse(err.isSuccess());
        Assert.assertEquals(422, err.getError().code);
        Assert.assertEquals("empty input", err.getError().message);
    }

    @Test
    @SmallTest
    public void testGeneratedBridge_zeroArgMethod() {
        JniResultIfaceBridge bridge = new JniResultIfaceBridge(mResultBridge);

        Result<Boolean, JniTestError> ok = bridge.ping();
        Assert.assertTrue(ok.isSuccess());
        Assert.assertEquals(Boolean.TRUE, ok.get());
    }
}
