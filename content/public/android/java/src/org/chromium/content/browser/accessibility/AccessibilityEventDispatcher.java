// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.SystemClock;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * This class works as an intermediary between {@link WebContentsAccessibilityImpl} and the OS to
 * throttle and queue AccessibilityEvents that are sent in quick succession. This ensures we do
 * not overload the system and create lag by sending superfluous events.
 */
public class AccessibilityEventDispatcher {
    // Maps an AccessibilityEvent type to a throttle delay in milliseconds. This is populated once
    // in the constructor.
    private Map<Integer, Integer> mEventThrottleDelays;

    // Set of AccessibilityEvent types to throttle wholesale, rather than on a per |virtualViewId|
    // basis. Delays are still set independently in the |mEventThrottleDelays| map. This is
    // populated once in the constructor.
    private Set<Integer> mViewIndependentEventsToThrottle;

    // Set of AccessibilityEvent types that are relevant to enabled accessibility services and
    // will be enqueued to be dispatched.
    private Set<Integer> mRelevantEventTypes;

    // For events being throttled (see: |mEventsToThrottle|), this array will map the eventType
    // to the last time (long in milliseconds) such an event has been sent.
    private Map<Long, Long> mEventLastFiredTimes = new HashMap<Long, Long>();

    // For events being throttled (see: |mEventsToThrottle|), this array will map the eventType
    // to a single Runnable that will send an event after some delay.
    private Map<Long, Runnable> mPendingEvents = new HashMap<Long, Runnable>();

    // Implementation of the callback interface to {@link WebContentsAccessibilityImpl} so that we
    // can maintain a connection through JNI to the native code.
    private Client mClient;

    /**
     * Callback interface to link {@link WebContentsAccessibilityImpl} with an instance of the
     * {@link AccessibilityEventDispatcher} so we can separate out queueing/throttling logic into a
     * dispatcher while still maintaining a connection through the JNI to native code.
     */
    interface Client {
        /**
         * Post a Runnable to a view's message queue.
         *
         * @param toPost                The Runnable to post.
         * @param delayInMilliseconds   The delay in milliseconds before running.
         */
        void postRunnable(Runnable toPost, long delayInMilliseconds);

        /**
         * Remove a Runnable from a view's message queue.
         *
         * @param toRemove              The Runnable to remove.
         */
        void removeRunnable(Runnable toRemove);

        /**
         * Build an AccessibilityEvent for the given id and type. Requires a connection through the
         * JNI to the native code to populate the fields of the event. If successfully built, then
         * send the event and return true, otherwise return false.
         *
         * @param virtualViewId         This virtualViewId for the view trying to send an event.
         * @param eventType             The AccessibilityEvent type.
         * @return                      boolean value of whether event was sent.
         */
        boolean dispatchEvent(int virtualViewId, int eventType);
    }

    /** Create an AccessibilityEventDispatcher and define the delays for event types. */
    public AccessibilityEventDispatcher(
            Client mClient,
            Map<Integer, Integer> eventThrottleDelays,
            Set<Integer> viewIndependentEventsToThrottle,
            Set<Integer> relevantEventTypes) {
        this.mClient = mClient;
        this.mEventThrottleDelays = eventThrottleDelays;
        this.mViewIndependentEventsToThrottle = viewIndependentEventsToThrottle;
        this.mRelevantEventTypes = relevantEventTypes;
    }

    /**
     * Enqueue an AccessibilityEvent. This method will leverage our throttling and queue, and
     * check the appropriate amount of time we should delay, if at all, before building/sending this
     * event. When ready, this will handle the delayed construction and dispatching of this event.
     *
     * @param virtualViewId     This virtualViewId for the view trying to send an event.
     * @param eventType         The AccessibilityEvent type.
     */
    public void enqueueEvent(int virtualViewId, int eventType) {
        // Check if this is a relevant event type.
        if (!mRelevantEventTypes.contains(eventType)) {
            return;
        }

        // Check whether this type of event is one we want to throttle, and if not then send it
        if (!mEventThrottleDelays.containsKey(eventType)) {
            mClient.dispatchEvent(virtualViewId, eventType);
            return;
        }

        // Check when we last fired an event of |eventType| for this |virtualViewId|. If we have not
        // fired an event of this type for this id, or the last time was longer ago than the delay
        // for this eventType as per |mEventThrottleDelays|, then we allow this event to be sent
        // immediately and record the time and clear any lingering callbacks.
        long now = SystemClock.elapsedRealtime();
        long uuid = uuid(virtualViewId, eventType);
        if (mEventLastFiredTimes.get(uuid) == null
                || now - mEventLastFiredTimes.get(uuid) >= mEventThrottleDelays.get(eventType)) {
            // Attempt to dispatch an event, can fail and return false if node is invalid etc.
            if (mClient.dispatchEvent(virtualViewId, eventType)) {
                // Record time of last fired event if the dispatch was successful.
                mEventLastFiredTimes.put(uuid, now);
            }

            // Remove any lingering callbacks and pending events regardless of success.
            mClient.removeRunnable(mPendingEvents.get(uuid));
            mPendingEvents.remove(uuid);
        } else {
            // We have fired an event of |eventType| for this |virtualViewId| within our
            // |mEventThrottleDelays| delay window. Store this event, replacing any events in
            // |mPendingEvents| of the same |uuid|, and set a delay equal.
            mClient.removeRunnable(mPendingEvents.get(uuid));

            Runnable myRunnable =
                    () -> {
                        // We have delayed firing this event, so accessibility may not be enabled or
                        // the node may be invalid, in which case dispatch will return false.
                        if (mClient.dispatchEvent(virtualViewId, eventType)) {
                            // After sending event, record time it was sent
                            mEventLastFiredTimes.put(uuid, SystemClock.elapsedRealtime());
                        }

                        // Remove any lingering callbacks and pending events regardless of success.
                        mClient.removeRunnable(mPendingEvents.get(uuid));
                        mPendingEvents.remove(uuid);
                    };

            mClient.postRunnable(
                    myRunnable,
                    (mEventLastFiredTimes.get(uuid) + mEventThrottleDelays.get(eventType)) - now);
            mPendingEvents.put(uuid, myRunnable);
        }
    }

    /**
     * Helper method to cancel all posted Runnables if the Client object is being destroyed early.
     */
    public void clearQueue() {
        for (Long uuid : mPendingEvents.keySet()) {
            mClient.removeRunnable(mPendingEvents.get(uuid));
        }
        mPendingEvents.clear();
    }

    /**
     * Helper method to update the list of relevant event types to be dispatched.
     * @param relevantEventTypes        Set<Integer> relevant event types
     */
    public void updateRelevantEventTypes(Set<Integer> relevantEventTypes) {
        this.mRelevantEventTypes = relevantEventTypes;
    }

    /**
     * Calculates a unique identifier for a given |virtualViewId| and |eventType| pairing. This is
     * used to replace the need for a Pair<> or wrapper object to hold two simple ints. We shift the
     * |virtualViewId| 32 bits left, and bitwise OR with |eventType|, creating a 64-bit unique long.
     *
     * @param virtualViewId     This virtualViewId for the view trying to send an event.
     * @param eventType         The AccessibilityEvent type.
     * @return uuid             The uuid (universal unique id) for this pairing.
     */
    private long uuid(int virtualViewId, int eventType) {
        // For some event types, we will disregard the |virtualViewId| and throttle the events
        // entirely by |eventType|.
        if (mViewIndependentEventsToThrottle.contains(eventType)) {
            return eventType;
        }

        return ((long) virtualViewId << 32) | ((long) eventType);
    }
}
