// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.graphics.Rect;
import android.util.SparseArray;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.accessibility.AccessibilityNodeInfoCompatDumper;

import java.util.HashMap;
import java.util.Map;

/** Utility class for common actions involving AccessibilityNodeInfo objects. */
@JNINamespace("content")
@NullMarked
public final class AccessibilityNodeInfoUtils {
    // The threshold for occlusion. If the percentage of the node info that is occluded is above
    // this threshold, we will mark it as not visible.
    private static final float OCCLUSION_THRESHOLD = 0.75f;

    private AccessibilityNodeInfoUtils() {}

    @CalledByNative
    public static <K> Map<K, int[][]> createTextAttributeRangesMap() {
        return new HashMap<K, int[][]>();
    }

    @CalledByNative
    public static void setTextAttributeRangesMapFloatValue(
            Map<Float, int[][]> map, float value, int[] starts, int[] ends) {
        setTextAttributeRangesMapValue(map, value, starts, ends);
    }

    @CalledByNative
    public static void setTextAttributeRangesMapIntValue(
            Map<Integer, int[][]> map, int value, int[] starts, int[] ends) {
        setTextAttributeRangesMapValue(map, value, starts, ends);
    }

    @CalledByNative
    public static void setTextAttributeRangesMapStringValue(
            Map<String, int[][]> map, String value, int[] starts, int[] ends) {
        setTextAttributeRangesMapValue(map, value, starts, ends);
    }

    public static <T> void setTextAttributeRangesMapValue(
            Map<T, int[][]> map, T value, int[] starts, int[] ends) {
        if (map == null || value == null || starts == null || ends == null) {
            return;
        }
        map.put(value, new int[][] {starts, ends});
    }

