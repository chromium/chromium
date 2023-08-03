// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.Calendar;

/**
 * Helper class for recording UMA histograms of accessibility events
 */
public class AccessibilityHistogramRecorder {
    // OnDemand AX Mode histogram values
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PERCENTAGE_DROPPED_HISTOGRAM =
            "Accessibility.Android.OnDemand.PercentageDropped";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE =
            "Accessibility.Android.OnDemand.PercentageDropped.Complete";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS =
            "Accessibility.Android.OnDemand.PercentageDropped.FormControls";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC =
            "Accessibility.Android.OnDemand.PercentageDropped.Basic";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String EVENTS_DROPPED_HISTOGRAM =
            "Accessibility.Android.OnDemand.EventsDropped";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String ONE_HUNDRED_PERCENT_HISTOGRAM =
            "Accessibility.Android.OnDemand.OneHundredPercentEventsDropped";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE =
            "Accessibility.Android.OnDemand.OneHundredPercentEventsDropped.Complete";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS =
            "Accessibility.Android.OnDemand.OneHundredPercentEventsDropped.FormControls";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC =
            "Accessibility.Android.OnDemand.OneHundredPercentEventsDropped.Basic";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String USAGE_FOREGROUND_TIME = "Accessibility.Android.Usage.Foreground";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String USAGE_NATIVE_INITIALIZED_TIME =
            "Accessibility.Android.Usage.NativeInit";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String USAGE_ACCESSIBILITY_ALWAYS_ON_TIME =
            "Accessibility.Android.Usage.A11yAlwaysOn";

    private static final int EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET = 1;
    private static final int EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET = 10000;
    private static final int EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT = 100;

    // Node cache histogram values
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String CACHE_MAX_NODES_HISTOGRAM =
            "Accessibility.Android.Cache.MaxNodesInCache";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String CACHE_PERCENTAGE_RETRIEVED_FROM_CACHE_HISTOGRAM =
            "Accessibility.Android.Cache.PercentageRetrievedFromCache";

    private static final int CACHE_MAX_NODES_MIN_BUCKET = 1;
    private static final int CACHE_MAX_NODES_MAX_BUCKET = 3000;
    private static final int CACHE_MAX_NODES_BUCKET_COUNT = 100;

    // These track the total number of enqueued events, and the total number of dispatched events,
    // so we can report the percentage/number of dropped events.
    private int mTotalEnqueuedEvents;
    private int mTotalDispatchedEvents;

    // These track the usage of the |mNodeInfoCache| to report metrics on the max number of items
    // that were stored in the cache, and the percentage of requests retrieved from the cache.
    private int mMaxNodesInCache;
    private int mNodeWasReturnedFromCache;
    private int mNodeWasCreatedFromScratch;

    // These track the usage in time when a web contents is in the foreground.
    private long mTimeOfFirstShown = -1;
    private long mTimeOfNativeInitialization = -1;

    /**
     * Increment the count of enqueued events
     */
    public void incrementEnqueuedEvents() {
        mTotalEnqueuedEvents++;
    }

    /**
     * Increment the count of dispatched events
     */
    public void incrementDispatchedEvents() {
        mTotalDispatchedEvents++;
    }

    /**
     * Update the value of max nodes in the cache given the current size of the node info cache
     * @param nodeInfoCacheSize the size of the node info cache
     */
    public void updateMaxNodesInCache(int nodeInfoCacheSize) {
        mMaxNodesInCache = Math.max(mMaxNodesInCache, nodeInfoCacheSize);
    }

    /**
     * Increment the count of instances when a node was returned from the cache
     */
    public void incrementNodeWasReturnedFromCache() {
        mNodeWasReturnedFromCache++;
    }

    /**
     * Increment the count of instances when a node was created from scratch
     */
    public void incrementNodeWasCreatedFromScratch() {
        mNodeWasCreatedFromScratch++;
    }

    /**
     * Set the time this instance was shown to the current time in ms.
     */
    public void updateTimeOfFirstShown() {
        mTimeOfFirstShown = Calendar.getInstance().getTimeInMillis();
    }

    /**
     * Set the time this instance had native initialization called to the current time in ms.
     */
    public void updateTimeOfNativeInitialization() {
        mTimeOfNativeInitialization = Calendar.getInstance().getTimeInMillis();
    }

    /**
     * Record UMA histograms for performance-related accessibility metrics.
     */
    public void recordAccessibilityPerformanceHistograms() {
        // If the OnDemand feature is enabled, log UMA metrics and reset counters.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ON_DEMAND_ACCESSIBILITY_EVENTS)) {
            recordEventsHistograms();
        }

