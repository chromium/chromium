// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;

/** Test suite for {@link FakeAndroidCache}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@MinAndroidSdkLevel(Build.VERSION_CODES.TIRAMISU)
public class FakeAndroidCacheTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private WebContentsAccessibilityImpl mWebContentsAccessibility;
    private FakeAndroidCache mFakeAndroidCache;
    private final int mFirstNodeId = 1;
    private final int mSecondNodeId = 2;
    private final int mThirdNodeId = 3;
    private final int mFourthNodeId = 4;

    @Before
    public void setUp() {
        Mockito.when(mWebContentsAccessibility.getChildIdsForExperiment(Mockito.anyInt()))
                .thenReturn(new int[] {});
        Mockito.when(mWebContentsAccessibility.getCurrentRootIdForExperiment()).thenReturn(-1);
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(Mockito.anyInt()))
                .thenReturn(null);
        mFakeAndroidCache = new FakeAndroidCache(mWebContentsAccessibility);
    }

    @Test
    @SmallTest
    public void testAddNodeAndRemoveIt() {
        // Create a test node and remove it from the cache.
        AccessibilityNodeInfoCompat testNode =
                fillEmptyAccessibilityNodeInfoCompat("testNode", String.valueOf(mFirstNodeId));
        mFakeAndroidCache.addNode(mFirstNodeId, testNode);
        mFakeAndroidCache.clearNode(mFirstNodeId, /* recursive= */ false);

        // To make sure its not in the cache, we make sure to mark is as stale by updating the
        // expected node to be returned if ever built again.
        AccessibilityNodeInfoCompat testNodeUpdated =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated", String.valueOf(mFirstNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mFirstNodeId))
                .thenReturn(testNodeUpdated);
        mFakeAndroidCache.validateAccessibilityForExperiment();
        Assert.assertEquals(0, mFakeAndroidCache.getStaleNodeCountForTesting());
    }

    @Test
    @SmallTest
    public void testDetectStaleNode() {
        // Create a test node and add it to the cache.
        AccessibilityNodeInfoCompat testNode =
                fillEmptyAccessibilityNodeInfoCompat("testNode", String.valueOf(mFirstNodeId));
        mFakeAndroidCache.addNode(mFirstNodeId, testNode);

        // Make stale by making the expectation return a different node.
        AccessibilityNodeInfoCompat testNodeUpdated =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated", String.valueOf(mFirstNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mFirstNodeId))
                .thenReturn(testNodeUpdated);
        mFakeAndroidCache.validateAccessibilityForExperiment();
        Assert.assertEquals(1, mFakeAndroidCache.getStaleNodeCountForTesting());
    }

    @Test
    @SmallTest
    public void testNotStaleChildNode() {
        // Create a test node and add it to the cache.
        AccessibilityNodeInfoCompat testNode1 =
                fillEmptyAccessibilityNodeInfoCompat("testNode1", String.valueOf(mFirstNodeId));
        Mockito.when(mWebContentsAccessibility.getChildIdsForExperiment(mFirstNodeId))
                .thenReturn(new int[] {mSecondNodeId});
        mFakeAndroidCache.addNode(mFirstNodeId, testNode1);

        AccessibilityNodeInfoCompat testNode2 =
                fillEmptyAccessibilityNodeInfoCompat("testNode2", String.valueOf(mSecondNodeId));
        mFakeAndroidCache.addNode(mSecondNodeId, testNode2);

        mFakeAndroidCache.clearNode(mFirstNodeId, /* recursive= */ true);

        // If recreated, it should show up as stale when validating by returning a different node.
        AccessibilityNodeInfoCompat testNodeUpdated2 =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated2", String.valueOf(mSecondNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mSecondNodeId))
                .thenReturn(testNodeUpdated2);

        mFakeAndroidCache.validateAccessibilityForExperiment();
        Assert.assertEquals(0, mFakeAndroidCache.getStaleNodeCountForTesting());
    }

    @Test
    @SmallTest
    public void testDetectStaleChildNode() {
        // Create a test node and add it to the cache.
        AccessibilityNodeInfoCompat testNode1 =
                fillEmptyAccessibilityNodeInfoCompat("testNode1", String.valueOf(mFirstNodeId));
        Mockito.when(mWebContentsAccessibility.getChildIdsForExperiment(mFirstNodeId))
                .thenReturn(new int[] {mSecondNodeId});
        mFakeAndroidCache.addNode(mFirstNodeId, testNode1);

        AccessibilityNodeInfoCompat testNode2 =
                fillEmptyAccessibilityNodeInfoCompat("testNode2", String.valueOf(mSecondNodeId));
        mFakeAndroidCache.addNode(mSecondNodeId, testNode2);

        mFakeAndroidCache.clearNode(mFirstNodeId, /* recursive= */ false);

        // Make stale by making the expectation return a different node.
        AccessibilityNodeInfoCompat testNodeUpdated2 =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated2", String.valueOf(mSecondNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mSecondNodeId))
                .thenReturn(testNodeUpdated2);

        mFakeAndroidCache.validateAccessibilityForExperiment();
        Assert.assertEquals(1, mFakeAndroidCache.getStaleNodeCountForTesting());
    }

    @Test
    @SmallTest
    public void testComplexTree() {
        // Create a test node and add it to the cache.
        AccessibilityNodeInfoCompat testNode1 =
                fillEmptyAccessibilityNodeInfoCompat("testNode1", String.valueOf(mFirstNodeId));
        Mockito.when(mWebContentsAccessibility.getChildIdsForExperiment(mFirstNodeId))
                .thenReturn(new int[] {mSecondNodeId});
        mFakeAndroidCache.addNode(mFirstNodeId, testNode1);

        AccessibilityNodeInfoCompat testNode2 =
                fillEmptyAccessibilityNodeInfoCompat("testNode2", String.valueOf(mSecondNodeId));
        Mockito.when(mWebContentsAccessibility.getChildIdsForExperiment(mSecondNodeId))
                .thenReturn(new int[] {mThirdNodeId, mFourthNodeId});
        mFakeAndroidCache.addNode(mSecondNodeId, testNode2);

        AccessibilityNodeInfoCompat testNode3 =
                fillEmptyAccessibilityNodeInfoCompat("testNode3", String.valueOf(mThirdNodeId));
        mFakeAndroidCache.addNode(mThirdNodeId, testNode3);

        AccessibilityNodeInfoCompat testNode4 =
                fillEmptyAccessibilityNodeInfoCompat("testNode4", String.valueOf(mFourthNodeId));
        mFakeAndroidCache.addNode(mFourthNodeId, testNode4);

        mFakeAndroidCache.clearNode(mSecondNodeId, /* recursive= */ false);

        mFakeAndroidCache.validateAccessibilityForExperiment();
        Assert.assertEquals(0, mFakeAndroidCache.getStaleNodeCountForTesting());

        // We then recursive clear node1, and expect for grand children to be cleared too.
        mFakeAndroidCache.clearNode(mFirstNodeId, /* recursive= */ true);

        // We make the grand children return a mismatch if recreated when validating since they
        // should have been cleared from the cache.
        AccessibilityNodeInfoCompat testNodeUpdated3 =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated3", String.valueOf(mThirdNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mThirdNodeId))
                .thenReturn(testNodeUpdated3);
        AccessibilityNodeInfoCompat testNodeUpdated4 =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated4", String.valueOf(mFourthNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mFourthNodeId))
                .thenReturn(testNodeUpdated4);

        // There should be no stale nodes, we cleared all the cache.
        mFakeAndroidCache.validateAccessibilityForExperiment();
        Assert.assertEquals(0, mFakeAndroidCache.getStaleNodeCountForTesting());
    }

    @Test
    @SmallTest
    public void testOnlyReportNewIncomingNodes() {
        AccessibilityHistogramRecorder mockRecorder =
                Mockito.mock(AccessibilityHistogramRecorder.class);
        FakeAndroidCache cacheWithRecorder =
                new FakeAndroidCache(mWebContentsAccessibility, mockRecorder);

        AccessibilityNodeInfoCompat testNode =
                fillEmptyAccessibilityNodeInfoCompat("testNode", String.valueOf(mFirstNodeId));

        // 1. Add the node for the first time (new incoming node).
        cacheWithRecorder.addNode(mFirstNodeId, testNode);

        // Verify that reportNodeAddedToFakeCache is called exactly once.
        Mockito.verify(mockRecorder, Mockito.times(1)).reportNodeAddedToFakeCache(mFirstNodeId);

        // 2. Add the node again (update/replacement).
        cacheWithRecorder.addNode(mFirstNodeId, testNode);

        // Verify that reportNodeAddedToFakeCache was NOT called again (total times called is still
        // 1).
        Mockito.verify(mockRecorder, Mockito.times(1)).reportNodeAddedToFakeCache(mFirstNodeId);
    }

    @Test
    @SmallTest
    public void testScenarioSteadyState() {
        AccessibilityHistogramRecorder recorder = new AccessibilityHistogramRecorder();
        FakeAndroidCache cacheWithRecorder =
                new FakeAndroidCache(mWebContentsAccessibility, recorder);

        // Stub getAccessibilityTreeSizeForExperiment so validation doesn't return early
        Mockito.when(mWebContentsAccessibility.getAccessibilityTreeSizeForExperiment())
                .thenReturn(20L);

        // 1. Baseline round (add 10 nodes, remove 1 node)
        for (int i = 1; i <= 10; i++) {
            cacheWithRecorder.addNode(
                    i, fillEmptyAccessibilityNodeInfoCompat("node" + i, String.valueOf(i)));
        }
        cacheWithRecorder.clearNode(10, /* recursive= */ false); // Cache size becomes 9

        cacheWithRecorder
                .validateAccessibilityForExperiment(); // Triggers Round 1 validation (clears sets,
        // updates cacheSize to 9)

        int previousCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals("Steady state previous cache size should be 9", 9, previousCacheSize);

        // 2. Steady state round (add 3 nodes, clear 2 nodes)
        cacheWithRecorder.addNode(11, fillEmptyAccessibilityNodeInfoCompat("node11", "11"));
        cacheWithRecorder.addNode(12, fillEmptyAccessibilityNodeInfoCompat("node12", "12"));
        cacheWithRecorder.addNode(13, fillEmptyAccessibilityNodeInfoCompat("node13", "13"));
        cacheWithRecorder.clearNode(11, /* recursive= */ false);
        cacheWithRecorder.clearNode(1, /* recursive= */ false);

        // At this point, previous cache size remains 9 (the size at the end of Round 1).
        //
        // 1. Churn Calculation:
        // - Removals count = 2 (node11 and node1 were cleared in this round).
        // - previousCacheSize = 9 (retained from Round 1 baseline).
        // - Churn Percentage = (removals * 100) / previousCacheSize
        //                    = (2 * 100) / 9
        //                    = 22.22% -> Truncated to 22%.
        //
        // 2. Thrashing Calculation:
        // - Additions count = 3 (node11, node12, node13 were added).
        // - Intersection count = 1 (only node11 was both added and removed in this same round).
        // - Thrashing Percentage = (intersection * 100) / additions
        //                        = (1 * 100) / 3
        //                        = 33.33% -> Truncated to 33%.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN,
                                22)
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_THRASHING,
                                33)
                        .build();

        // Trigger Round 2 validation
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify histograms were successfully recorded
        histogramWatcher.assertExpected();

        // Verify that sets are cleared and final cache size baseline is updated to 10
        int finalCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals(
                "Steady state final cache size baseline should be 10", 10, finalCacheSize);
    }

    @Test
    @SmallTest
    public void testScenarioConsolidatedRound() {
        AccessibilityHistogramRecorder recorder = new AccessibilityHistogramRecorder();
        FakeAndroidCache cacheWithRecorder =
                new FakeAndroidCache(mWebContentsAccessibility, recorder);

        // Stub getAccessibilityTreeSizeForExperiment so validation doesn't return early
        Mockito.when(mWebContentsAccessibility.getAccessibilityTreeSizeForExperiment())
                .thenReturn(30L);

        // Round 1
        for (int i = 1; i <= 10; i++) {
            cacheWithRecorder.addNode(
                    i, fillEmptyAccessibilityNodeInfoCompat("node" + i, String.valueOf(i)));
        }
        cacheWithRecorder.clearNode(10, /* recursive= */ false);

        // Triggers Round 1 validation (clears sets, updates baseline to 9)
        cacheWithRecorder.validateAccessibilityForExperiment();
        int previousCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals("Consolidated previous cache size should be 9", 9, previousCacheSize);

        // Round 2: Addition-only round (add 5 nodes, no removals)
        for (int i = 11; i <= 15; i++) {
            cacheWithRecorder.addNode(
                    i, fillEmptyAccessibilityNodeInfoCompat("node" + i, String.valueOf(i)));
        }

        // Round 2 is addition-only, so we expect no recordings to be made
        var round2Watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN)
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_THRASHING)
                        .build();

        // Triggers Round 2 validation (skips recording, does not clear sets, baseline remains 9)
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify that indeed no histograms were recorded
        round2Watcher.assertExpected();

        previousCacheSize = recorder.getPreviousCacheSizeForTesting();
        // Only added nodes so previous cache size remains 9
        Assert.assertEquals("Consolidated previous cache size should be 9", 9, previousCacheSize);

        // Round 3: Recorded round (add 3 nodes, clear 2 nodes)
        cacheWithRecorder.addNode(16, fillEmptyAccessibilityNodeInfoCompat("node16", "16"));
        cacheWithRecorder.addNode(17, fillEmptyAccessibilityNodeInfoCompat("node17", "17"));
        cacheWithRecorder.addNode(18, fillEmptyAccessibilityNodeInfoCompat("node18", "18"));
        cacheWithRecorder.clearNode(16, /* recursive= */ false);
        cacheWithRecorder.clearNode(1, /* recursive= */ false);

        // At this point, Round 2 (skipped) and Round 3 are consolidated since sets were not
        // cleared.
        //
        // 1. Churn Calculation:
        // - Total removals count = 2 (node16 and node1 were cleared).
        // - previousCacheSize = 9 (retained from Round 1 baseline).
        // - Churn Percentage = (removals * 100) / previousCacheSize
        //                    = (2 * 100) / 9
        //                    = 22.22% -> Truncated to 22%.
        //
        // 2. Thrashing Calculation:
        // - Total additions count = 8 (5 from Round 2 + 3 from Round 3).
        // - Intersection count = 1 (only node16 was both added and removed since last clear).
        // - Thrashing Percentage = (intersection * 100) / additions
        //                        = (1 * 100) / 8
        //                        = 12.5% -> Truncated to 12%.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN,
                                22)
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_THRASHING,
                                12)
                        .build();

        // Trigger Round 3 validation
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify histograms were successfully recorded
        histogramWatcher.assertExpected();

        // Verify that sets are cleared and final cache size baseline is updated to 15
        int finalCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals(
                "Consolidated final cache size baseline should be 15", 15, finalCacheSize);
    }

    @Test
    @SmallTest
    public void testScenarioEmptyCacheStartup() {
        AccessibilityHistogramRecorder recorder = new AccessibilityHistogramRecorder();
        FakeAndroidCache cacheWithRecorder =
                new FakeAndroidCache(mWebContentsAccessibility, recorder);

        // Stub getAccessibilityTreeSizeForExperiment so validation doesn't return early
        Mockito.when(mWebContentsAccessibility.getAccessibilityTreeSizeForExperiment())
                .thenReturn(10L);

        // Round 1: Addition-only round on empty cache (add 3 nodes, no removals)
        cacheWithRecorder.addNode(1, fillEmptyAccessibilityNodeInfoCompat("node1", "1"));
        cacheWithRecorder.addNode(2, fillEmptyAccessibilityNodeInfoCompat("node2", "2"));
        cacheWithRecorder.addNode(3, fillEmptyAccessibilityNodeInfoCompat("node3", "3"));

        // Round 1 is addition-only, so we expect no recordings to be made
        var round1Watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN)
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_THRASHING)
                        .build();

        // Triggers Round 1 validation (skips recording, does not clear sets, baseline is 0)
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify that indeed no histograms were recorded
        round1Watcher.assertExpected();

        // Verify previousCacheSize right before Round 2 validation is 0 (relative to empty startup)
        int previousCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals("Startup previous cache size should be 0", 0, previousCacheSize);

        // Round 2: First round with removals (add 2 nodes, clear 2 nodes)
        cacheWithRecorder.addNode(4, fillEmptyAccessibilityNodeInfoCompat("node4", "4"));
        cacheWithRecorder.addNode(5, fillEmptyAccessibilityNodeInfoCompat("node5", "5"));
        cacheWithRecorder.clearNode(4, /* recursive= */ false);
        cacheWithRecorder.clearNode(2, /* recursive= */ false);

        // Verify previousCacheSize right before Round 2 validation is 0 (relative to empty startup)
        previousCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals("Startup previous cache size should be 0", 0, previousCacheSize);

        // Round 2 has removals, but since previous cache size before consolidated operations was 0
        // (startup),
        // calculating churn or thrashing on 0 is undefined, so it should also skip recording
        // histograms.
        var round2Watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN)
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_THRASHING)
                        .build();

        // Trigger Round 2 validation (sets are cleared despite previousCacheSize being 0)
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify that indeed no histograms were recorded
        round2Watcher.assertExpected();

        // Verify that sets are cleared and final cache size baseline is updated to 3
        int finalCacheSize = recorder.getPreviousCacheSizeForTesting();
        Assert.assertEquals("Startup final cache size baseline should be 3", 3, finalCacheSize);
    }

    private AccessibilityNodeInfoCompat fillEmptyAccessibilityNodeInfoCompat(
            String text, String uniqueId) {
        // Obtain an empty AccessibilityNodeInfo object.
        AccessibilityNodeInfoCompat nodeInfo =
                new AccessibilityNodeInfoCompat(AccessibilityNodeInfo.obtain());
        nodeInfo.setText(text);
        nodeInfo.setUniqueId(uniqueId);
        return nodeInfo;
    }
}
