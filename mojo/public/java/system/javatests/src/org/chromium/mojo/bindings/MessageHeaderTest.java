// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.mojo.bindings.test.mojom.imported.Point;

/** Testing internal classes of interfaces. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MessageHeaderTest {
    /** Testing that headers are identical after being serialized/deserialized. */
    @Test
    @SmallTest
    public void testSimpleMessageHeader() throws BadMessageException {
        final int xValue = 1;
        final int yValue = 2;
        final int methodId = 6;
        Point p = new Point();
        p.x = xValue;
        p.y = yValue;
        ServiceMessage message =
                p.serializeWithHeader(
                        null,
                        new MessageHeader(MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID, methodId));

        MessageHeader header = message.getHeader();
        Assert.assertEquals(0, header.getInterfaceId());
        Assert.assertEquals(methodId, header.getMethodId());
        Assert.assertEquals(0, header.getFlags());

        Point p2 = Point.deserialize(message.getPayload());
        Assert.assertNotNull(p2);
        Assert.assertEquals(p.x, p2.x);
        Assert.assertEquals(p.y, p2.y);
    }

    /** Testing that headers are identical after being serialized/deserialized. */
    @Test
    @SmallTest
    public void testMessageWithRequestIdHeader() throws BadMessageException {
        final int xValue = 1;
        final int yValue = 2;
        final int methodId = 6;
        final long requestId = 0x1deadbeafL;
        Point p = new Point();
        p.x = xValue;
        p.y = yValue;
        ServiceMessage message =
                p.serializeWithHeader(
                        null,
                        new MessageHeader(
                                MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                                methodId,
                                MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG,
                                0));
        message.setRequestId(requestId);

        MessageHeader header = message.getHeader();
        Assert.assertEquals(0, header.getInterfaceId());
        Assert.assertEquals(methodId, header.getMethodId());
        Assert.assertEquals(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG, header.getFlags());
        Assert.assertEquals(requestId, header.getRequestId());

        Point p2 = Point.deserialize(message.getPayload());
        Assert.assertNotNull(p2);
        Assert.assertEquals(p.x, p2.x);
        Assert.assertEquals(p.y, p2.y);
    }

    @Test
    @SmallTest
    public void testMessageHeader_hasExactFlags() throws BadMessageException {
        var expectedFlags =
                MessageHeader.MESSAGE_IS_RESPONSE_FLAG | MessageHeader.MESSAGE_IS_SYNC_FLAG;
        var header =
                new MessageHeader(
                        MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID, 0, expectedFlags, 0);
        Assert.assertTrue(header.hasExactFlags(expectedFlags));
        // Too many flags.
        Assert.assertFalse(
                header.hasExactFlags(expectedFlags | MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG));
        // Too few flags.
        Assert.assertFalse(header.hasExactFlags(MessageHeader.MESSAGE_IS_RESPONSE_FLAG));

        // Unknown flags should be dropped.
        var headerWithUnknownFlags =
                new MessageHeader(
                        MessageHeader.TEMPORARY_DEFAULT_INTERFACE_ID,
                        0,
                        MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG | (0x1 << 20),
                        0);
        Assert.assertTrue(
                headerWithUnknownFlags.hasExactFlags(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG));
    }
}
