// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_OFFSCREEN;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_SUPPORTED_ELEMENTS;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_UNCLIPPED_BOTTOM;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_UNCLIPPED_TOP;

import static java.lang.String.CASE_INSENSITIVE_ORDER;

import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityNodeInfo;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Utility class for common actions involving AccessibilityNodeInfo objects.
 */
public class AccessibilityNodeInfoUtils {
    /**
     * Helper method to perform a custom toString on a given AccessibilityNodeInfo object.
     *
     * @param node Object to create a toString for
     * @return String Custom toString result for the given object
     */
    public static String toString(AccessibilityNodeInfo node) {
        // Guard against null inputs.
        if (node == null) return "";

        StringBuilder builder = new StringBuilder();

        // Print classname first, but only print content after the last period to remove redundancy.
        assert node.getClassName() != null : "Classname should never be null";
        assert !node.getClassName().toString().contains("\\.") : "Classname should contain periods";
        String[] classNameParts = node.getClassName().toString().split("\\.");
        builder.append(classNameParts[classNameParts.length - 1]);

        // Print text unless it is empty (null is allowed).
        if (node.getText() == null) {
            builder.append(" text:\"null\"");
        } else if (!node.getText().toString().isEmpty()) {
            builder.append(" text:\"")
                    .append(node.getText().toString().replace("\n", "\\n"))
                    .append("\"");
        }

        // Text properties - Only print when non-null.
        if (node.getContentDescription() != null) {
            builder.append(" contentDescription:\"")
                    .append(node.getContentDescription().toString().replace("\n", "\\n"))
                    .append("\"");
        }
        if (node.getViewIdResourceName() != null) {
            builder.append(" viewIdResName:\"").append(node.getViewIdResourceName()).append("\"");
        }
        if (node.getError() != null) {
            builder.append(" error:\"").append(node.getError()).append("\"");
        }

        // Boolean properties - Only print when set to true except for enabled and visibleToUser,
        // which are both mostly true, so only print when they are false.
        if (node.canOpenPopup()) {
            builder.append(" canOpenPopUp");
        }
        if (node.isCheckable()) {
            builder.append(" checkable");
        }
        if (node.isChecked()) {
            builder.append(" checked");
        }
        if (node.isClickable()) {
            builder.append(" clickable");
        }
        if (node.isContentInvalid()) {
            builder.append(" contentInvalid");
        }
        if (node.isDismissable()) {
            builder.append(" dismissable");
        }
        if (node.isEditable()) {
            builder.append(" editable");
        }
        if (!node.isEnabled()) {
            builder.append(" disabled");
        }
        if (node.isFocusable()) {
            builder.append(" focusable");
        }
        if (node.isFocused()) {
            builder.append(" focused");
        }
        if (node.isMultiLine()) {
            builder.append(" multiLine");
        }
        if (node.isPassword()) {
            builder.append(" password");
        }
        if (node.isScrollable()) {
            builder.append(" scrollable");
        }
        if (node.isSelected()) {
            builder.append(" selected");
        }
        if (!node.isVisibleToUser()) {
            builder.append(" notVisibleToUser");
        }

        // Integer properties - Only print when not default values.
        if (node.getInputType() != InputType.TYPE_NULL) {
            builder.append(" inputType:").append(node.getInputType());
        }
        if (node.getTextSelectionStart() != -1) {
            builder.append(" textSelectionStart:").append(node.getTextSelectionStart());
        }
        if (node.getTextSelectionEnd() != -1) {
            builder.append(" textSelectionEnd:").append(node.getTextSelectionEnd());
        }
        if (node.getMaxTextLength() != -1) {
            builder.append(" maxTextLength:").append(node.getMaxTextLength());
        }

        // Child objects - print for non-null cases.
        if (node.getCollectionInfo() != null) {
            builder.append(" CollectionInfo:").append(toString(node.getCollectionInfo()));
        }
        if (node.getCollectionItemInfo() != null) {
            builder.append(" CollectionItemInfo:").append(toString(node.getCollectionItemInfo()));
        }
        if (node.getRangeInfo() != null) {
            builder.append(" RangeInfo:").append(toString(node.getRangeInfo()));
        }

        // Actions and Bundle extras - Always print.
        builder.append(" actions:").append(toString(node.getActionList()));
        builder.append(" bundle:").append(toString(node.getExtras()));

        return builder.toString();
    }

    // Various helper methods to print custom toStrings for objects.
    private static String toString(AccessibilityNodeInfo.CollectionInfo info) {
        // Only include the isHierarchical boolean if true, since it is more often false, and
        // ignore selection mode, which is not set by Chrome.
        String prefix = "[";
        if (info.isHierarchical()) {
            prefix += "hierarchical, ";
        }
        return String.format(
                "%srows=%s, cols=%s]", prefix, info.getRowCount(), info.getColumnCount());
    }

    private static String toString(AccessibilityNodeInfo.CollectionItemInfo info) {
        // Only include isHeading and isSelected if true, since both are more often false.
        String prefix = "[";
        if (info.isHeading()) {
            prefix += "heading, ";
        }
        if (info.isSelected()) {
            prefix += "selected, ";
        }
        return String.format("%srowIndex=%s, rowSpan=%s, colIndex=%s, colSpan=%s]", prefix,
                info.getRowIndex(), info.getRowSpan(), info.getColumnIndex(), info.getColumnSpan());
    }

    private static String toString(AccessibilityNodeInfo.RangeInfo info) {
        // Chrome always uses the float range type, so only print values of RangeInfo.
        return String.format(
                "[current=%s, min=%s, max=%s]", info.getCurrent(), info.getMin(), info.getMax());
    }

