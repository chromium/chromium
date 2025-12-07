// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.UnsupportedEncodingException;

/**
 * Unit tests for MessagePayload.
 * Note: After new type is added, please add a test case here.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MessagePayloadTest {
    @Test
    public void testString() {
        final String testStr = "TestStr";
        MessagePayload messagePayload = new MessagePayload(testStr);
        Assert.assertEquals(testStr, messagePayload.getAsString());
        Assert.assertEquals(MessagePayloadType.STRING, messagePayload.getType());

        Assert.assertEquals(MessagePayloadType.STRING, messagePayload.getType());
    }

    @Test
    public void testStringCanBeNull() {
        MessagePayload jsValue = new MessagePayload((String) null);
        Assert.assertNull(jsValue.getAsString());
        Assert.assertEquals(MessagePayloadType.STRING, jsValue.getType());
    }

    @Test
    public void testArrayBuffer() throws UnsupportedEncodingException {
        final byte[] bytes = "TestStr".getBytes("UTF-8");
        MessagePayload jsValue = new MessagePayload(bytes);
        Assert.assertEquals(jsValue.getAsArrayBuffer(), bytes);
        Assert.assertEquals(MessagePayloadType.ARRAY_BUFFER, jsValue.getType());
    }

    @Test
    public void testArrayBufferCannotBeNull() {
        try {
            new MessagePayload((byte[]) null);
            Assert.fail("Should throw exception");
        } catch (NullPointerException e) {
            // Expected
        }
    }

    @Test
    public void testWrongValueTypeString() throws UnsupportedEncodingException {
        MessagePayload jsValue = new MessagePayload("TestStr".getBytes("UTF-8"));
        Assert.assertEquals(MessagePayloadType.ARRAY_BUFFER, jsValue.getType());
        try {
            jsValue.getAsString();
            Assert.fail("Should throw exception");
        } catch (IllegalStateException e) {
            // Expected
        }
    }

    @Test
    public void testWrongValueTypeArrayBuffer() {
        MessagePayload jsValue = new MessagePayload("TestStr");
        Assert.assertEquals(MessagePayloadType.STRING, jsValue.getType());
        try {
            jsValue.getAsArrayBuffer();
            Assert.fail("Should throw exception");
        } catch (IllegalStateException e) {
            // Expected
        }
    }
}
