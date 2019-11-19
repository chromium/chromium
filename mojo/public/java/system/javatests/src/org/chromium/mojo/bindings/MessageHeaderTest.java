// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.mojo.bindings.test.mojom.imported.Point;

/**
 * Testing internal classes of interfaces.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class MessageHeaderTest {
    /**
     * Testing that headers are identical after being serialized/deserialized.
     */
    @Test
    @SmallTest
    public void testSimpleMessageHeader() {
        final int xValue = 1;
        final int yValue = 2;
        final int type = 6;
        Point p = new Point();
        p.x = xValue;
        p.y = yValue;
        ServiceMessage message = p.serializeWithHeader(null, new MessageHeader(type));

        MessageHeader header = message.getHeader();
        Assert.assertTrue(header.validateHeader(type, 0));
        Assert.assertEquals(type, header.getType());
        Assert.assertEquals(0, header.getFlags());

        Point p2 = Point.deserialize(message.getPayload());
        Assert.assertNotNull(p2);
        Assert.assertEquals(p.x, p2.x);
        Assert.assertEquals(p.y, p2.y);
    }

    /**
     * Testing that headers are identical after being serialized/deserialized.
     */
    @Test
    @SmallTest
    public void testMessageWithRequestIdHeader() {
        final int xValue = 1;
        final int yValue = 2;
        final int type = 6;
        final long requestId = 0x1deadbeafL;
        Point p = new Point();
        p.x = xValue;
        p.y = yValue;
        ServiceMessage message = p.serializeWithHeader(
                null, new MessageHeader(type, MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG, 0));
        message.setRequestId(requestId);

        MessageHeader header = message.getHeader();
        Assert.assertTrue(header.validateHeader(type, MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG));
        Assert.assertEquals(type, header.getType());
        Assert.assertEquals(MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG, header.getFlags());
        Assert.assertEquals(requestId, header.getRequestId());

        Point p2 = Point.deserialize(message.getPayload());
        Assert.assertNotNull(p2);
        Assert.assertEquals(p.x, p2.x);
        Assert.assertEquals(p.y, p2.y);
    }
}
