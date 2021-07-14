// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Bundle;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import java.util.LinkedList;

/**
 * Helper class for tracking accessibility actions and events for end-to-end tests.
 */
public class AccessibilityActionAndEventTracker {
    private LinkedList<String> mEvents;
    private boolean mTestComplete;

    public AccessibilityActionAndEventTracker() {
        this.mEvents = new LinkedList<String>();
        this.mTestComplete = false;
    }

    public void addEvent(AccessibilityEvent event) {
        mEvents.add(eventToString(event));
    }

    public void addAction(int action, Bundle arguments) {
        mEvents.add(actionToString(action, arguments));
    }

    public String results() {
        StringBuilder results = new StringBuilder();

        for (String event : mEvents) {
            if (event != null && !event.isEmpty()) {
                results.append(event);
                results.append('\n');
            }
        }

        return results.toString().trim();
    }

    /**
     * Helper method to signal the beginning of a given unit test.
     */
    public void signalReadyForTest() {
        mTestComplete = false;
    }

    /**
     * Helper method to signal the end of a given unit test.
     */
    public void signalEndOfTest() {
        mTestComplete = true;
    }

    /**
     * Helper method for polling, used to tell main thread when a test is complete.
     * @return  boolean     Whether the tracker has received a signal that the test is complete.
     */
    public boolean testComplete() {
        return mTestComplete;
    }

    /**
     * Helper method to take an accessibility action and convert it to a string of useful
     * information for testing.
     *
     * @param action            int action
     * @param arguments         Bundle arguments
     * @return                  String representation of the given action
     */
    private String actionToString(int action, Bundle arguments) {
        StringBuilder builder = new StringBuilder();
        builder.append(actionIntToString(action));

        // If we have non-null arguments, add them to our String for this action.
        if (arguments != null) {
            StringBuilder argsBuilder = new StringBuilder();
            argsBuilder.append("[");

            for (String key : arguments.keySet()) {
                argsBuilder.append(" {");
                argsBuilder.append(key);
                argsBuilder.append(arguments.get(key).toString());
                argsBuilder.append("},");
            }
            argsBuilder.append(" ]");

            builder.append(", ");
            builder.append(argsBuilder.toString());
        }

        return builder.toString();
    }

    /**
     * Helper method to take an event and convert it to a string of useful information for testing.
     * For any events with significant info, we append this to the end of the string in square
     * braces. For example, for the TYPE_ANNOUNCEMENT events we append the announcement text.
     *
     * @param event             AccessibilityEvent event to get a string for
     * @return                  String representation of the given event
     */
    private static String eventToString(AccessibilityEvent event) {
        // Convert event type to a human readable String (except TYPE_WINDOW_CONTENT_CHANGED)
        if (event.getEventType() == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) return null;

        StringBuilder builder = new StringBuilder();
        builder.append(AccessibilityEvent.eventTypeToString(event.getEventType()));

        // Add extra information based on eventType.
        switch (event.getEventType()) {
            // For announcements, track the text announced to the user.
            case AccessibilityEvent.TYPE_ANNOUNCEMENT: {
                builder.append(" - [");
                builder.append(event.getText().get(0).toString());
                builder.append("]");
                break;
            }
            // For text selection/traversal, track the To and From indices.
            case AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
            case AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED: {
                builder.append(" - [");
                builder.append(event.getFromIndex());
                builder.append(", ");
                builder.append(event.getToIndex());
                builder.append("]");
                break;
            }

            // For appearance of dialogs, track the content types.
            case AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED: {
                builder.append(" - [contentTypes=");
                builder.append(event.getContentChangeTypes());
                builder.append("]");
                break;
            }

            // Events that do not add extra information for unit tests
            case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
            case AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED:
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
            case AccessibilityEvent.TYPE_VIEW_SCROLLED:
            case AccessibilityEvent.TYPE_VIEW_SELECTED:
            case AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED:
            case AccessibilityEvent.TYPE_WINDOWS_CHANGED:
            default:
                break;
        }

        // Return generated String.
        return builder.toString();
    }

