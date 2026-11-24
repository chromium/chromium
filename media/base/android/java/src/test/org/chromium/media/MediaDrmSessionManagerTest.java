// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.media.MediaDrmSessionManager.SessionId;

/** Tests for MediaDrmSessionManager. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaDrmSessionManagerTest {
    @Test
    public void testToHexStringHandlesBytesWithHighBitSet() {
        byte[] bytes = new byte[] {0x00, 0x0f, 0x7f, (byte) 0x80, (byte) 0xab, (byte) 0xff};
        assertEquals("000F7F80ABFF", SessionId.toHexString(bytes));
    }
}
