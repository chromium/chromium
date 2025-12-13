// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ChildBindingState;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.TestChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;

/** Unit test for {@link ChildProcessConnectionMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
public class ChildProcessConnectionMetricsUnitTest {
    private ArrayList<ChildProcessConnection> mRanking;
    private BindingManager mBindingManager;
    private ChildProcessConnectionMetrics mConnectionMetrics;

    @Before
    public void setUp() {
        LauncherThread.setCurrentThreadAsLauncherThread();
        mRanking = new ArrayList<ChildProcessConnection>();
        mBindingManager =
                new BindingManager(
                        RuntimeEnvironment.application,
                        BindingManager.NO_MAX_SIZE,
                        mRanking,
                        /* onChangedImplicitly= */ null);
        mConnectionMetrics = new ChildProcessConnectionMetrics();
        mConnectionMetrics.setBindingManager(mBindingManager);
    }

    @Test
    @SmallTest
    public void testEmitMetricsNoConnections() {
        ChildProcessConnection connection = createMockConnection(ChildBindingState.STRONG);
        mConnectionMetrics.addConnection(connection);
        removeConnection(connection);

        mConnectionMetrics.emitMetrics();
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.VisibleConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.NotPerceptibleConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 0));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentVisibleConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 0));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 0));
    }

    @Test
    @SmallTest
    public void testEmitMetrics_BindingManagerUsesNotPerceptible() {
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.STRONG));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.VISIBLE));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        ChildProcessConnection lowestRankingConnection =
                createMockConnection(ChildBindingState.WAIVED);
        mConnectionMetrics.addConnection(lowestRankingConnection);
        setLowestRanking(lowestRankingConnection);

        mConnectionMetrics.emitMetrics();
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 6));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 1));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.VisibleConnections", 1));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.NotPerceptibleConnections", 3));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 1));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentVisibleConnections", 1));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 4));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 3));
    }

    @Test
    @SmallTest
    public void testEmitMetricsWithUpdate() {
        ChildProcessConnection lowestRankingConnection =
                createMockConnection(ChildBindingState.VISIBLE);
        mConnectionMetrics.addConnection(lowestRankingConnection);
        ChildProcessConnection highestRankingConnection =
                createMockConnection(ChildBindingState.VISIBLE);
        mConnectionMetrics.addConnection(highestRankingConnection);
        setLowestRanking(lowestRankingConnection);
        mConnectionMetrics.emitMetrics();

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 2));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.VisibleConnections", 2));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.NotPerceptibleConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 0));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentVisibleConnections", 2));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 0));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 0));

        removeHighPriorityBindings(lowestRankingConnection);
        removeHighPriorityBindings(highestRankingConnection);
        mConnectionMetrics.emitMetrics();

        Assert.assertEquals(
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 2));
        Assert.assertEquals(
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 0));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.VisibleConnections", 1));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.NotPerceptibleConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 1));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentVisibleConnections", 0));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 2));

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 1));
    }

    /**
     * Create a mock connection with the specified content binding state and a BindingManager
     * binding.
     */
    private ChildProcessConnection createMockConnection(
            @ChildBindingState int contentBindingState) {
        ChildProcessConnection connection =
                new TestChildProcessConnection(
                        new ComponentName("pkg", "cls"),
                        /* bindToCaller= */ false,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null);
        connection.start(ChildBindingState.VISIBLE, /* serviceCallback= */ null);
        if (contentBindingState == ChildBindingState.STRONG) {
            connection.addStrongBinding();
            connection.removeVisibleBinding();
        } else if (contentBindingState == ChildBindingState.WAIVED) {
            connection.removeVisibleBinding();
        }
        mBindingManager.addConnection(connection);
        return connection;
    }

    /**
     * Remove all strong and visible bindings from the given connection.
     *
     * @param connection The connection to remove bindings from.
     */
    private void removeHighPriorityBindings(ChildProcessConnection connection) {
        if (connection.bindingStateCurrent() == ChildBindingState.STRONG) {
            connection.removeStrongBinding();
        }
        if (connection.bindingStateCurrent() == ChildBindingState.VISIBLE) {
            connection.removeVisibleBinding();
        }
    }

    /** Make the supplied connection the lowest ranking. */
    private void setLowestRanking(ChildProcessConnection connection) {
        if (mRanking.size() == 1) {
            mBindingManager.removeConnection(mRanking.get(0));
            mBindingManager.addConnection(mRanking.get(0));
        }
        mRanking.clear();
        if (connection != null) {
            mRanking.add(connection);
        }
        mBindingManager.rankingChanged();
    }

    private void removeConnection(ChildProcessConnection connection) {
        mConnectionMetrics.removeConnection(connection);
        mBindingManager.removeConnection(connection);
    }
}
