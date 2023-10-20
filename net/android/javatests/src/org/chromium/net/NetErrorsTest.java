// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Tests to verify that NetError.java is created successfully. */
package org.chromium.net;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NetErrorsTest {
    // These are manually copied and should be kept in sync with net_error_list.h.
    private static final int IO_PENDING_ERROR = -1;
    private static final int FAILED_ERROR = -2;

    /**
     * Test whether we can include NetError.java and call to static integers defined in the file.
     *
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testExampleErrorDefined() {
        Assert.assertEquals(IO_PENDING_ERROR, NetError.ERR_IO_PENDING);
        Assert.assertEquals(FAILED_ERROR, NetError.ERR_FAILED);
    }
}
