// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

/** Helper class with various testing util methods for content shell accessibility tests. */
public class AccessibilityContentShellTestUtils {
    // Common test output error messages
    public static final String ANP_ERROR =
            "Could not find AccessibilityNodeProvider object for WebContentsAccessibilityImpl";
    public static final String NODE_TIMEOUT_ERROR =
            "Could not find specified node before polling timeout.";
    public static final String END_OF_TEST_ERROR =
            "Did not receive kEndOfTest signal before polling timeout.";
    public static final String READY_FOR_TEST_ERROR =
            "Did not receive kReadyForTest signal before polling timeout.";

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
        boolean matches(AccessibilityNodeInfoCompat node, T element);
    }

    // Helper methods for common matching conditions for AccessibilityNodeInfo objects.

    static AccessibilityNodeInfoMatcher<Integer> sInputTypeMatcher =
            (node, type) -> node.getInputType() == type;

    static AccessibilityNodeInfoMatcher<String> sClassNameMatcher =
            (node, className) -> node.getClassName().equals(className);

    static AccessibilityNodeInfoMatcher<String> sTextMatcher =
            (node, text) -> {
                if (node.getText() == null) return false;

                return node.getText().toString().equals(text);
            };

    static AccessibilityNodeInfoMatcher<String> sTextOrContentDescriptionMatcher =
            (node, text) -> {
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

    static AccessibilityNodeInfoMatcher<String> sTextVisibleToUserMatcher =
            (node, text) -> {
                if (node.getText() == null) return false;

                return node.getText().toString().equals(text) && node.isVisibleToUser();
            };

    static AccessibilityNodeInfoMatcher<String> sRangeInfoMatcher =
            (node, element) -> node.getRangeInfo() != null;

    static AccessibilityNodeInfoMatcher<String> sViewIdResourceNameMatcher =
            (node, text) -> {
                if (node.getViewIdResourceName() == null) return false;

                return node.getViewIdResourceName().equals(text);
            };

    /**
     * Main AccessibilityDelegate for accessibility content shell tests.
     *
     * The delegate will set values in the |AccessibilityContentShellTestData| singleton based
     * on the event type. The method will always return |false| so that the AccessibilityEvent
     * is not actually sent to AT, which would make the test fail.
     */
    public static View.AccessibilityDelegate sContentShellDelegate =
            new View.AccessibilityDelegate() {
                @Override
                public boolean onRequestSendAccessibilityEvent(
                        ViewGroup host, View child, AccessibilityEvent event) {
                    AccessibilityContentShellTestData data =
                            AccessibilityContentShellTestData.getInstance();

                    // Switch on eventType and save relevant data as needed.
                    switch (event.getEventType()) {
                            // Save the text of proactive announcements.
                        case AccessibilityEvent.TYPE_ANNOUNCEMENT:
                            {
                                data.setAnnouncementText(event.getText().get(0).toString());
                                break;
                            }

                            // Save the traverse and selection indices during text traversal.
                        case AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED:
                            {
                                data.setSelectionFromIndex(event.getFromIndex());
                                data.setSelectionToIndex(event.getToIndex());
                                data.setReceivedSelectionEvent(true);
                                break;
                            }
                        case AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
                            {
                                data.setTraverseFromIndex(event.getFromIndex());
                                data.setTraverseToIndex(event.getToIndex());
                                data.setReceivedTraversalEvent(true);
                                break;
                            }

                            // Save that a particular type of event has been sent.
                        case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
                            {
                                data.setReceivedAccessibilityFocusEvent(true);
                                break;
                            }
                        case AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED:
                            {
                                data.incrementWindowContentChangedCount();
                                break;
                            }
                        case AccessibilityEvent.TYPE_VIEW_SCROLLED:
                            {
                                data.setReceivedEvent(true);
                                break;
                            }

                            // Currently unused/ignored for content shell test purposes.
                        case AccessibilityEvent.TYPE_ASSIST_READING_CONTEXT:
                        case AccessibilityEvent.TYPE_GESTURE_DETECTION_END:
                        case AccessibilityEvent.TYPE_GESTURE_DETECTION_START:
                        case AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED:
                        case AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END:
                        case AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_START:
                        case AccessibilityEvent.TYPE_TOUCH_INTERACTION_END:
                        case AccessibilityEvent.TYPE_TOUCH_INTERACTION_START:
                        case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED:
                        case AccessibilityEvent.TYPE_VIEW_CLICKED:
                        case AccessibilityEvent.TYPE_VIEW_CONTEXT_CLICKED:
                        case AccessibilityEvent.TYPE_VIEW_FOCUSED:
                        case AccessibilityEvent.TYPE_VIEW_HOVER_ENTER:
                        case AccessibilityEvent.TYPE_VIEW_HOVER_EXIT:
                        case AccessibilityEvent.TYPE_VIEW_LONG_CLICKED:
                        case AccessibilityEvent.TYPE_VIEW_SELECTED:
                        case AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED:
                        case AccessibilityEvent.TYPE_WINDOWS_CHANGED:
                        case AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED:
                            break;
                    }

                    // Return false so that an accessibility event is not actually sent.
                    return false;
                }
            };
}
