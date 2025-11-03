// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Bundle;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

/** Helper class for tracking accessibility actions and events for end-to-end tests. */
@NullMarked
public class AccessibilityActionAndEventTracker {
    private final ArrayList<String> mEvents;
    private final boolean mShouldFilterTrivialEvents;
    private boolean mTestComplete;
    private @Nullable CountDownLatch mEventLatch;

    public AccessibilityActionAndEventTracker() {
        mEvents = new ArrayList<String>();
        mTestComplete = false;
        mShouldFilterTrivialEvents = true;
    }

    public AccessibilityActionAndEventTracker(boolean shouldFilterTrivialEvents) {
        // TODO(crbug.com/414363686) this overloaded constructor should be removed after flakiness
        // of event test with trivial events included is solved. mShouldFilterTrivialEvents should
        // also be removed after fixing the flakiness.
        mEvents = new ArrayList<String>();
        mTestComplete = false;
        mShouldFilterTrivialEvents = shouldFilterTrivialEvents;
    }

    // This interface allows us to store additional information about WINDOW_CONTENT_CHANGED events
    // that do not have an assigned content type (i.e. LIVE_REGION_NODE_CHANGED events).
    @IntDef({
        WindowContentChangedSubtype.NONE,
        WindowContentChangedSubtype.LIVE_REGION_NODE_CHANGED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WindowContentChangedSubtype {
        int NONE = 0;
        int LIVE_REGION_NODE_CHANGED = 1;
    }

    // WINDOW_CONTENT_CHANGED events are eligible to be tracked if they have a specific subtype.
    public boolean shouldWindowContentChangedSubtypeBeTracked(
            @WindowContentChangedSubtype int subtype) {
        return subtype == WindowContentChangedSubtype.LIVE_REGION_NODE_CHANGED;
    }

    public void setEventLatch(@Nullable CountDownLatch latch) {
        mEventLatch = latch;
    }

    public void addEvent(AccessibilityEvent event, @WindowContentChangedSubtype int subtype) {
        // In rare cases there may be a lingering event, so only add if the test is not complete.
        if (!mTestComplete) {
            if (mShouldFilterTrivialEvents) {
                // Convert event type to a human readable String (except TYPE_WINDOW_CONTENT_CHANGED
                // with an ineligible subtype, no CONTENT_CHANGE_TYPE_STATE_DESCRIPTION flag, and no
                // CONTENT_CHANGE_TYPE_PANE_TITLE flag)
                if (event.getEventType() == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) {
                    boolean hasNoStateDescriptionChange =
                            (event.getContentChangeTypes()
                                            & AccessibilityEvent
                                                    .CONTENT_CHANGE_TYPE_STATE_DESCRIPTION)
                                    == 0;
                    boolean hasNoPaneTitleChange =
                            (event.getContentChangeTypes()
                                            & AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_TITLE)
                                    == 0;
                    if (hasNoStateDescriptionChange
                            && hasNoPaneTitleChange
                            && !shouldWindowContentChangedSubtypeBeTracked(subtype)) {
                        return;
                    }
                }
            }
            mEvents.add(eventToString(event));
        }
    }

    public void addAction(int action, @Nullable Bundle arguments) {
        // In rare cases there may be a lingering action, so only add if the test is not complete.
        if (!mTestComplete) {
            mEvents.add(actionToString(action, arguments));
        }
    }

    public String results() {
        StringBuilder results = new StringBuilder();

        for (String event : new ArrayList<String>(mEvents)) {
            if (event != null && !event.isEmpty()) {
                results.append(event);
                results.append('\n');
            }
        }

        return results.toString().trim();
    }

    /** Helper method to signal the beginning of a given unit test. */
    public void signalReadyForTest() {
        mTestComplete = false;
    }

    /** Helper method to signal the end of a given unit test. */
    public void signalEndOfTest() {
        mTestComplete = true;
        // If a latch is waiting, signal it.
        if (mEventLatch != null) {
            mEventLatch.countDown();
        }
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
    private String actionToString(int action, @Nullable Bundle arguments) {
        StringBuilder builder = new StringBuilder();
        builder.append(AccessibilityNodeInfoUtils.toString(action));

        // If we have non-null arguments, add them to our String for this action.
        if (arguments != null) {
            StringBuilder argsBuilder = new StringBuilder();
            argsBuilder.append("[");

            for (String key : arguments.keySet()) {
                argsBuilder.append(" {");
                argsBuilder.append(key);
                // In case of null values, use "null".
                argsBuilder.append(arguments.get(key));
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
     * @param event AccessibilityEvent event to get a string for
     * @return String representation of the given event
     */
    private static String eventToString(AccessibilityEvent event) {
        // Convert event type to a human readable String
        StringBuilder builder = new StringBuilder();
        builder.append(AccessibilityEvent.eventTypeToString(event.getEventType()));

        // Add extra information based on eventType.
        switch (event.getEventType()) {
                // For announcements, track the text announced to the user.
            case AccessibilityEvent.TYPE_ANNOUNCEMENT:
                {
                    builder.append(" - [");
                    builder.append(event.getText().get(0).toString());
                    builder.append("]");
                    break;
                }
                // For text selection/traversal, track the To and From indices.
            case AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
            case AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED:
                {
                    builder.append(" - [");
                    builder.append(event.getFromIndex());
                    builder.append(", ");
                    builder.append(event.getToIndex());
                    builder.append("]");
                    break;
                }

                // For appearance of dialogs, track the content types.
            case AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED:
                {
                    builder.append(" - [contentTypes=");
                    builder.append(event.getContentChangeTypes());
                    builder.append("]");
                    break;
                }

                // Any TYPE_WINDOW_CONTENT_CHANGED event here should have the
                // CONTENT_CHANGE_TYPE_STATE_DESCRIPTION flag
            case AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED:
                {
                    builder.append(" - [contentTypes=");
                    builder.append(event.getContentChangeTypes());
                    builder.append("]");
                    break;
                }

                // Events that do not add extra information for unit tests
            case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
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
}
