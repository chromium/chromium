// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServer.EmbeddedTestServerFailure;

/**
 * Tests for {@link EmbeddedTestServer}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class EmbeddedTestServerTest {
    /**
     * Calling {@link EmbeddedTestServer#stopAndDestroyServer} more than once should hard fail.
     */
    @Test(expected = EmbeddedTestServerFailure.class)
    @MediumTest
    public void testServiceAliveAfterNativePage() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        testServer.stopAndDestroyServer();
        testServer.stopAndDestroyServer();
    }
}
