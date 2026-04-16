// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;
import android.os.Parcel;

import androidx.annotation.Nullable;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A fake cache for {@link AccessibilityNodeInfoCompat} objects. This is used to simulate the
 * behavior of the Android framework's cache. This must be used with Android Tiramisu and above.
 */
@JNINamespace("content")
@NullMarked
public class FakeAndroidCache {
    private final Map<Integer, CachedNodeState> mCache = new HashMap<>();
    private final WebContentsAccessibilityImpl mWebContentsAccessibilityImpl;
    @Nullable private final AccessibilityHistogramRecorder mHistogramRecorder;
    // Only for testing
    private int mStaleNodeCount;

    public FakeAndroidCache(WebContentsAccessibilityImpl webContentsAccessibilityImpl) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            throw new UnsupportedOperationException(
                    "FakeAndroidCache is only available on Android Tiramisu and above.");
        }
        mWebContentsAccessibilityImpl = webContentsAccessibilityImpl;
        mHistogramRecorder = null;
    }

    public FakeAndroidCache(
            WebContentsAccessibilityImpl webContentsAccessibilityImpl,
            AccessibilityHistogramRecorder histogramRecorder) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            throw new UnsupportedOperationException(
                    "FakeAndroidCache is only available on Android Tiramisu and above.");
        }
        mWebContentsAccessibilityImpl = webContentsAccessibilityImpl;
        mHistogramRecorder = histogramRecorder;
    }

    private static class CachedNodeState {
        // A safe copy of the node's state at the time of caching.
        final @Nullable byte[] mNodeInfoParcelData;

        // The virtual view IDs of the children at the time of caching.
        final List<Integer> mChildIds;

        private CachedNodeState(AccessibilityNodeInfoCompat mNodeInfo, List<Integer> childIds) {
            Parcel nodeParcel = Parcel.obtain();
            mNodeInfo.unwrap().writeToParcel(nodeParcel, 0);
            mNodeInfoParcelData = nodeParcel.marshall();
            mChildIds = new ArrayList<>(childIds);
        }
    }

    // Validate accessibility node info throughout the fake android cache.
    @CalledByNative
    public void validateAccessibilityForExperiment() {
        if (mHistogramRecorder != null) {
            // We must first calculate the total number of nodes.
            mHistogramRecorder.setTotalNodesCount(
                    mWebContentsAccessibilityImpl.getAccessibilityTreeSizeForExperiment());
            // Then the total number of nodes in the fake cache.
            mHistogramRecorder.setTotalFakeCacheNodesCount(mCache.size());
        }

        for (Map.Entry<Integer, CachedNodeState> entry : mCache.entrySet()) {
            int virtualViewId = entry.getKey();
            CachedNodeState cachedState = entry.getValue();
            // Build a completely fresh node for comparison.
            AccessibilityNodeInfoCompat freshInfo =
                    mWebContentsAccessibilityImpl.buildFreshAccessibilityNodeInfo(virtualViewId);
            if (freshInfo == null) {
                continue;
            }
            if (freshInfo.getUniqueId() == null) {
                throw new IllegalArgumentException("Unique ID for node info should not be null.");
            }
            // Avoid comparing against the root node given its event handing.
            if (freshInfo
                    .getUniqueId()
                    .equals(
                            String.valueOf(
                                    mWebContentsAccessibilityImpl
                                            .getCurrentRootIdForExperiment()))) {
                continue;
            }

            Parcel freshInfoParcel = Parcel.obtain();
            freshInfo.unwrap().writeToParcel(freshInfoParcel, 0);
            if (!Arrays.equals(freshInfoParcel.marshall(), cachedState.mNodeInfoParcelData)) {
                if (mHistogramRecorder != null) {
                    mHistogramRecorder.incrementStaleNodeOnFakeCache();
                }
                mStaleNodeCount++;
            }
        }
        if (mHistogramRecorder != null) {
            mHistogramRecorder.recordFakeCacheHistograms();
        }
    }

    /**
     * Adds a node to the fake Android cache for testing.
     *
     * @param virtualViewId The virtual view id of the node.
     */
    public void addNode(int virtualViewId, AccessibilityNodeInfoCompat nodeInfo) {
        if (nodeInfo == null) {
            return;
        }
        int[] childIds = mWebContentsAccessibilityImpl.getChildIdsForExperiment(virtualViewId);
        List<Integer> childIdList = new ArrayList<>(childIds != null ? childIds.length : 0);
        if (childIds != null) {
            for (int id : childIds) {
                childIdList.add(id);
            }
        }
        mCache.put(virtualViewId, new CachedNodeState(nodeInfo, childIdList));
        if (mHistogramRecorder != null) {
            mHistogramRecorder.reportNodeAddedToFakeCache(virtualViewId);
        }
    }

    // Clear the entire fake cache.
    private void clear() {
        if (mHistogramRecorder != null) {
            mCache.keySet().forEach(mHistogramRecorder::reportNodeRemovedFromFakeCache);
        }
        mCache.clear();
    }

    /**
     * Recursively clear this node and all its descendants from the fake cache.
     *
     * @param virtualViewId The virtual view id of the node.
     * @param recursive Indicate if all of the node's children should be cleared.
     */
    public void clearNode(int virtualViewId, boolean recursive) {
        CachedNodeState cachedNodeState = mCache.get(virtualViewId);
        if (cachedNodeState != null) {
            mCache.remove(virtualViewId);
            if (mHistogramRecorder != null) {
                mHistogramRecorder.reportNodeRemovedFromFakeCache(virtualViewId);
            }
            if (recursive) {
                for (int childId : cachedNodeState.mChildIds) {
                    clearNode(childId, true);
                }
            }
        } else {
            if (recursive) {
                // According to Android's AccessibilityCache, if a node is not found, children might
                // be
                // present so we clear all of the cache.
                clear();
            }
        }
    }

    /**
     * Checks if cache holds a node.
     *
     * @param virtualViewId The virtual view id of the node.
     * @return If the cache either holds it or not.
     */
    @CalledByNative
    private boolean isNodeLikelyKnownByAndroidFrameworkForExperiment(int virtualViewId) {
        return mCache.containsKey(virtualViewId);
    }

    /**
     * Returns the number of stale nodes in the fake Android cache.
     *
     * @return The number of stale nodes.
     */
    public int getStaleNodeCountForTesting() {
        return mStaleNodeCount;
    }
}