        // Always track the histograms for cache usage statistics.
        recordCacheHistograms();
    }

    /**
     * Record UMA histograms for the event counts for the OnDemand feature.
     */
    public void recordEventsHistograms() {
        // To investigate whether adding more AXModes could be beneficial, track separate
        // stats when both the AccessibilityPerformanceFiltering and OnDemand features are enabled.
        boolean isAccessibilityPerformanceFilteringEnabled =
                ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PERFORMANCE_FILTERING);

        // There are only 2 AXModes, kAXModeComplete is used when a screenreader is active.
        boolean isAXModeComplete = AccessibilityState.isScreenReaderEnabled();
        boolean isAXModeFormControls = AccessibilityState.isOnlyPasswordManagersEnabled();

        // If we did not enqueue any events, we can ignore the data as a trivial case.
        if (mTotalEnqueuedEvents > 0) {
            // Log the percentage dropped (dispatching 0 events should be 100% dropped).
            int percentSent = (int) (mTotalDispatchedEvents * 1.0 / mTotalEnqueuedEvents * 100.0);
            RecordHistogram.recordPercentageHistogram(
                    PERCENTAGE_DROPPED_HISTOGRAM, 100 - percentSent);
            // Log the percentage dropped per AXMode as well.
            if (isAccessibilityPerformanceFilteringEnabled) {
                RecordHistogram.recordPercentageHistogram(isAXModeComplete
                                ? PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE
                                : isAXModeFormControls
                                ? PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS
                                : PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC,
                        100 - percentSent);
            }

            // Log the total number of dropped events. (Not relevant to be tracked per AXMode)
            RecordHistogram.recordCustomCountHistogram(EVENTS_DROPPED_HISTOGRAM,
                    mTotalEnqueuedEvents - mTotalDispatchedEvents,
                    EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET, EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET,
                    EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT);

            // If 100% of events were dropped, also track the number of dropped events in a
            // separate bucket.
            if (percentSent == 0) {
                RecordHistogram.recordCustomCountHistogram(ONE_HUNDRED_PERCENT_HISTOGRAM,
                        mTotalEnqueuedEvents - mTotalDispatchedEvents,
                        EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET, EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET,
                        EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT);

                // Log the 100% events count per AXMode as well.
                if (isAccessibilityPerformanceFilteringEnabled) {
                    RecordHistogram.recordCustomCountHistogram(isAXModeComplete
                                    ? ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE
                                    : isAXModeFormControls
                                    ? ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS
                                    : ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC,
                            mTotalEnqueuedEvents - mTotalDispatchedEvents,
                            EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET,
                            EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET,
                            EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT);
                }
            }
        }

        // Reset counters.
        mTotalEnqueuedEvents = 0;
        mTotalDispatchedEvents = 0;
    }

    /**
     *  Record UMA histograms for the AccessibilityNodeInfo cache usage statistics.
     */
    public void recordCacheHistograms() {
        RecordHistogram.recordCustomCountHistogram(CACHE_MAX_NODES_HISTOGRAM, mMaxNodesInCache,
                CACHE_MAX_NODES_MIN_BUCKET, CACHE_MAX_NODES_MAX_BUCKET,
                CACHE_MAX_NODES_BUCKET_COUNT);

        int totalNodeRequests = mNodeWasReturnedFromCache + mNodeWasCreatedFromScratch;
        int percentFromCache = (int) (mNodeWasReturnedFromCache * 1.0 / totalNodeRequests * 100.0);

        RecordHistogram.recordPercentageHistogram(
                CACHE_PERCENTAGE_RETRIEVED_FROM_CACHE_HISTOGRAM, percentFromCache);

        // Reset counters.
        mMaxNodesInCache = 0;
        mNodeWasReturnedFromCache = 0;
        mNodeWasCreatedFromScratch = 0;
    }

    /**
     * Record UMA histograms for the usage timers of the native accessibility engine.
     */
    public void recordAccessibilityUsageHistograms() {
        // If the Tab was not shown, the following histograms have no value.
        if (mTimeOfFirstShown < 0) return;

        long now = Calendar.getInstance().getTimeInMillis();

        // Record the general usage in the foreground, long histograms are up to 1 hour.
        RecordHistogram.recordLongTimesHistogram(USAGE_FOREGROUND_TIME, now - mTimeOfFirstShown);
        mTimeOfFirstShown = -1;

        // If native was not initialized, the following histograms have no value.
        if (mTimeOfNativeInitialization < 0) return;

        // Record native initialized time, long histograms are up to 1 hour.
        RecordHistogram.recordLongTimesHistogram(
                USAGE_NATIVE_INITIALIZED_TIME, now - mTimeOfNativeInitialization);

        // When the foreground and native usage times are close in value, then we will assume this
        // was an instance with an accessibility service always running, and record that usage.
        long timeDiff = Math.abs(mTimeOfNativeInitialization - mTimeOfFirstShown);
        if (timeDiff < 500 /* ms */ || ((double) timeDiff / mTimeOfFirstShown) < 0.03 /* % */) {
            RecordHistogram.recordLongTimesHistogram(
                    USAGE_ACCESSIBILITY_ALWAYS_ON_TIME, now - mTimeOfNativeInitialization);
        }
        mTimeOfNativeInitialization = -1;
    }
}