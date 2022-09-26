// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.preconditions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.build.BuildConfig;

/**
 * Test that ensures Preconditions are working.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PreconditionsTest {
    public void assertThrowsNullPointerException(Runnable r) {
        try {
            r.run();
            fail("Expected NullPointerException not thrown.");
        } catch (NullPointerException e) {
        }
    }

    /**
     * Test that failing Preconditions throw an exceptions only when dchecks are on.
     */
    @Test
    @SmallTest
    public void testFailingPreconditionsWorkAsExpected() {
        if (BuildConfig.ENABLE_ASSERTS) {
            assertThrowsNullPointerException(() -> {
                com.google.android.gms.common.internal.Preconditions.checkNotNull(null);
            });
            assertThrowsNullPointerException(
                    () -> { com.google.common.base.Preconditions.checkNotNull(null); });
        } else {
            assertNull(com.google.android.gms.common.internal.Preconditions.checkNotNull(null));
            assertNull(com.google.common.base.Preconditions.checkNotNull(null));
        }
    }

    /**
     * Tests the return value when the precondition succeeds.
     */
    @Test
    @SmallTest
    public void testPassingPreconditions() {
        String value = "test";
        assertEquals(
                value, com.google.android.gms.common.internal.Preconditions.checkNotNull(value));
        assertEquals(value, com.google.common.base.Preconditions.checkNotNull(value));
    }
}
