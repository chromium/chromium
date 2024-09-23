// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CONTEXT_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COPY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CUT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_EXPAND;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_IME_ENTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_LONG_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PASTE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_BACKWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_FORWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_PROGRESS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_SELECTION;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_TEXT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SHOW_ON_SCREEN;

import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_CSS_DISPLAY;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_OFFSCREEN;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_SUPPORTED_ELEMENTS;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_BOTTOM;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_HEIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_LEFT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_RIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_WIDTH;

import static java.lang.String.CASE_INSENSITIVE_ORDER;

import android.graphics.Rect;
import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Utility class for common actions involving AccessibilityNodeInfo objects. */
public class AccessibilityNodeInfoUtils {
    /**
     * Helper method to perform a custom toString on a given AccessibilityNodeInfo object.
     *
     * @param node Object to create a toString for
     * @return String Custom toString result for the given object
     */
    public static String toString(
            AccessibilityNodeInfoCompat node, boolean includeScreenSizeDependentAttributes) {
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
        // Print hint unless it is null or empty.
        if (node.getHintText() != null && !node.getHintText().toString().isEmpty()) {
            builder.append(" hint:\"").append(node.getHintText()).append("\"");
        }

        // Text properties - Only print when non-null.
        if (node.getContentDescription() != null) {
            builder.append(" contentDescription:\"")
                    .append(node.getContentDescription().toString().replace("\n", "\\n"))
                    .append("\"");
        }
        if (node.getPaneTitle() != null) {
            builder.append(" paneTitle:\"").append(node.getPaneTitle()).append("\"");
        }
        if (node.getViewIdResourceName() != null) {
            builder.append(" viewIdResName:\"").append(node.getViewIdResourceName()).append("\"");
        }
        if (node.getError() != null) {
            builder.append(" error:\"").append(node.getError()).append("\"");
        }
        if (node.getStateDescription() != null
                && !node.getStateDescription().toString().isEmpty()) {
            builder.append(" stateDescription:\"").append(node.getStateDescription()).append("\"");
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
        if (node.isScrollable() && includeScreenSizeDependentAttributes) {
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
        builder.append(" actions:")
                .append(toString(node.getActionList(), includeScreenSizeDependentAttributes));
        builder.append(" bundle:")
                .append(toString(node.getExtras(), includeScreenSizeDependentAttributes));

        // Add bounds when including screen size dependent attributes.
        if (includeScreenSizeDependentAttributes) {
            Rect output = new Rect();
            node.getBoundsInScreen(output);
            builder.append(" bounds:[")
                    .append(output.left)
                    .append(", ")
                    .append(output.top)
                    .append(" - ")
                    .append(output.width())
                    .append("x")
                    .append(output.height())
                    .append("]");

            output = new Rect();
            node.getBoundsInParent(output);
            builder.append(" boundsInParent:[")
                    .append(output.left)
                    .append(", ")
                    .append(output.top)
                    .append(" - ")
                    .append(output.width())
                    .append("x")
                    .append(output.height())
                    .append("]");
        }

        return builder.toString();
    }

    // Various helper methods to print custom toStrings for objects.
    private static String toString(AccessibilityNodeInfoCompat.CollectionInfoCompat info) {
        // Only include the isHierarchical boolean if true, since it is more often false, and
        // ignore selection mode, which is not set by Chrome.
        String prefix = "[";
        if (info.isHierarchical()) {
            prefix += "hierarchical, ";
        }
        return String.format(
                "%srows=%s, cols=%s]", prefix, info.getRowCount(), info.getColumnCount());
    }

    private static String toString(AccessibilityNodeInfoCompat.CollectionItemInfoCompat info) {
        // Only include isHeading and isSelected if true, since both are more often false.
        String prefix = "[";
        if (info.isHeading()) {
            prefix += "heading, ";
        }
        if (info.isSelected()) {
            prefix += "selected, ";
        }
        // Only include row/col span if not equal to 1, the default value.
        if (info.getRowSpan() != 1) {
            prefix += String.format("rowSpan=%s, ", info.getRowSpan());
        }
        if (info.getColumnSpan() != 1) {
            prefix += String.format("colSpan=%s, ", info.getColumnSpan());
        }
        return String.format(
                "%srowIndex=%s, colIndex=%s]", prefix, info.getRowIndex(), info.getColumnIndex());
    }

    private static String toString(AccessibilityNodeInfoCompat.RangeInfoCompat info) {
        // Chrome always uses the float range type, so only print values of RangeInfo.
        return String.format(
                "[current=%s, min=%s, max=%s]", info.getCurrent(), info.getMin(), info.getMax());
    }

    private static String toString(
            List<AccessibilityNodeInfoCompat.AccessibilityActionCompat> actionList,
            boolean includeScreenSizeDependentAttributes) {
        // Sort actions list to ensure consistent output of tests.
        Collections.sort(actionList, (a1, b2) -> Integer.compare(a1.getId(), b2.getId()));

        List<String> actionStrings = new ArrayList<String>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (AccessibilityNodeInfoCompat.AccessibilityActionCompat action : actionList) {
            // Five actions are set on all nodes, so ignore those when printing the tree.
            if (action.equals(ACTION_NEXT_HTML_ELEMENT)
                    || action.equals(ACTION_PREVIOUS_HTML_ELEMENT)
                    || action.equals(ACTION_SHOW_ON_SCREEN)
                    || action.equals(ACTION_CONTEXT_CLICK)
                    || action.equals(ACTION_LONG_CLICK)) {
                continue;
            }

            // When |includeScreenSizeDependentAttributes| is true, we include all other actions,
            // so append and return early, otherwise continue to checks below.
            if (includeScreenSizeDependentAttributes) {
                actionStrings.add(toString(action.getId()));
                continue;
            }

            // Scroll actions are dependent on screen size, so ignore them to reduce flakiness
            if (action.equals(ACTION_SCROLL_FORWARD)
                    || action.equals(ACTION_SCROLL_BACKWARD)
                    || action.equals(ACTION_SCROLL_DOWN)
                    || action.equals(ACTION_SCROLL_UP)
                    || action.equals(ACTION_SCROLL_RIGHT)
                    || action.equals(ACTION_SCROLL_LEFT)) {
                continue;
            }
            // Page actions are dependent on screen size, so ignore them to reduce flakiness.
            if (action.equals(ACTION_PAGE_UP)
                    || action.equals(ACTION_PAGE_DOWN)
                    || action.equals(ACTION_PAGE_LEFT)
                    || action.equals(ACTION_PAGE_RIGHT)) {
                continue;
            }

            actionStrings.add(toString(action.getId()));
        }
        builder.append(TextUtils.join(", ", actionStrings)).append("]");

        return builder.toString();
    }

    public static String toString(int action) {
        if (action == ACTION_NEXT_AT_MOVEMENT_GRANULARITY.getId()) {
            return "NEXT";
        } else if (action == ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY.getId()) {
            return "PREVIOUS";
        } else if (action == ACTION_SET_TEXT.getId()) {
            return "SET_TEXT";
        } else if (action == ACTION_PASTE.getId()) {
            return "PASTE";
        } else if (action == ACTION_IME_ENTER.getId()) {
            return "IME_ENTER";
        } else if (action == ACTION_SET_SELECTION.getId()) {
            return "SET_SELECTION";
        } else if (action == ACTION_CUT.getId()) {
            return "CUT";
        } else if (action == ACTION_COPY.getId()) {
            return "COPY";
        } else if (action == ACTION_SCROLL_FORWARD.getId()) {
            return "SCROLL_FORWARD";
        } else if (action == ACTION_SCROLL_BACKWARD.getId()) {
            return "SCROLL_BACKWARD";
        } else if (action == ACTION_SCROLL_UP.getId()) {
            return "SCROLL_UP";
        } else if (action == ACTION_PAGE_UP.getId()) {
            return "PAGE_UP";
        } else if (action == ACTION_SCROLL_DOWN.getId()) {
            return "SCROLL_DOWN";
        } else if (action == ACTION_PAGE_DOWN.getId()) {
            return "PAGE_DOWN";
        } else if (action == ACTION_SCROLL_LEFT.getId()) {
            return "SCROLL_LEFT";
        } else if (action == ACTION_PAGE_LEFT.getId()) {
            return "PAGE_LEFT";
        } else if (action == ACTION_SCROLL_RIGHT.getId()) {
            return "SCROLL_RIGHT";
        } else if (action == ACTION_PAGE_RIGHT.getId()) {
            return "PAGE_RIGHT";
        } else if (action == ACTION_CLEAR_FOCUS.getId()) {
            return "CLEAR_FOCUS";
        } else if (action == ACTION_FOCUS.getId()) {
            return "FOCUS";
        } else if (action == ACTION_CLEAR_ACCESSIBILITY_FOCUS.getId()) {
            return "CLEAR_AX_FOCUS";
        } else if (action == ACTION_ACCESSIBILITY_FOCUS.getId()) {
            return "AX_FOCUS";
        } else if (action == ACTION_CLICK.getId()) {
            return "CLICK";
        } else if (action == ACTION_EXPAND.getId()) {
            return "EXPAND";
        } else if (action == ACTION_COLLAPSE.getId()) {
            return "COLLAPSE";
        } else if (action == ACTION_SET_PROGRESS.getId()) {
            return "SET_PROGRESS";
        } else if (action == ACTION_LONG_CLICK.getId()) {
            return "LONG_CLICK";
        } else {
            return "NOT_IMPLEMENTED";
        }
        /*
         * The ACTION_LONG_CLICK click action is deliberately never be added to a node.
         * These are the remaining potential actions which Chrome does not implement:
         *  ACTION_DISMISS, ACTION_SELECT, ACTION_CLEAR_SELECTION, ACTION_SCROLL_TO_POSITION,
         *  ACTION_MOVE_WINDOW, ACTION_SHOW_TOOLTIP, ACTION_HIDE_TOOLTIP, ACTION_PRESS_AND_HOLD
         */
    }

    private static String toString(Bundle extras, boolean includeScreenSizeDependentAttributes) {
        // Sort keys to ensure consistent output of tests.
        List<String> sortedKeySet = new ArrayList<String>(extras.keySet());
        Collections.sort(sortedKeySet, CASE_INSENSITIVE_ORDER);

        List<String> bundleStrings = new ArrayList<>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (String key : sortedKeySet) {
            // Two Bundle extras are related to bounding boxes, these should be ignored so the
            // tests can safely run on varying devices and not be screen-dependent, unless
            // explicitly allowed for this instance.
            if (!includeScreenSizeDependentAttributes
                    && (key.equals(EXTRAS_KEY_UNCLIPPED_TOP)
                            || key.equals(EXTRAS_KEY_UNCLIPPED_BOTTOM)
                            || key.equals(EXTRAS_KEY_UNCLIPPED_LEFT)
                            || key.equals(EXTRAS_KEY_UNCLIPPED_RIGHT)
                            || key.equals(EXTRAS_KEY_UNCLIPPED_WIDTH)
                            || key.equals(EXTRAS_KEY_UNCLIPPED_HEIGHT))) {
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
            // Bundle extra, unless explicitly allowed for this instance.
            if (!includeScreenSizeDependentAttributes && key.equals(EXTRAS_KEY_OFFSCREEN)) {
                continue;
            }

            // The AccessibilityNodeInfoCompat class uses the extras for backwards compatibility,
            // so exclude anything that contains the classname in the key.
            if (key.contains("AccessibilityNodeInfoCompat")) {
                continue;
            }

            // CSS Display is very noisy and currently unused, so we exclude it here because we
            // don't have a way to filter it for certain tests.
            if (key.equals(EXTRAS_KEY_CSS_DISPLAY)) {
                continue;
            }

            // Simplify the key String before printing to make test outputs easier to read.
            bundleStrings.add(
                    key.replace("AccessibilityNodeInfo.", "")
                            + "=\""
                            + extras.get(key).toString()
                            + "\"");
        }
        builder.append(TextUtils.join(", ", bundleStrings)).append("]");

        return builder.toString();
    }
}
