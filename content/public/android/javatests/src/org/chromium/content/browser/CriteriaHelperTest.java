// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaHelper.TimeoutException;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;

import java.util.concurrent.Callable;

/** Tests for {@link CriteriaHelper}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CriteriaHelperTest {
    private static final String ERROR_MESSAGE = "my special error message";
    private static final String OUTER_ERROR_MESSAGE = "Timed out after 0 milliseconds";

    private static final Runnable NEVER_SATISFIED_RUNNABLE =
            () -> {
                throw new CriteriaNotSatisfiedException(ERROR_MESSAGE);
            };
    private static final Callable FALSE_CALLABLE = () -> false;

    @Test
    @MediumTest
    public void testUiThread() {
        // Also tests Criteria.checkThat().
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(ThreadUtils.runningOnUiThread(), Matchers.is(true)));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testUiThreadNested() {
        CriteriaHelper.pollUiThreadNested(
                () -> Criteria.checkThat(ThreadUtils.runningOnUiThread(), Matchers.is(true)));
    }

    @Test
    @MediumTest
    public void testInstrumentationThread() {
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(ThreadUtils.runningOnUiThread(), Matchers.is(false)));
    }

    @Test
    @MediumTest
    public void testUiThread_Callable() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Assert.assertTrue(ThreadUtils.runningOnUiThread());
                    return true;
                });
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testUiThreadNestedCallable() {
        CriteriaHelper.pollUiThreadNested(
                () -> {
                    Assert.assertTrue(ThreadUtils.runningOnUiThread());
                    return true;
                });
    }

    @Test
    @MediumTest
    public void testInstrumentationThread_Callable() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Assert.assertFalse(ThreadUtils.runningOnUiThread());
                    return true;
                });
    }

    @Test
    @MediumTest
    public void testThrow_Runnable_UiThread() {
        TimeoutException t =
                Assert.assertThrows(
                        TimeoutException.class,
                        () -> {
                            CriteriaHelper.pollUiThread(
                                    NEVER_SATISFIED_RUNNABLE, 0, DEFAULT_POLLING_INTERVAL);
                        });
        Assert.assertEquals(OUTER_ERROR_MESSAGE, t.getMessage());
        Assert.assertEquals(ERROR_MESSAGE, t.getCause().getMessage());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThrow_Runnable_UiThreadNested() {
        TimeoutException t =
                Assert.assertThrows(
                        TimeoutException.class,
                        () -> {
                            CriteriaHelper.pollUiThreadNested(
                                    NEVER_SATISFIED_RUNNABLE, 0, DEFAULT_POLLING_INTERVAL);
                        });
        Assert.assertEquals(OUTER_ERROR_MESSAGE, t.getMessage());
        Assert.assertEquals(ERROR_MESSAGE, t.getCause().getMessage());
    }

    @Test
    @MediumTest
    public void testThrow_Runnable_InstrumentationThread() {
        TimeoutException t =
                Assert.assertThrows(
                        TimeoutException.class,
                        () -> {
                            CriteriaHelper.pollInstrumentationThread(
                                    NEVER_SATISFIED_RUNNABLE, 0, DEFAULT_POLLING_INTERVAL);
                        });
        Assert.assertEquals(OUTER_ERROR_MESSAGE, t.getMessage());
        Assert.assertEquals(ERROR_MESSAGE, t.getCause().getMessage());
    }

    @Test
    @MediumTest
    public void testThrow_Callable_UiThread() {
        TimeoutException t =
                Assert.assertThrows(
                        TimeoutException.class,
                        () -> {
                            CriteriaHelper.pollUiThread(
                                    FALSE_CALLABLE, 0, DEFAULT_POLLING_INTERVAL);
                        });
        Assert.assertEquals(OUTER_ERROR_MESSAGE, t.getMessage());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThrow_Callable_UiThreadNested() {
        TimeoutException t =
                Assert.assertThrows(
                        TimeoutException.class,
                        () -> {
                            CriteriaHelper.pollUiThreadNested(
                                    FALSE_CALLABLE, 0, DEFAULT_POLLING_INTERVAL);
                        });
        Assert.assertEquals(OUTER_ERROR_MESSAGE, t.getMessage());
    }

    @Test
    @MediumTest
    public void testThrow_Callable_InstrumentationThread() {
        TimeoutException t =
                Assert.assertThrows(
                        TimeoutException.class,
                        () -> {
                            CriteriaHelper.pollInstrumentationThread(
                                    FALSE_CALLABLE, 0, DEFAULT_POLLING_INTERVAL);
                        });
        Assert.assertEquals(OUTER_ERROR_MESSAGE, t.getMessage());
    }
}
