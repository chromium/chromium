// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.NoSuchElementException;

/** Tests the public API of the result container. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ResultTest {
    /** Tests the base case of retrieving the success value. */
    @Test
    @SmallTest
    public void testGet() throws Exception {
        Result<String, Boolean> result = Result.of("hihi");

        Assert.assertTrue(result.isSuccess());
        Assert.assertTrue("hihi".equals(result.get()));
    }

    /** Tests the base case of retrieving the error value. */
    @Test
    @SmallTest
    public void testGetError() {
        Result<String, Boolean> result = Result.ofError(true);

        Assert.assertFalse(result.isSuccess());
        Assert.assertEquals(true, result.getError());
    }

    /** Tests the case where an invalid access is performed. */
    @Test
    @SmallTest
    public void testIllegalAccess() {
        Result<String, Boolean> success = Result.of("hihi");
        Assert.assertThrows(NoSuchElementException.class, () -> success.getError());

        Result<String, Boolean> failure = Result.ofError(false);
        Assert.assertThrows(NoSuchElementException.class, () -> failure.get());
    }
}
