// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

/**
 * Helper class with various testing util methods for content shell accessibility tests.
 */
public class AccessibilityContentShellTestUtils {
    /**
     * Basic interface to define a way to match |AccessibilityNodeInfo| objects based on the
     * expected value of a given element of type T.
     * @param <T>               Generic type of the element for matching
     */
    public interface AccessibilityNodeInfoMatcher<T> {
        /**
         * Method used to check if a node matches the given element requirement.
         *
         * @param node              The node to test match with
         * @param element           The element and type we are matching against
         * @return                  True/false for whether or not the node is a match
         */
        boolean matches(AccessibilityNodeInfo node, T element);
    }

    // Helper methods for common matching conditions for AccessibilityNodeInfo objects.

    static AccessibilityNodeInfoMatcher<Integer> sInputTypeMatcher =
            (node, type) -> node.getInputType() == type;

    static AccessibilityNodeInfoMatcher<String> sClassNameMatcher =
            (node, className) -> node.getClassName().equals(className);

    static AccessibilityNodeInfoMatcher<String> sTextMatcher = (node, text) -> {
        if (node.getText() == null) return false;

        return node.getText().toString().equals(text);
    };

    static AccessibilityNodeInfoMatcher<String> sTextOrContentDescriptionMatcher = (node, text) -> {
        // If there is no content description, rely only on text
        if (node.getContentDescription() == null) {
            return text.equals(node.getText());
        }

        // If there is no text, rely only on content description
        if (node.getText() == null) {
            return text.equals(node.getContentDescription());
        }

        return text.equals(node.getText()) || text.equals(node.getContentDescription());
    };

    static AccessibilityNodeInfoMatcher<String> sTextVisibleToUserMatcher = (node, text) -> {
        if (node.getText() == null) return false;

        return node.getText().toString().equals(text) && node.isVisibleToUser();
    };

    static AccessibilityNodeInfoMatcher<String> sRangeInfoMatcher =
            (node, element) -> node.getRangeInfo() != null;

    // Helper methods for common AccessibilityEvent tracking conditions.

    public static View.AccessibilityDelegate textIndicesDelegate(
            AccessibilityContentShellTestData data) {
        return new View.AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                if (event.getEventType()
                        == AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY) {
                    data.setTraverseFromIndex(event.getFromIndex());
                    data.setTraverseToIndex(event.getToIndex());
                } else if (event.getEventType()
                        == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED) {
                    data.setSelectionFromIndex(event.getFromIndex());
                    data.setSelectionToIndex(event.getToIndex());
                }

                // Return false so that an accessibility event is not actually sent.
                return false;
            }
        };
    }

    public static View.AccessibilityDelegate announcementDelegate(
            AccessibilityContentShellTestData data) {
        return new View.AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                if (event.getEventType() == AccessibilityEvent.TYPE_ANNOUNCEMENT) {
                    data.setAnnouncementText(event.getText().get(0).toString());
                }

                // Return false so that an accessibility event is not actually sent.
                return false;
            }
        };
    }

    public static View.AccessibilityDelegate contentChangeDelegate(
            AccessibilityContentShellTestData data) {
        return new View.AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                if (event.getEventType() == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) {
                    data.incrementWindowContentChangedCount();
                }

                // Return false so that an accessibility event is not actually sent.
                return false;
            }
        };
    }

    public static View.AccessibilityDelegate inputRangeScrollDelegate(
            AccessibilityContentShellTestData data) {
        return new View.AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_SCROLLED) {
                    data.setReceivedEvent(true);
                }

                // Return false so that an accessibility event is not actually sent.
                return false;
            }
        };
    }

    public static View.AccessibilityDelegate accessibilityFocusDelegate(
            AccessibilityContentShellTestData data) {
        return new View.AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
                    data.setReceivedAccessibilityFocusEvent(true);
                }

                // Return false so that an accessibility event is not actually sent.
                return false;
            }
        };
    }
}