    private static String toString(List<AccessibilityNodeInfo.AccessibilityAction> actionList) {
        // Sort actions list to ensure consistent output of tests.
        Collections.sort(actionList, (a1, b2) -> Integer.compare(a1.getId(), b2.getId()));

        List<String> actionStrings = new ArrayList<String>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (AccessibilityNodeInfo.AccessibilityAction action : actionList) {
            // Four actions are set on all nodes, so ignore those when printing the tree.
            if (action.getId() == AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT
                    || action.getId() == AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT
                    || action.getId() == WebContentsAccessibilityImpl.ACTION_SHOW_ON_SCREEN
                    || action.getId() == WebContentsAccessibilityImpl.ACTION_CONTEXT_CLICK) {
                continue;
            }

            actionStrings.add(toString(action.getId()));
        }
        builder.append(TextUtils.join(", ", actionStrings)).append("]");

        return builder.toString();
    }

    private static String toString(int action) {
        switch (action) {
            // These could potentially be added to any given node.
            case AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY:
                return "NEXT";
            case AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY:
                return "PREVIOUS";
            case AccessibilityNodeInfo.ACTION_SET_TEXT:
                return "SET_TEXT";
            case AccessibilityNodeInfo.ACTION_PASTE:
                return "PASTE";
            case WebContentsAccessibilityImpl.ACTION_IME_ENTER:
                return "IME_ENTER";
            case AccessibilityNodeInfo.ACTION_SET_SELECTION:
                return "SET_SELECTION";
            case AccessibilityNodeInfo.ACTION_CUT:
                return "CUT";
            case AccessibilityNodeInfo.ACTION_COPY:
                return "COPY";
            case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
                return "SCROLL_FORWARD";
            case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
                return "SCROLL_BACKWARD";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_UP:
                return "SCROLL_UP";
            case WebContentsAccessibilityImpl.ACTION_PAGE_UP:
                return "PAGE_UP";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_DOWN:
                return "SCROLL_DOWN";
            case WebContentsAccessibilityImpl.ACTION_PAGE_DOWN:
                return "PAGE_DOWN";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_LEFT:
                return "SCROLL_LEFT";
            case WebContentsAccessibilityImpl.ACTION_PAGE_LEFT:
                return "PAGE_LEFT";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_RIGHT:
                return "SCROLL_RIGHT";
            case WebContentsAccessibilityImpl.ACTION_PAGE_RIGHT:
                return "PAGE_RIGHT";
            case AccessibilityNodeInfo.ACTION_CLEAR_FOCUS:
                return "CLEAR_FOCUS";
            case AccessibilityNodeInfo.ACTION_FOCUS:
                return "FOCUS";
            case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                return "CLEAR_AX_FOCUS";
            case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                return "AX_FOCUS";
            case AccessibilityNodeInfo.ACTION_CLICK:
                return "CLICK";
            case AccessibilityNodeInfo.ACTION_EXPAND:
                return "EXPAND";
            case AccessibilityNodeInfo.ACTION_COLLAPSE:
                return "COLLAPSE";
            case WebContentsAccessibilityImpl.ACTION_SET_PROGRESS:
                return "SET_PROGRESS";

            // The long click action is deliberately never be added to a node.
            case AccessibilityNodeInfo.ACTION_LONG_CLICK:
            // These are the remaining potential actions which Chrome does not implement.
            case AccessibilityNodeInfo.ACTION_DISMISS:
            case AccessibilityNodeInfo.ACTION_SELECT:
            case AccessibilityNodeInfo.ACTION_CLEAR_SELECTION:
            case WebContentsAccessibilityImpl.ACTION_SCROLL_TO_POSITION:
            case WebContentsAccessibilityImpl.ACTION_MOVE_WINDOW:
            case WebContentsAccessibilityImpl.ACTION_SHOW_TOOLTIP:
            case WebContentsAccessibilityImpl.ACTION_HIDE_TOOLTIP:
            case WebContentsAccessibilityImpl.ACTION_PRESS_AND_HOLD:
            default:
                return "NOT_IMPLEMENTED";
        }
    }

    private static String toString(Bundle extras) {
        // Sort keys to ensure consistent output of tests.
        List<String> sortedKeySet = new ArrayList<String>(extras.keySet());
        Collections.sort(sortedKeySet, CASE_INSENSITIVE_ORDER);

        List<String> bundleStrings = new ArrayList<>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (String key : sortedKeySet) {
            // Two Bundle extras are related to bounding boxes, these should be ignored so the
            // tests can safely run on varying devices and not be screen-dependent.
            if (key.equals(EXTRAS_KEY_UNCLIPPED_TOP) || key.equals(EXTRAS_KEY_UNCLIPPED_BOTTOM)) {
                continue;
            }

            // Since every node has a few Bundle extras, and some are often empty, we will only
            // print non-null and not empty values.
            if (extras.get(key) == null || extras.get(key).toString().isEmpty()) {
                continue;
            }

            // For the special case of the supported HTML elements, which prints the same for the
            // rootWebArea on each test, assert consistency and suppress from results.
            if (key.equals(EXTRAS_KEY_SUPPORTED_ELEMENTS)) {
                continue;
            }

            // To prevent flakiness or dependency on screensize/form factor, drop the "offscreen"
            // Bundle extra.
            if (key.equals(EXTRAS_KEY_OFFSCREEN)) {
                continue;
            }

            // Simplify the key String before printing to make test outputs easier to read.
            bundleStrings.add(key.replace("AccessibilityNodeInfo.", "") + "=\""
                    + extras.get(key).toString() + "\"");
        }
        builder.append(TextUtils.join(", ", bundleStrings)).append("]");

        return builder.toString();
    }
}