    /**
     * Helper method to convert given int action into a readable String.
     * @param action        int action
     * @return              String action text
     */
    public static String actionIntToString(int action) {
        switch (action) {
            case AccessibilityNodeInfo.ACTION_FOCUS:
                return "ACTION_FOCUS";
            case AccessibilityNodeInfo.ACTION_CLEAR_FOCUS:
                return "ACTION_CLEAR_FOCUS";
            case AccessibilityNodeInfo.ACTION_SELECT:
                return "ACTION_SELECT";
            case AccessibilityNodeInfo.ACTION_CLEAR_SELECTION:
                return "ACTION_CLEAR_SELECTION";
            case AccessibilityNodeInfo.ACTION_CLICK:
                return "ACTION_CLICK";
            case AccessibilityNodeInfo.ACTION_LONG_CLICK:
                return "ACTION_LONG_CLICK";
            case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                return "ACTION_ACCESSIBILITY_FOCUS";
            case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                return "ACTION_CLEAR_ACCESSIBILITY_FOCUS";
            case AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY:
                return "ACTION_NEXT_AT_MOVEMENT_GRANULARITY";
            case AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY:
                return "ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY";
            case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT:
                return "ACTION_NEXT_HTML_ELEMENT";
            case AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT:
                return "ACTION_PREVIOUS_HTML_ELEMENT";
            case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
                return "ACTION_SCROLL_FORWARD";
            case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
                return "ACTION_SCROLL_BACKWARD";
            case AccessibilityNodeInfo.ACTION_CUT:
                return "ACTION_CUT";
            case AccessibilityNodeInfo.ACTION_COPY:
                return "ACTION_COPY";
            case AccessibilityNodeInfo.ACTION_PASTE:
                return "ACTION_PASTE";
            case AccessibilityNodeInfo.ACTION_SET_SELECTION:
                return "ACTION_SET_SELECTION";
            case AccessibilityNodeInfo.ACTION_EXPAND:
                return "ACTION_EXPAND";
            case AccessibilityNodeInfo.ACTION_COLLAPSE:
                return "ACTION_COLLAPSE";
            case AccessibilityNodeInfo.ACTION_DISMISS:
                return "ACTION_DISMISS";
            case AccessibilityNodeInfo.ACTION_SET_TEXT:
                return "ACTION_SET_TEXT";

            // These come from WebContentsAccessibilityImpl to support earlier Android versions.
            case WebContentsAccessibilityImpl.ACTION_SHOW_ON_SCREEN:
                return "ACTION_SHOW_ON_SCREEN";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_TO_POSITION:
                return "ACTION_SCROLL_TO_POSITION";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_UP:
                return "ACTION_SCROLL_UP";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_LEFT:
                return "ACTION_SCROLL_LEFT";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_DOWN:
                return "ACTION_SCROLL_DOWN";
            case WebContentsAccessibilityImpl.ACTION_SCROLL_RIGHT:
                return "ACTION_SCROLL_RIGHT";
            case WebContentsAccessibilityImpl.ACTION_PAGE_DOWN:
                return "ACTION_PAGE_DOWN";
            case WebContentsAccessibilityImpl.ACTION_PAGE_UP:
                return "ACTION_PAGE_UP";
            case WebContentsAccessibilityImpl.ACTION_PAGE_LEFT:
                return "ACTION_PAGE_LEFT";
            case WebContentsAccessibilityImpl.ACTION_PAGE_RIGHT:
                return "ACTION_PAGE_RIGHT";
            case WebContentsAccessibilityImpl.ACTION_SET_PROGRESS:
                return "ACTION_SET_PROGRESS";
            case WebContentsAccessibilityImpl.ACTION_CONTEXT_CLICK:
                return "ACTION_CONTEXT_CLICK";
            case WebContentsAccessibilityImpl.ACTION_SHOW_TOOLTIP:
                return "ACTION_SHOW_TOOLTIP";
            case WebContentsAccessibilityImpl.ACTION_HIDE_TOOLTIP:
                return "ACTION_HIDE_TOOLTIP";
            case WebContentsAccessibilityImpl.ACTION_PRESS_AND_HOLD:
                return "ACTION_PRESS_AND_HOLD";
            case WebContentsAccessibilityImpl.ACTION_IME_ENTER:
                return "ACTION_IME_ENTER";
            case WebContentsAccessibilityImpl.ACTION_MOVE_WINDOW:
                return "ACTION_MOVE_WINDOW";
            default:
                return "ACTION_UNKNOWN";
        }
    }
}
