// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ChildBindingState;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.TestChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.LinkedList;

/**
 * Unit test for {@link ChildProcessConnectionMetrics}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChildProcessConnectionMetricsUnitTest {
    private LinkedList<ChildProcessConnection> mRanking;
    private BindingManager mBindingManager;
    private ChildProcessConnectionMetrics mConnectionMetrics;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        LauncherThread.setCurrentThreadAsLauncherThread();
        mRanking = new LinkedList<ChildProcessConnection>();
        mBindingManager = new BindingManager(RuntimeEnvironment.application, mRanking);
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
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.ChildProcessBinding.PercentageStrongConnections_LessThan3Connections"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ModerateConnections", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.ChildProcessBinding.PercentageModerateConnections_LessThan3Connections"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivedConnections_LessThan3Connections"));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentModerateConnections", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentModerateConnections_LessThan3Connections"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentWaivedConnections_LessThan3Connections"));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivableConnections_LessThan3Connections"));
    }

    @Test
    @SmallTest
    public void testEmitMetrics() {
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.STRONG));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.MODERATE));
        ChildProcessConnection lowestRankingConnection =
                createMockConnection(ChildBindingState.WAIVED);
        mConnectionMetrics.addConnection(lowestRankingConnection);
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        setLowestRanking(lowestRankingConnection); // Now 0 moderate bindings.

        mConnectionMetrics.emitMetrics();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 6));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageStrongConnections_6To10Connections",
                        17));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ModerateConnections", 4));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageModerateConnections_6To10Connections",
                        67));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivedConnections_6To10Connections",
                        17));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentModerateConnections", 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentModerateConnections_6To10Connections",
                        17));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 4));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentWaivedConnections_6To10Connections",
                        67));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 3));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivableConnections_6To10Connections",
                        50));
    }

    @Test
    @SmallTest
    public void testEmitMetricsWithUpdate() {
        ChildProcessConnection lowestRankingConnection =
                createMockConnection(ChildBindingState.MODERATE);
        mConnectionMetrics.addConnection(lowestRankingConnection);
        ChildProcessConnection highestRankingConnection =
                createMockConnection(ChildBindingState.MODERATE);
        mConnectionMetrics.addConnection(highestRankingConnection);
        setLowestRanking(lowestRankingConnection);
        mConnectionMetrics.emitMetrics();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 2));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageStrongConnections_LessThan3Connections",
                        0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ModerateConnections", 2));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageModerateConnections_LessThan3Connections",
                        100));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivedConnections_LessThan3Connections",
                        0));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentModerateConnections", 2));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentModerateConnections_LessThan3Connections",
                        100));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentWaivedConnections_LessThan3Connections",
                        0));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivableConnections_LessThan3Connections",
                        0));

        updateContentBinding(lowestRankingConnection, ChildBindingState.WAIVED);
        updateContentBinding(highestRankingConnection, ChildBindingState.WAIVED);
        mConnectionMetrics.emitMetrics();

        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.TotalConnections", 2));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.StrongConnections", 0));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageStrongConnections_LessThan3Connections",
                        0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ModerateConnections", 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageModerateConnections_LessThan3Connections",
                        50));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivedConnections", 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivedConnections_LessThan3Connections",
                        50));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentModerateConnections", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentModerateConnections_LessThan3Connections",
                        0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.ContentWaivedConnections", 2));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageContentWaivedConnections_LessThan3Connections",
                        100));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.WaivableConnections", 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.PercentageWaivableConnections_LessThan3Connections",
                        50));
    }

    /**
     * Create a mock connection with the specified content binding state and a BindingManager
     * binding.
     */
    private ChildProcessConnection createMockConnection(
            @ChildBindingState int contentBindingState) {
        ChildProcessConnection connection = new TestChildProcessConnection(
                new ComponentName("pkg", "cls"), /*bindToCallerCheck=*/false,
                /*bindAsExternalService=*/false, /*serviceBundle=*/null);
        connection.start(/*useStrongBinding=*/false, /*serviceCallback*/ null);
        if (contentBindingState == ChildBindingState.STRONG) {
            connection.addStrongBinding();
            connection.removeModerateBinding();
        } else if (contentBindingState == ChildBindingState.WAIVED) {
            connection.removeModerateBinding();
        }
        mBindingManager.addConnection(connection);
        return connection;
    }

    /**
     * Change the current binding state of a mock {@code connection} to {@code contentBindingState}.
     */
    private void updateContentBinding(
            ChildProcessConnection connection, @ChildBindingState int contentBindingState) {
        final boolean needsStrongBinding = contentBindingState == ChildBindingState.STRONG;
        final boolean hasContentStrongBinding = connection.isStrongBindingBound();

        final boolean lowestRank = mRanking.size() == 1 && mRanking.get(0) == connection;
        final boolean needsModerateBinding = contentBindingState == ChildBindingState.MODERATE;
        final boolean hasContentModerateBinding =
                (lowestRank && connection.isModerateBindingBound())
                || connection.getModerateBindingCount() == 2;

        if (needsStrongBinding && !hasContentStrongBinding) {
            connection.addStrongBinding();
        } else if (!needsStrongBinding && hasContentStrongBinding) {
            connection.removeStrongBinding();
        }

        if (needsModerateBinding && !hasContentModerateBinding) {
            connection.addModerateBinding();
        } else if (!needsModerateBinding && hasContentModerateBinding) {
            connection.removeModerateBinding();
        }
    }

    /**
     * Make the supplied connection the lowest ranking.
     */
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
