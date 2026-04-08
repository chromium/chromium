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
