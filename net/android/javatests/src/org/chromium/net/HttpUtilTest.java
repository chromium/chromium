// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link HttpUtil}. HttpUtil forwards to native code, and the lion's share of the logic
 * is tested there. This test is primarily to make sure everything is plumbed through correctly.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class HttpUtilTest {
    private static final List<String> UNALLOWED_HEADER_NAMES = Arrays.asList(
            "accept-encoding",  // Unsafe header.
            "ACCEPT-ENCODING",  // Unsafe header.
            "referer ",  // Unsafe header.
            "referer",  // Unsafe header.
            " referer",  // Unsafe header.
            "",  // Badly formed header.
            "ref(erer",  // Badly formed header.
            "ref\nerer"  // Badly formed header.
    );

    private static final List<String> ALLOWED_HEADER_NAMES = Arrays.asList(
            "accept-language",
            "Cache-Control"
    );

    private static final String UNALLOWED_HEADER_VALUE = "value\nAccept-Encoding: br";
    private static final String ALLOWED_HEADER_VALUE = "value";

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
    }

    @Test
    @MediumTest
    public void testAllowedHeaders() {
        for (String headerName: ALLOWED_HEADER_NAMES) {
            Assert.assertTrue(headerName,
                    HttpUtil.isAllowedHeader(headerName, ALLOWED_HEADER_VALUE));
        }
    }

    @Test
    @MediumTest
    public void testUnallowedHeaders() {
        for (String headerName: UNALLOWED_HEADER_NAMES) {
            Assert.assertFalse(headerName,
                    HttpUtil.isAllowedHeader(headerName, UNALLOWED_HEADER_VALUE));
        }
    }

    @Test
    @MediumTest
    public void testUnallowedHeaderNames() {
        for (String headerName: UNALLOWED_HEADER_NAMES) {
            Assert.assertFalse(headerName,
                    HttpUtil.isAllowedHeader(headerName, ALLOWED_HEADER_VALUE));
        }
    }


    @Test
    @MediumTest
    public void testUnallowedHeaderValue() {
        for (String headerName: ALLOWED_HEADER_NAMES) {
            Assert.assertFalse(headerName,
                    HttpUtil.isAllowedHeader(headerName, UNALLOWED_HEADER_VALUE));
        }
    }
}