    /**
     * Helper method to perform a custom toString on a given AccessibilityNodeInfo object.
     *
     * @param wcax WebContentsAccessibilityImpl object.
     * @param node Object to create a toString for
     * @return String Custom toString result for the given object
     */
    public static String toString(
            WebContentsAccessibilityImpl wcax,
            @Nullable AccessibilityNodeInfoCompat node,
            boolean includeScreenSizeDependentAttributes) {
        if (node == null) return "";

        StringBuilder builder =
                new StringBuilder(
                        AccessibilityNodeInfoCompatDumper.toString(
                                node, includeScreenSizeDependentAttributes));

        Object[] selection =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            int virtualViewId = Integer.parseInt(node.getUniqueId());
                            return wcax.getExtendedSelection(virtualViewId);
                        });
        if (selection != null) {
            AccessibilityNodeInfoCompat startNode = (AccessibilityNodeInfoCompat) selection[0];
            int startOffset = (int) selection[1];
            AccessibilityNodeInfoCompat endNode = (AccessibilityNodeInfoCompat) selection[2];
            int endOffset = (int) selection[3];

            if (startNode != null && startNode.equals(node)) {
                builder.append(" extendedSelectionStart:").append(startOffset);
            }
            if (endNode != null && endNode.equals(node)) {
                builder.append(" extendedSelectionEnd:").append(endOffset);
            }
        }

        return builder.toString();
    }

    /**
     * Determine if web content node is mostly occluded.
     *
     * @param info The {@link AccessibilityNodeInfoCompat} for which we want to check major
     *     occlusion.
     * @param occludingRects The set of rects that are occluding the view.
     */
    public static boolean isNodeMostlyOccluded(
            AccessibilityNodeInfoCompat info, SparseArray<Rect> occludingRects) {
        if (!info.isVisibleToUser()) return false;
        Rect rect = new Rect();
        info.getBoundsInScreen(rect);
        return resizedRectOnOcclusion(rect, occludingRects) == null;
    }

    /**
     * Handle occlusion for a node that is visible to accessibility. If the node is mostly occluded,
     * set its visibility to false. If the node is partially occluded, update its bounds to the
     * largest non-occluded rectangle.
     *
     * @param node The {@link AccessibilityNodeInfoCompat} for the web content node.
     * @param occludingRects The set of rects that are occluding the view.
     */
    public static void updateNodeForOcclusion(
            AccessibilityNodeInfoCompat node, SparseArray<Rect> occludingRects) {
        if (!node.isVisibleToUser()) return;
        Rect rect = new Rect();
        node.getBoundsInScreen(rect);

        Rect newBounds = resizedRectOnOcclusion(rect, occludingRects);
        if (newBounds == null) {
            node.setVisibleToUser(false);
        } else if (!newBounds.equals(rect)) {
            node.setBoundsInScreen(newBounds);
        }
    }

    /**
     * Calculate resized rect after accounting for occlusion.
     *
     * @param rect The {@link Rect} rect we want to calculate occlusion for.
     * @param occludingRects The set of rects that are occluding the view.
     */
    @Nullable
    public static Rect resizedRectOnOcclusion(Rect rect, SparseArray<Rect> occludingRects) {
        if (rect.isEmpty()) return rect;
        Rect occludingRect = largestOccludingRect(rect, occludingRects);
        if (occludingRect != null) {
            long nodeArea = (long) rect.width() * rect.height();
            long intersectionArea = (long) occludingRect.width() * occludingRect.height();

            if (((float) intersectionArea / nodeArea) > OCCLUSION_THRESHOLD) {
                return null;
            }
            Rect newBounds = remainingNonOccludedRect(rect, occludingRect);
            return newBounds != null ? newBounds : rect;
        }
        return rect;
    }

    /**
     * Calculate biggest possible occlusion for a rect belonging to a web content node.
     *
     * @param rect The {@link Rect} for which we want to find largest occluder.
     * @param occludingRects The set of rects that are occluding the view.
     */
    @Nullable
    public static Rect largestOccludingRect(final Rect rect, SparseArray<Rect> occludingRects) {
        if (rect.isEmpty()) return null;

        Rect highestOccludingRect = null;
        long highestOccludingRectArea = 0;
        for (int i = 0; i < occludingRects.size(); i++) {
            Rect occludingRect = occludingRects.valueAt(i);
            Rect intersection = new Rect(rect);
            if (intersection.intersect(occludingRect)) {
                long intersectionArea = (long) intersection.width() * intersection.height();
                if (intersectionArea > highestOccludingRectArea) {
                    highestOccludingRect = intersection;
                    highestOccludingRectArea = intersectionArea;
                }
            }
        }
        return highestOccludingRect;
    }

    /**
     * Return the largest rectangular portion of the rect that is not occluded by the occludingRect.
     *
     * @param rect The {@link Rect} of interest.
     * @param occludingRect The {@link Rect} that is potentially occluding.
     */
    @Nullable
    public static Rect remainingNonOccludedRect(final Rect rect, final Rect occludingRect) {
        // The occludingRect is the area of intersection between the node and an occluding view.
        // We need to update the node's bounds to be the largest unobscured rectangle.
        // The visible area is rect minus occludingRect. This can be complex,
        // so we find the largest of four possible rectangles surrounding the occlusion.

        // Rectangle to the left of the occlusion.
        Rect leftOfOcclusion = new Rect(rect.left, rect.top, occludingRect.left, rect.bottom);
        // Rectangle to the right of the occlusion.
        Rect rightOfOcclusion = new Rect(occludingRect.right, rect.top, rect.right, rect.bottom);
        // Rectangle above the occlusion.
        Rect topOfOcclusion = new Rect(rect.left, rect.top, rect.right, occludingRect.top);
        // Rectangle below the occlusion.
        Rect bottomOfOcclusion = new Rect(rect.left, occludingRect.bottom, rect.right, rect.bottom);

        Rect[] possibleBounds = {
            leftOfOcclusion, rightOfOcclusion, topOfOcclusion, bottomOfOcclusion
        };

        Rect newBounds = null;
        long maxArea = -1;

        for (Rect bounds : possibleBounds) {
            long area = (long) bounds.width() * bounds.height();
            if (area > maxArea) {
                maxArea = area;
                newBounds = bounds;
            }
        }

        // Only update if the new bounds are valid and smaller than the original.
        if (newBounds != null && newBounds.width() > 0 && newBounds.height() > 0) {
            long webNodeArea = (long) rect.width() * rect.height();
            if (maxArea < webNodeArea) {
                return newBounds;
            }
        }
        return null;
    }
}
