// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

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
}
