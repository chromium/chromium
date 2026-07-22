// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;
import android.os.SystemClock;
import android.view.accessibility.AccessibilityEvent;
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
    private AccessibilityHistogramRecorder mHistogramRecorder;
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
        mHistogramRecorder = new AccessibilityHistogramRecorder();
        mFakeAndroidCache = new FakeAndroidCache(mWebContentsAccessibility, mHistogramRecorder);
    }

    @Test
    @SmallTest
    public void testAddNodeAndRemoveIt() {
        // Create a test node and remove it from the cache.
        AccessibilityNodeInfoCompat testNode =
                fillEmptyAccessibilityNodeInfoCompat("testNode", String.valueOf(mFirstNodeId));
        mFakeAndroidCache.addNode(mFirstNodeId, testNode);
        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        mFakeAndroidCache.clearNodeOnEvent(
                createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), mFirstNodeId);

        // To make sure its not in the cache, we make sure to mark is as stale by updating the
        // expected node to be returned if ever built again.
        AccessibilityNodeInfoCompat testNodeUpdated =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated", String.valueOf(mFirstNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mFirstNodeId))
                .thenReturn(testNodeUpdated);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_STALE_NODES,
                                0)
                        .build();

        mFakeAndroidCache.validateAccessibilityForExperiment();

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAddedToCacheTimestamp() {
        // Verify that adding a node to the cache records a valid timestamp.
        long startTime = SystemClock.elapsedRealtime();
        AccessibilityNodeInfoCompat testNode =
                fillEmptyAccessibilityNodeInfoCompat("testNode", String.valueOf(mFirstNodeId));
        mFakeAndroidCache.addNode(mFirstNodeId, testNode);
        long endTime = SystemClock.elapsedRealtime();

        long addedTimestamp = mFakeAndroidCache.getAddedToCacheTimestampForTesting(mFirstNodeId);
        Assert.assertTrue(addedTimestamp >= startTime);
        Assert.assertTrue(addedTimestamp <= endTime);

        // A non-existent node should return -1.
        Assert.assertEquals(
                -1, mFakeAndroidCache.getAddedToCacheTimestampForTesting(mSecondNodeId));
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

        // It is 100% stale since we only have one single node and its stale.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_STALE_NODES,
                                100)
                        .build();

        mFakeAndroidCache.validateAccessibilityForExperiment();

        histogramWatcher.assertExpected();
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

        // Use TYPE_VIEW_SCROLLED event to clear the node recursively.
        mFakeAndroidCache.clearNodeOnEvent(
                createEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED), mFirstNodeId);

        // If recreated, it should show up as stale when validating by returning a different node.
        AccessibilityNodeInfoCompat testNodeUpdated2 =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated2", String.valueOf(mSecondNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mSecondNodeId))
                .thenReturn(testNodeUpdated2);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_STALE_NODES,
                                0)
                        .build();

        mFakeAndroidCache.validateAccessibilityForExperiment();

        histogramWatcher.assertExpected();
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

        AccessibilityNodeInfoCompat testNode3 =
                fillEmptyAccessibilityNodeInfoCompat("testNode3", String.valueOf(mThirdNodeId));
        mFakeAndroidCache.addNode(mThirdNodeId, testNode3);

        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        mFakeAndroidCache.clearNodeOnEvent(
                createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), mFirstNodeId);

        // Make stale by making the expectation return a different node.
        AccessibilityNodeInfoCompat testNodeUpdated2 =
                fillEmptyAccessibilityNodeInfoCompat(
                        "testNodeUpdated2", String.valueOf(mSecondNodeId));
        Mockito.when(mWebContentsAccessibility.buildFreshAccessibilityNodeInfo(mSecondNodeId))
                .thenReturn(testNodeUpdated2);

        // It is 50% stale since even if the parent node was removed, testNode3 is still here.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_STALE_NODES,
                                50)
                        .build();

        mFakeAndroidCache.validateAccessibilityForExperiment();

        histogramWatcher.assertExpected();
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

        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        mFakeAndroidCache.clearNodeOnEvent(
                createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), mSecondNodeId);

        var histogramWatcher1 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_STALE_NODES,
                                0)
                        .build();

        mFakeAndroidCache.validateAccessibilityForExperiment();
        histogramWatcher1.assertExpected();

        // We then recursive clear node1, and expect for grand children to be cleared too.
        // Use TYPE_VIEW_SCROLLED event to clear the node recursively.
        mFakeAndroidCache.clearNodeOnEvent(
                createEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED), mFirstNodeId);

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

        var histogramWatcher2 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_STALE_NODES,
                                0)
                        .build();

        // There should be no stale nodes, we cleared all the cache.
        mFakeAndroidCache.validateAccessibilityForExperiment();
        histogramWatcher2.assertExpected();
    }

    @Test
    @SmallTest
    public void testScenarioSteadyState() {
        AccessibilityHistogramRecorder recorder = new AccessibilityHistogramRecorder();
        FakeAndroidCache cacheWithRecorder =
                new FakeAndroidCache(mWebContentsAccessibility, recorder);

        // 1. Baseline batch (add 10 nodes, remove 1 node).
        for (int i = 1; i <= 10; i++) {
            cacheWithRecorder.addNode(
                    i, fillEmptyAccessibilityNodeInfoCompat("node" + i, String.valueOf(i)));
        }
        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        cacheWithRecorder.clearNodeOnEvent(
                createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), 10); // Cache size becomes 9.

        cacheWithRecorder
                .validateAccessibilityForExperiment(); // Triggers Batch 1 validation (clears
        // removals count,
        // updates cacheSize to 9).

        int peakCacheNodesInBatch = cacheWithRecorder.getPeakCacheNodesInBatchForTesting();
        Assert.assertEquals(
                "Steady state peakCacheNodesInBatch cache size should be 9",
                9,
                peakCacheNodesInBatch);

        // 2. Steady state batch (add 3 nodes, clear 2 nodes).
        cacheWithRecorder.addNode(11, fillEmptyAccessibilityNodeInfoCompat("node11", "11"));
        cacheWithRecorder.addNode(12, fillEmptyAccessibilityNodeInfoCompat("node12", "12"));
        cacheWithRecorder.addNode(13, fillEmptyAccessibilityNodeInfoCompat("node13", "13"));
        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        cacheWithRecorder.clearNodeOnEvent(createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), 11);
        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        cacheWithRecorder.clearNodeOnEvent(createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), 1);

        // 1. Churn Calculation:
        // - Removals count = 2 (node11 and node1 were cleared in this batch).
        // - peakCacheNodesInBatch = mCache.size() (10) + mRemovedNodeCount (2) = 12.
        // - Churn Percentage = (removals * 100) / peakCacheNodesInBatch
        //                    = (2 * 100) / 12
        //                    = 16.67% -> Truncated to 16%.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN,
                                16)
                        .build();

        // Trigger Batch 2 validation.
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify histograms were successfully recorded.
        histogramWatcher.assertExpected();

        // Verify that removals count is reset to 0, making peakCacheNodesInBatch cache size equal
        // to
        // current cache
        // size (10).
        int finalCacheSize = cacheWithRecorder.getPeakCacheNodesInBatchForTesting();
        Assert.assertEquals(
                "Steady state final peakCacheNodesInBatch cache size should be 10",
                10,
                finalCacheSize);
    }

    @Test
    @SmallTest
    public void testScenarioEmptyCacheStartup() {
        AccessibilityHistogramRecorder recorder = new AccessibilityHistogramRecorder();
        FakeAndroidCache cacheWithRecorder =
                new FakeAndroidCache(mWebContentsAccessibility, recorder);

        // Batch 1: Addition-only batch on empty cache (add 3 nodes, no removals).
        cacheWithRecorder.addNode(1, fillEmptyAccessibilityNodeInfoCompat("node1", "1"));
        cacheWithRecorder.addNode(2, fillEmptyAccessibilityNodeInfoCompat("node2", "2"));
        cacheWithRecorder.addNode(3, fillEmptyAccessibilityNodeInfoCompat("node3", "3"));

        // Batch 1 is addition-only, so churn shouldn't have been recorded.
        var batch1Watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN)
                        .build();

        // Triggers Batch 1 validation (skips recording churn because removals count is 0, resets
        // removals count to 0).
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify that indeed no histograms were recorded.
        batch1Watcher.assertExpected();

        // Verify peakCacheNodesInBatch right before Batch 2 is 3 (current nodes 3 + removals 0).
        int peakCacheNodesInBatch = cacheWithRecorder.getPeakCacheNodesInBatchForTesting();
        Assert.assertEquals(
                "Startup peakCacheNodesInBatch cache size should be 3", 3, peakCacheNodesInBatch);

        // Batch 2: First batch with removals (add 2 nodes, clear 2 nodes).
        cacheWithRecorder.addNode(4, fillEmptyAccessibilityNodeInfoCompat("node4", "4"));
        cacheWithRecorder.addNode(5, fillEmptyAccessibilityNodeInfoCompat("node5", "5"));
        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        cacheWithRecorder.clearNodeOnEvent(createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), 4);
        // Use TYPE_VIEW_CLICKED event to clear the node non-recursively.
        cacheWithRecorder.clearNodeOnEvent(createEvent(AccessibilityEvent.TYPE_VIEW_CLICKED), 2);

        // Verify peakCacheNodesInBatch right before Batch 2 validation is 5 (current nodes 3 +
        // removals
        // 2).
        peakCacheNodesInBatch = cacheWithRecorder.getPeakCacheNodesInBatchForTesting();
        Assert.assertEquals(
                "Startup peakCacheNodesInBatch cache size should be 5", 5, peakCacheNodesInBatch);

        // Batch 2 has removals.
        // Churn = (removals * 100) / peakCacheNodesInBatch
        //       = (2 * 100) / (3 + 2) = 40%.
        var batch2Watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AccessibilityHistogramRecorder
                                        .ACCESSIBILITY_FAKE_CACHE_PERCENTAGE_CHURN,
                                40)
                        .build();

        // Trigger Batch 2 validation (records churn and resets removals count).
        cacheWithRecorder.validateAccessibilityForExperiment();

        // Verify that churn histogram was successfully recorded.
        batch2Watcher.assertExpected();

        // Verify that removals count is reset to 0, making peakCacheNodesInBatch cache size equal
        // to
        // current cache
        // size (3).
        int finalCacheSize = cacheWithRecorder.getPeakCacheNodesInBatchForTesting();
        Assert.assertEquals(
                "Startup final peakCacheNodesInBatch cache size should be 3", 3, finalCacheSize);
    }

    private AccessibilityEvent createEvent(int eventType) {
        return AccessibilityEvent.obtain(eventType);
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
