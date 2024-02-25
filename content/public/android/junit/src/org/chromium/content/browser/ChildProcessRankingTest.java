// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.TestChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.ChildProcessImportance;

/** Unit tests for ChildProessRanking */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChildProcessRankingTest {
    private TestChildProcessConnection createConnection() {
        TestChildProcessConnection connection =
                new TestChildProcessConnection(
                        new ComponentName("pkg", "cls"),
                        /* bindToCallerCheck= */ false,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        return connection;
    }

    private void assertRankingAndRemoveAll(
            ChildProcessRanking ranking, ChildProcessConnection[] connections) {
        int index = connections.length;
        ChildProcessConnection reverseIterationArray[] =
                new ChildProcessConnection[connections.length];
        for (ChildProcessConnection c : ranking) {
            reverseIterationArray[--index] = c;
        }
        Assert.assertArrayEquals(connections, reverseIterationArray);
        Assert.assertEquals(0, index);

        index = connections.length;
        ChildProcessConnection reverseRemoveArray[] =
                new ChildProcessConnection[connections.length];
        for (int i = 0; i < connections.length; ++i) {
            ChildProcessConnection c = ranking.getLowestRankedConnection();
            reverseRemoveArray[--index] = c;
            ranking.removeConnection(c);
        }
        Assert.assertArrayEquals(connections, reverseRemoveArray);
        Assert.assertNull(ranking.getLowestRankedConnection());
    }

    private void assertNotInGroup(ChildProcessConnection[] connections) {
        for (ChildProcessConnection c : connections) {
            Assert.assertEquals(0, c.getGroup());
        }
    }

    private void assertInGroupOrderedByImportance(ChildProcessConnection[] connections) {
        int importanceSoFar = -1;
        for (ChildProcessConnection c : connections) {
            Assert.assertTrue(c.getGroup() > 0);
            Assert.assertTrue(c.getImportanceInGroup() > importanceSoFar);
            importanceSoFar = c.getImportanceInGroup();
        }
    }

    @Test
    public void testRanking() {
        ChildProcessRanking ranking = new ChildProcessRanking(10);
        doTestRanking(ranking, false);
    }

    @Test
    public void testRankingWithoutLimit() {
        ChildProcessRanking ranking = new ChildProcessRanking();
        doTestRanking(ranking, false);
    }

    @Test
    public void testEnableGroupAfter() {
        ChildProcessRanking ranking = new ChildProcessRanking();
        doTestRanking(ranking, true);
    }

    private void doTestRanking(ChildProcessRanking ranking, boolean enableGroupImportanceAfter) {
        if (!enableGroupImportanceAfter) ranking.enableServiceGroupImportance();

        ChildProcessConnection c1 = createConnection();
        ChildProcessConnection c2 = createConnection();
        ChildProcessConnection c3 = createConnection();
        ChildProcessConnection c4 = createConnection();
        ChildProcessConnection c5 = createConnection();
        ChildProcessConnection c6 = createConnection();
        ChildProcessConnection c7 = createConnection();
        ChildProcessConnection c8 = createConnection();
        ChildProcessConnection c9 = createConnection();
        ChildProcessConnection c10 = createConnection();

        // Insert in lowest ranked to highest ranked order.

        // Invisible frame.
        ranking.addConnection(
                c1,
                /* foreground= */ false,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);

        // Visible subframe outside viewport.
        ranking.addConnection(
                c2,
                /* foreground= */ true,
                /* frameDepth= */ 2,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c3,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);

        // Visible subframe inside viewport.
        ranking.addConnection(
                c4,
                /* foreground= */ true,
                /* frameDepth= */ 2,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c5,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);

        // Visible main frame.
        ranking.addConnection(
                c6,
                /* foreground= */ true,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);

        if (enableGroupImportanceAfter) {
            assertNotInGroup(new ChildProcessConnection[] {c6, c5, c4, c3, c2, c1});
            ranking.enableServiceGroupImportance();
        }

        assertRankingAndRemoveAll(ranking, new ChildProcessConnection[] {c6, c5, c4, c3, c2, c1});

        assertNotInGroup(new ChildProcessConnection[] {c6, c5, c4});
        assertInGroupOrderedByImportance(new ChildProcessConnection[] {c3, c2, c1});
    }

    @Test
    public void testRankingWithImportance() {
        ChildProcessConnection c1 = createConnection();
        ChildProcessConnection c2 = createConnection();
        ChildProcessConnection c3 = createConnection();
        ChildProcessConnection c4 = createConnection();

        ChildProcessRanking ranking = new ChildProcessRanking(4);
        ranking.enableServiceGroupImportance();

        // Insert in lowest ranked to highest ranked order.
        ranking.addConnection(
                c1,
                /* foreground= */ false,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c2,
                /* foreground= */ false,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ false,
                ChildProcessImportance.MODERATE);
        ranking.addConnection(
                c3,
                /* foreground= */ false,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ false,
                ChildProcessImportance.IMPORTANT);
        ranking.addConnection(
                c4,
                /* foreground= */ false,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ false,
                ChildProcessImportance.IMPORTANT);

        assertRankingAndRemoveAll(ranking, new ChildProcessConnection[] {c4, c3, c2, c1});
        assertNotInGroup(new ChildProcessConnection[] {c4, c3, c2});
        assertInGroupOrderedByImportance(new ChildProcessConnection[] {c1});
    }

    @Test
    public void testUpdate() {
        ChildProcessConnection c1 = createConnection();
        ChildProcessConnection c2 = createConnection();
        ChildProcessConnection c3 = createConnection();
        ChildProcessConnection c4 = createConnection();

        ChildProcessRanking ranking = new ChildProcessRanking(4);
        ranking.enableServiceGroupImportance();

        // c1,2 are in one tab, and c3,4 are in second tab.
        ranking.addConnection(
                c1,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c2,
                /* foreground= */ true,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c3,
                /* foreground= */ false,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c4,
                /* foreground= */ false,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        Assert.assertEquals(c3, ranking.getLowestRankedConnection());

        // Switch from tab c1,2 to tab c3,c4.
        ranking.updateConnection(
                c1,
                /* foreground= */ false,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.updateConnection(
                c2,
                /* foreground= */ false,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.updateConnection(
                c3,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.updateConnection(
                c4,
                /* foreground= */ true,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);

        assertRankingAndRemoveAll(ranking, new ChildProcessConnection[] {c4, c3, c2, c1});
        assertNotInGroup(new ChildProcessConnection[] {c4, c3});
        assertInGroupOrderedByImportance(new ChildProcessConnection[] {c2, c1});
    }

    @Test
    public void testIntersectsViewport() {
        ChildProcessConnection c1 = createConnection();
        ChildProcessConnection c2 = createConnection();
        ChildProcessConnection c3 = createConnection();

        ChildProcessRanking ranking = new ChildProcessRanking(4);
        ranking.enableServiceGroupImportance();

        // Insert in lowest ranked to highest ranked order.
        ranking.addConnection(
                c1,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c2,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c3,
                /* foreground= */ true,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);

        assertRankingAndRemoveAll(ranking, new ChildProcessConnection[] {c3, c2, c1});
        assertNotInGroup(new ChildProcessConnection[] {c3, c2});
        assertInGroupOrderedByImportance(new ChildProcessConnection[] {c1});
    }

    @Test
    public void testFrameDepthIntOverflow() {
        ChildProcessConnection c1 = createConnection();
        ChildProcessConnection c2 = createConnection();
        ChildProcessConnection c3 = createConnection();
        ChildProcessRanking ranking = new ChildProcessRanking();

        // Native can pass up the maximum value of unsigned int.
        long intOverflow = ((long) Integer.MAX_VALUE) * 2;
        ranking.addConnection(
                c3,
                /* foreground= */ true,
                /* frameDepth= */ intOverflow - 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c2,
                /* foreground= */ true,
                /* frameDepth= */ 10,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c1,
                /* foreground= */ true,
                /* frameDepth= */ intOverflow,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);

        assertRankingAndRemoveAll(ranking, new ChildProcessConnection[] {c2, c3, c1});
    }

    @Test
    public void testThrowExceptionWhenGoingOverLimit() {
        ChildProcessRanking ranking = new ChildProcessRanking(2);

        ChildProcessConnection c1 = createConnection();
        ChildProcessConnection c2 = createConnection();
        ChildProcessConnection c3 = createConnection();

        ranking.addConnection(
                c1,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c2,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ true,
                ChildProcessImportance.NORMAL);
        boolean exceptionThrown = false;
        try {
            ranking.addConnection(
                    c3,
                    /* foreground= */ true,
                    /* frameDepth= */ 1,
                    /* intersectsViewport= */ true,
                    ChildProcessImportance.NORMAL);
        } catch (Throwable e) {
            exceptionThrown = true;
        }
        Assert.assertTrue(exceptionThrown);
    }

    @Test
    public void testRebindHighRankConnection() {
        ChildProcessRanking ranking = new ChildProcessRanking();
        ranking.enableServiceGroupImportance();

        TestChildProcessConnection c1 = createConnection();
        TestChildProcessConnection c2 = createConnection();
        TestChildProcessConnection c3 = createConnection();

        ranking.addConnection(
                c1,
                /* foreground= */ true,
                /* frameDepth= */ 0,
                /* intersectsViewport= */ false,
                ChildProcessImportance.IMPORTANT);
        ranking.addConnection(
                c2,
                /* foreground= */ true,
                /* frameDepth= */ 2,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);
        ranking.addConnection(
                c3,
                /* foreground= */ true,
                /* frameDepth= */ 3,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);

        assertNotInGroup(new ChildProcessConnection[] {c1});
        assertInGroupOrderedByImportance(new ChildProcessConnection[] {c2, c3});

        c1.getAndResetRebindCalled();
        ranking.updateConnection(
                c3,
                /* foreground= */ true,
                /* frameDepth= */ 1,
                /* intersectsViewport= */ false,
                ChildProcessImportance.NORMAL);
        assertNotInGroup(new ChildProcessConnection[] {c1});
        assertInGroupOrderedByImportance(new ChildProcessConnection[] {c3, c2});
        Assert.assertFalse(c1.getAndResetRebindCalled());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(c1.getAndResetRebindCalled());
    }
}
