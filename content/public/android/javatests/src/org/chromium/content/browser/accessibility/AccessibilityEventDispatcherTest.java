// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Test suite to ensure that |AccessibilityEventDispatcher| behaves appropriately. */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccessibilityEventDispatcherTest {
    private AccessibilityEventDispatcher mDispatcher;
    private Map<Integer, Integer> mEventDelays = new HashMap<Integer, Integer>();
    private Set<Integer> mViewIndependentEvents = new HashSet<Integer>();

    // Helper member variables for testing.
    private boolean mRunnablePosted;
    private boolean mRunnableRemoved;
    private boolean mEventDispatched;

    /**
     * Test setup, run before each test. Creates a HashMap of eventType's to delay (fake values),
     * and creates an |AccessibilityEventDispatcher| with a Client that tracks method calls.
     */
    @Before
    public void setUp() {
        // Mock two eventTypes to throttle
        mEventDelays.put(2, 10);
        mEventDelays.put(3, 5000);

        // Create a dispatcher, and track which callback methods have been called with booleans
        mDispatcher =
                new AccessibilityEventDispatcher(
                        new AccessibilityEventDispatcher.Client() {
                            @Override
                            public void postRunnable(Runnable toPost, long delayInMilliseconds) {
                                mRunnablePosted = true;
                            }

                            @Override
                            public void removeRunnable(Runnable toRemove) {
                                mRunnableRemoved = true;
                            }

                            @Override
                            public boolean dispatchEvent(int virtualViewId, int eventType) {
                                mEventDispatched = true;
                                return true;
                            }
                        },
                        mEventDelays,
                        mViewIndependentEvents,
                        new HashSet<Integer>(Arrays.asList(1, 2, 3)));

        mRunnablePosted = false;
        mRunnableRemoved = false;
        mEventDispatched = false;
    }

    /** Test enqueue properly ignores events not being throttled and acts like a pass-through. */
    @Test
    @SmallTest
    public void testEnqueue_notThrottle() {
        mDispatcher.enqueueEvent(1, 1);

        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertFalse(mRunnableRemoved);
    }

    /**
     * Test enqueue sends first event of a given type when no previous events of that type have
     * been sent, i.e. we do not throttle the first event.
     */
    @Test
    @SmallTest
    public void testEnqueue_noPreviousEvents() {
        mDispatcher.enqueueEvent(1, 2);

        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);
    }

    /**
     * Test enqueue sends events of a given type when the previous event of that type has been
     * sent longer ago than our delay time.
     *
     * @throws InterruptedException        Thread.sleep()
     */
    @Test
    @SmallTest
    public void testEnqueue_noRecentPreviousEvents() throws InterruptedException {
        // Send first event through as normal
        mDispatcher.enqueueEvent(1, 2);
        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);

        // Reset trackers
        mRunnablePosted = false;
        mRunnableRemoved = false;
        mEventDispatched = false;

        // Wait 5 seconds so next event won't be throttled
        Thread.sleep(5000);

        // Send another event and it should pass through as normal (we waited longer than delay)
        mDispatcher.enqueueEvent(1, 2);
        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);
    }

    /**
     * Test enqueue posts a |Runnable| for events of a given type that come in quick succession,
     * i.e. we are throttling events and queueing them for later sending.
     */
    @Test
    @SmallTest
    public void testEnqueue_recentEventsInQueue() {
        // Send first event through as normal
        mDispatcher.enqueueEvent(1, 3);
        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);

        // Send a series of more events rapidly
        mDispatcher.enqueueEvent(1, 3);
        mDispatcher.enqueueEvent(1, 3);
        mDispatcher.enqueueEvent(1, 3);
        Assert.assertTrue(mRunnablePosted);

        // Reset trackers
        mRunnablePosted = false;
        mRunnableRemoved = false;
        mEventDispatched = false;

        // Send final event, ensure runnable is posted but nothing was dispatched
        mDispatcher.enqueueEvent(1, 3);
        Assert.assertFalse(mEventDispatched);
        Assert.assertTrue(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);
    }

    /** Test enqueue will drop events that are not part of the relevant events type set. */
    @Test
    @SmallTest
    public void testEnqueue_relevantEventsCheck() {
        // Create a set of relevant events and pass it to the dispatcher.
        Set<Integer> relevantEvents = new HashSet<Integer>();
        relevantEvents.add(3);
        mDispatcher.updateRelevantEventTypes(relevantEvents);

        // Send a relevant event type and ensure it is dispatched.
        mDispatcher.enqueueEvent(1, 3);
        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);

        // Reset trackers.
        mRunnablePosted = false;
        mRunnableRemoved = false;
        mEventDispatched = false;

        // Send a not relevant event type and ensure it is dropped.
        mDispatcher.enqueueEvent(1, 2);
        Assert.assertFalse(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertFalse(mRunnableRemoved);
    }

    /** Test the creation of uuid for view independent throttling. */
    @Test
    @SmallTest
    public void testUuid_creation() {
        // Add a view independent event type.
        mViewIndependentEvents.add(3);

        // Send first event through as normal.
        mDispatcher.enqueueEvent(1, 3);
        Assert.assertTrue(mEventDispatched);
        Assert.assertFalse(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);

        // Send a series of events from various views of the same type.
        mDispatcher.enqueueEvent(2, 3);
        mDispatcher.enqueueEvent(3, 3);
        mDispatcher.enqueueEvent(4, 3);
        Assert.assertTrue(mRunnablePosted);

        // Reset trackers.
        mRunnablePosted = false;
        mRunnableRemoved = false;
        mEventDispatched = false;

        // Send final event, ensure runnable is posted but nothing was dispatched.
        mDispatcher.enqueueEvent(5, 3);
        Assert.assertFalse(mEventDispatched);
        Assert.assertTrue(mRunnablePosted);
        Assert.assertTrue(mRunnableRemoved);
    }
}
