// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.TimeoutTimer;

import java.util.concurrent.Callable;

/**
 * Helper methods for creating and managing criteria.
 *
 * <p>
 * If possible, use callbacks or testing delegates instead of criteria as they
 * do not introduce any polling delays.  Should only use criteria if no suitable
 * other approach exists.
 *
 * <p>
 * If you do not need failure reason, or only need static failure reason, the
 * Callback flavor can be less verbose with lambda.
 *
 * <pre>
 * <code>
 * private void assertMenuShown() {
 *     CriteriaHelper.pollUiThread(() -> getActivity().getAppMenuHandler().isAppMenuShowing(),
 *             "App menu was not shown");
 * }
 * </code>
 * </pre>
 *
 * <p>
 * Criteria supports dynamic failure reason like this:
 *
 * <pre>
 * <code>
 * public void waitForTabFullyLoaded(final Tab tab) {
 *     CriteriaHelper.pollUiThread(new Criteria() {
 *         {@literal @}Override
 *         public boolean isSatisfied() {
 *             if (tab.getWebContents() == null) {
 *                 updateFailureReason("Tab has no web contents");
 *                 return false;
 *             }
 *             updateFailureReason("Tab not fully loaded");
 *             return tab.isLoading();
 *         }
 *     });
 * }
 * </code>
 * </pre>
 */
public class CriteriaHelper {
    /** The default maximum time to wait for a criteria to become valid. */
    public static final long DEFAULT_MAX_TIME_TO_POLL = 3000L;
    /** The default polling interval to wait between checking for a satisfied criteria. */
    public static final long DEFAULT_POLLING_INTERVAL = 50;

    /**
     * Checks whether the given Criteria is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Criteria on the Instrumentation thread, which more often than not is not
     * correct in an InstrumentationTest. Use
     * {@link #pollUiThread(Criteria, long, long)} instead.
     *
     * @param criteria The Criteria that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(
            Criteria criteria, long maxTimeoutMs, long checkIntervalMs) {
        boolean isSatisfied = criteria.isSatisfied();
        TimeoutTimer timer = new TimeoutTimer(maxTimeoutMs);
        while (!isSatisfied && !timer.isTimedOut()) {
            try {
                Thread.sleep(checkIntervalMs);
            } catch (InterruptedException e) {
                // Catch the InterruptedException. If the exception occurs before maxTimeoutMs
                // and the criteria is not satisfied, the while loop will run again.
            }
            isSatisfied = criteria.isSatisfied();
        }
        Assert.assertTrue(criteria.getFailureReason(), isSatisfied);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Criteria on the test thread, which more often than not is not correct
     * in an InstrumentationTest.  Use {@link #pollUiThread(Criteria)} instead.
     *
     * @param criteria The Criteria that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria, long, long)
     */
    public static void pollInstrumentationThread(Criteria criteria) {
        pollInstrumentationThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> criteria,
            String failureReason, long maxTimeoutMs, long checkIntervalMs) {
        pollInstrumentationThread(
                toCriteria(criteria, failureReason), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     */
    public static void pollInstrumentationThread(Callable<Boolean> criteria, String failureReason) {
        pollInstrumentationThread(
                criteria, failureReason, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     */
    public static void pollInstrumentationThread(Callable<Boolean> criteria) {
        pollInstrumentationThread(criteria, null);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Criteria that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(
            final Criteria criteria, long maxTimeoutMs, long checkIntervalMs) {
        pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(criteria::isSatisfied);
            }

            @Override
            public String getFailureReason() {
                return criteria.getFailureReason();
            }
        }, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a default interval on the UI
     * thread. If dynamic failure reason is not necessary, {@link #pollUiThread(Callable)} is
     * simpler.
     * @param criteria The Criteria that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Criteria criteria) {
        pollUiThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria, String failureReason,
            long maxTimeoutMs, long checkIntervalMs) {
        pollUiThread(toCriteria(criteria, failureReason), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread. A static failure reason is given.
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria, String failureReason) {
        pollUiThread(criteria, failureReason, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread.
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria) {
        pollUiThread(criteria, null);
    }

    private static Criteria toCriteria(final Callable<Boolean> criteria, String failureReason) {
        return new Criteria(failureReason) {
            @Override
            public boolean isSatisfied() {
                try {
                    return criteria.call();
                } catch (Exception e) {
                    // If the exception keeps occurring, it would timeout.
                    return false;
                }
            }
        };
    }
}
