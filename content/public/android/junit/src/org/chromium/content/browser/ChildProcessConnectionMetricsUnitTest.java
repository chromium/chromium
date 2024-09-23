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

import java.util.LinkedList;

/** Unit test for {@link ChildProcessConnectionMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
public class ChildProcessConnectionMetricsUnitTest {
    private LinkedList<ChildProcessConnection> mRanking;
    private BindingManager mBindingManager;
    private ChildProcessConnectionMetrics mConnectionMetrics;

    @Before
    public void setUp() {
        LauncherThread.setCurrentThreadAsLauncherThread();
        mRanking = new LinkedList<ChildProcessConnection>();
        mBindingManager =
                new BindingManager(
                        RuntimeEnvironment.application, BindingManager.NO_MAX_SIZE, mRanking);
        mConnectionMetrics = new ChildProcessConnectionMetrics();
        mConnectionMetrics.setBindingManager(mBindingManager);
    }

    @Test
    @SmallTest
    public void testEmitMetricsNoConnections() {
        setupBindingType(false);
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
        setupBindingType(true);
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
    public void testEmitMetrics_BindingManagerUsesVisible() {
        setupBindingType(false);
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.STRONG));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.VISIBLE));
        ChildProcessConnection lowestRankingConnection =
                createMockConnection(ChildBindingState.WAIVED);
        mConnectionMetrics.addConnection(lowestRankingConnection);
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
        mConnectionMetrics.addConnection(createMockConnection(ChildBindingState.WAIVED));
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
                        "Android.ChildProcessBinding.VisibleConnections", 4));
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
        setupBindingType(false);
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

        updateContentBinding(lowestRankingConnection, ChildBindingState.WAIVED);
        updateContentBinding(highestRankingConnection, ChildBindingState.WAIVED);
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
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.ChildProcessBinding.VisibleConnections", 1));
        Assert.assertEquals(
                2,
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
                        /* bindToCallerCheck= */ false,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
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
     * Change the current binding state of a mock {@code connection} to {@code contentBindingState}.
     */
    private void updateContentBinding(
            ChildProcessConnection connection, @ChildBindingState int contentBindingState) {
        final boolean needsStrongBinding = contentBindingState == ChildBindingState.STRONG;
        final boolean hasContentStrongBinding = connection.isStrongBindingBound();

        final boolean lowestRank = mRanking.size() == 1 && mRanking.get(0) == connection;
        final boolean needsVisibleBinding = contentBindingState == ChildBindingState.VISIBLE;
        final boolean hasContentVisibleBinding =
                ((lowestRank || BindingManager.useNotPerceptibleBinding())
                                && connection.isVisibleBindingBound())
                        || connection.getVisibleBindingCount() == 2;

        if (needsStrongBinding && !hasContentStrongBinding) {
            connection.addStrongBinding();
        } else if (!needsStrongBinding && hasContentStrongBinding) {
            connection.removeStrongBinding();
        }

        if (needsVisibleBinding && !hasContentVisibleBinding) {
            connection.addVisibleBinding();
        } else if (!needsVisibleBinding && hasContentVisibleBinding) {
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

    private void setupBindingType(boolean useNotPerceptibleBinding) {
        BindingManager.setUseNotPerceptibleBindingForTesting(useNotPerceptibleBinding);
        if (useNotPerceptibleBinding) {
            Assert.assertTrue(BindingManager.useNotPerceptibleBinding());
            return;
        }
        Assert.assertFalse(BindingManager.useNotPerceptibleBinding());
    }
}
