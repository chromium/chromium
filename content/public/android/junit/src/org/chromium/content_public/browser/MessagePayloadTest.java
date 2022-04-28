// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

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
        Assert.assertEquals(messagePayload.getAsString(), testStr);
        Assert.assertEquals(
                messagePayload.getType(), MessagePayload.MessagePayloadType.TYPE_STRING);
    }
}
