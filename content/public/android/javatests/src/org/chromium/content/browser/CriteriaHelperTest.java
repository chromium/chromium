// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.hamcrest.CoreMatchers.containsString;
import static org.junit.Assert.assertThat;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;

import java.io.PrintWriter;
import java.io.StringWriter;

/**
 * Tests for {@link CriteriaHelper}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CriteriaHelperTest {
    private static final String ERROR_MESSAGE = "my special error message";

    @Rule
    public ExpectedException thrown = ExpectedException.none();

    @Test
    @MediumTest
    public void testUiThread() {
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
    public void testPass_Runnable_UiThread() {
        CriteriaHelper.pollUiThread(() -> {});
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testPass_Runnable_UiThreadNested() {
        CriteriaHelper.pollUiThreadNested(() -> {});
    }

    @Test
    @MediumTest
    public void testPass_Runnable_InstrumentationThread() {
        CriteriaHelper.pollInstrumentationThread(() -> {});
    }

    @Test
    @MediumTest
    public void testPass_Callable_UiThread() {
        CriteriaHelper.pollUiThread(() -> true);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testPass_Callable_UiThreadNested() {
        CriteriaHelper.pollUiThreadNested(() -> true);
    }

    @Test
    @MediumTest
    public void testPass_Callable_InstrumentationThread() {
        CriteriaHelper.pollInstrumentationThread(() -> true);
    }

    @Test
    @MediumTest
    public void testThrow_Runnable_UiThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThread(() -> {
            throw new CriteriaNotSatisfiedException("");
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThrow_Runnable_UiThreadNested() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThreadNested(() -> {
            throw new CriteriaNotSatisfiedException("");
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Runnable_InstrumentationThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollInstrumentationThread(() -> {
            throw new CriteriaNotSatisfiedException("");
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Callable_UiThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThrow_Callable_UiThreadNested() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThreadNested(() -> false, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Callable_InstrumentationThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollInstrumentationThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Runnable_UiThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThread(() -> {
            throw new CriteriaNotSatisfiedException(ERROR_MESSAGE);
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testMessage_Runnable_UiThreadNested() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThreadNested(() -> {
            throw new CriteriaNotSatisfiedException(ERROR_MESSAGE);
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Runnable_InstrumentationThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollInstrumentationThread(() -> {
            throw new CriteriaNotSatisfiedException(ERROR_MESSAGE);
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Callback_UiThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThread(() -> false, ERROR_MESSAGE, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Callback_InstrumentationThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollInstrumentationThread(
                () -> false, ERROR_MESSAGE, 0, DEFAULT_POLLING_INTERVAL);
    }

    private String getStackTrace(Throwable e) {
        StringWriter sw = new StringWriter();
        e.printStackTrace(new PrintWriter(sw));
        return sw.toString();
    }

    @Test
    @MediumTest
    public void testStack_Runnable_UiThread() {
        try {
            CriteriaHelper.pollUiThread(() -> {
                throw new CriteriaNotSatisfiedException("test");
            }, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Runnable_UiThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testStack_Runnable_UiThreadNested() {
        try {
            CriteriaHelper.pollUiThreadNested(() -> {
                throw new CriteriaNotSatisfiedException("test");
            }, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Runnable_UiThreadNested("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Runnable_InstrumentationThread() {
        try {
            CriteriaHelper.pollInstrumentationThread(() -> {
                throw new CriteriaNotSatisfiedException("test");
            }, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Runnable_InstrumentationThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Callable_UiThread() {
        try {
            CriteriaHelper.pollUiThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Callable_UiThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testStack_Callable_UiThreadNested() {
        try {
            CriteriaHelper.pollUiThreadNested(() -> false, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Callable_UiThreadNested("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Callable_InstrumentationThread() {
        try {
            CriteriaHelper.pollInstrumentationThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Callable_InstrumentationThread("));
            return;
        }
        Assert.fail();
    }
}
