// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests calculating the hash for Network Traffic Annotations. */
@RunWith(BaseRobolectricTestRunner.class)
public class NetworkTrafficAnnotationTagTest {
    @Test
    public void testIterativeHash() throws Exception {
        // Reference values obtained from
        // tools/traffic_annotation/scripts/auditor/auditor_tests.py.
        assertEquals(3556498, NetworkTrafficAnnotationTag.iterativeHash("test"));
        assertEquals(10236504, NetworkTrafficAnnotationTag.iterativeHash("unique_id"));
        assertEquals(70581310, NetworkTrafficAnnotationTag.iterativeHash("123_id"));
        assertEquals(69491511, NetworkTrafficAnnotationTag.iterativeHash("ID123"));
        assertEquals(
                98652091,
                NetworkTrafficAnnotationTag.iterativeHash(
                        "a_unique_"
                                + "looooooooooooooooooooooooooooooooooooooooooooooooooooooong_id"));
        assertEquals(124751853, NetworkTrafficAnnotationTag.iterativeHash("bébé"));
    }
}
