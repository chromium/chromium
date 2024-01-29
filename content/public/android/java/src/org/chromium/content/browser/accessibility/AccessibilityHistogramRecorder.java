// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.accessibility.AccessibilityState;

/** Helper class for recording UMA histograms of accessibility events */
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_INITIAL =
            "Accessibility.Android.AutoDisableV2.DisableCalled.Initial";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_SUCCESSIVE =
            "Accessibility.Android.AutoDisableV2.DisableCalled.Successive";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_INITIAL =
            "Accessibility.Android.AutoDisableV2.ReEnableCalled.Initial";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_SUCCESSIVE =
            "Accessibility.Android.AutoDisableV2.ReEnabledCalled.Successive";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_INITIAL =
            "Accessibility.Android.AutoDisableV2.DisabledTime.Initial";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_SUCCESSIVE =
            "Accessibility.Android.AutoDisableV2.DisabledTime.Successive";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_INITIAL =
            "Accessibility.Android.AutoDisableV2.EnabledTime.Initial";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_SUCCESSIVE =
            "Accessibility.Android.AutoDisableV2.EnabledTime.Successive";

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
    private long mTimeOfLastDisabledCall = -1;
    private long mOngoingSumOfTimeDisabled;

    /** Record that the Auto-disable Accessibility feature has disabled accessibility. */
    public void onDisableCalled(boolean initialCall) {
        TraceEvent.begin("AccessibilityHistogramRecorder.onDisabledCalled");
        // To disable accessibility, it needs to have been previously initialized.
        assert mTimeOfNativeInitialization > 0
                : "Accessibility onDisabled was called, but accessibility has not been"
                        + " initialized.";
        long now = SystemClock.elapsedRealtime();

        // As we disable accessibility, we want to record how long it had been enabled.
        if (initialCall) {
            RecordHistogram.recordLongTimesHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_INITIAL,
                    now - mTimeOfNativeInitialization);
            RecordHistogram.recordBooleanHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_INITIAL, true);
        } else {
            RecordHistogram.recordLongTimesHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_SUCCESSIVE,
                    now - mTimeOfNativeInitialization);
            RecordHistogram.recordBooleanHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_SUCCESSIVE, true);
        }

        // To track how long we kept accessibility disabled if it is eventually re-enabled, track
        // when this call occurred.
        mTimeOfLastDisabledCall = now;

        // Record native initialized time in the usual method so this timeframe is not missed.
        RecordHistogram.recordLongTimesHistogram(
                USAGE_NATIVE_INITIALIZED_TIME, now - mTimeOfNativeInitialization);

        // Reset values.
        mTimeOfNativeInitialization = -1;

        TraceEvent.end("AccessibilityHistogramRecorder.onDisabledCalled");
    }

    /** Record that the Auto-disable Accessibility feature has re-enabled accessibility. */
    public void onReEnableCalled(boolean initialCall) {
        TraceEvent.begin("AccessibilityHistogramRecorder.onReEnabledCalled");
        long now = SystemClock.elapsedRealtime();

        // As we re-enable accessibility, we want to record how long it had been disabled.
        if (initialCall) {
            RecordHistogram.recordLongTimesHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_INITIAL,
                    (now - mTimeOfLastDisabledCall) + mOngoingSumOfTimeDisabled);
            RecordHistogram.recordBooleanHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_INITIAL, true);
        } else {
            RecordHistogram.recordLongTimesHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_SUCCESSIVE,
                    (now - mTimeOfLastDisabledCall) + mOngoingSumOfTimeDisabled);
            RecordHistogram.recordBooleanHistogram(
                    AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_SUCCESSIVE, true);
        }

        // To track how long we kept accessibility re-enabled if it is eventually disabled again,
        // track when this call occurred.
        mTimeOfNativeInitialization = now;

        // Reset value.
        mTimeOfLastDisabledCall = -1;
        mOngoingSumOfTimeDisabled = 0;

        TraceEvent.end("AccessibilityHistogramRecorder.onReEnabledCalled");
    }

    /** Increment the count of enqueued events */
    public void incrementEnqueuedEvents() {
        mTotalEnqueuedEvents++;
    }

    /** Increment the count of dispatched events */
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

    /** Increment the count of instances when a node was returned from the cache */
    public void incrementNodeWasReturnedFromCache() {
        mNodeWasReturnedFromCache++;
    }

    /** Increment the count of instances when a node was created from scratch */
    public void incrementNodeWasCreatedFromScratch() {
        mNodeWasCreatedFromScratch++;
    }

    /** Set the time this instance was shown to the current time in ms. */
    public void updateTimeOfFirstShown() {
        mTimeOfFirstShown = SystemClock.elapsedRealtime();
    }

    /** Set the time this instance had native initialization called to the current time in ms. */
    public void updateTimeOfNativeInitialization() {
        mTimeOfNativeInitialization = SystemClock.elapsedRealtime();
    }

    /** Notify the recorder that this instance was shown, and has previously been auto-disabled. */
    public void showAutoDisabledInstance() {
        mTimeOfLastDisabledCall = SystemClock.elapsedRealtime();
    }

    /** Notify the recorder that this instance was hidden, and is currently auto-disabled. */
    public void hideAutoDisabledInstance() {
        mOngoingSumOfTimeDisabled += SystemClock.elapsedRealtime() - mTimeOfLastDisabledCall;
    }

    /** Record UMA histograms for performance-related accessibility metrics. */
    public void recordAccessibilityPerformanceHistograms() {
        // Always track the histograms for events and cache usage statistics.
        recordEventsHistograms();
        recordCacheHistograms();
    }

    /** Record UMA histograms for the event counts for the OnDemand feature. */
    public void recordEventsHistograms() {
        // There are only 2 AXModes, kAXModeComplete is used when a screenreader is active.
        boolean isAXModeComplete = AccessibilityState.isScreenReaderEnabled();
        boolean isAXModeFormControls = AccessibilityState.isOnlyPasswordManagersEnabled();

        // If we did not enqueue any events, we can ignore the data as a trivial case.
        if (mTotalEnqueuedEvents > 0) {
            // Log the percentage dropped (dispatching 0 events should be 100% dropped).
            int percentSent = (int) (mTotalDispatchedEvents * 1.0 / mTotalEnqueuedEvents * 100.0);
            RecordHistogram.recordPercentageHistogram(
                    PERCENTAGE_DROPPED_HISTOGRAM, 100 - percentSent);
            RecordHistogram.recordPercentageHistogram(
                    isAXModeComplete
                            ? PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE
                            : isAXModeFormControls
                                    ? PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS
                                    : PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC,
                    100 - percentSent);

            // Log the total number of dropped events. (Not relevant to be tracked per AXMode)
            RecordHistogram.recordCustomCountHistogram(
                    EVENTS_DROPPED_HISTOGRAM,
                    mTotalEnqueuedEvents - mTotalDispatchedEvents,
                    EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET,
                    EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET,
                    EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT);

            // If 100% of events were dropped, also track the number of dropped events in a
            // separate bucket.
            if (percentSent == 0) {
                RecordHistogram.recordCustomCountHistogram(
                        ONE_HUNDRED_PERCENT_HISTOGRAM,
                        mTotalEnqueuedEvents - mTotalDispatchedEvents,
                        EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET,
                        EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET,
                        EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT);

                RecordHistogram.recordCustomCountHistogram(
                        isAXModeComplete
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

        // Reset counters.
        mTotalEnqueuedEvents = 0;
        mTotalDispatchedEvents = 0;
    }

    /** Record UMA histograms for the AccessibilityNodeInfo cache usage statistics. */
    public void recordCacheHistograms() {
        RecordHistogram.recordCustomCountHistogram(
                CACHE_MAX_NODES_HISTOGRAM,
                mMaxNodesInCache,
                CACHE_MAX_NODES_MIN_BUCKET,
                CACHE_MAX_NODES_MAX_BUCKET,
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

    /** Record UMA histograms for the usage timers of the native accessibility engine. */
    public void recordAccessibilityUsageHistograms() {
        // If the Tab was not shown, the following histograms have no value.
        if (mTimeOfFirstShown < 0) return;

        long now = SystemClock.elapsedRealtime();

        // On activity recreate, or tab reparent, we can get quick succession of show/hide events,
        // and we do not want to record those, so limit to instances > 250ms.
        if (now - mTimeOfFirstShown < 250 /* ms */) {
            mTimeOfFirstShown = -1;
            return;
        }

        // Record the general usage in the foreground, long histograms are up to 1 hour.
        RecordHistogram.recordLongTimesHistogram(USAGE_FOREGROUND_TIME, now - mTimeOfFirstShown);

        // If native was not initialized, the following histograms have no value. Reset and return.
        if (mTimeOfNativeInitialization < 0) {
            mTimeOfFirstShown = -1;
            return;
        }

        // Record native initialized time, long histograms are up to 1 hour.
        RecordHistogram.recordLongTimesHistogram(
                USAGE_NATIVE_INITIALIZED_TIME, now - mTimeOfNativeInitialization);

        // When the foreground and native usage times are close in value, then we will assume this
        // was an instance with an accessibility service always running, and record that usage.
        long timeDiff = Math.abs(mTimeOfNativeInitialization - mTimeOfFirstShown);
        if (timeDiff < 500 /* ms */
                || ((double) timeDiff / (now - mTimeOfFirstShown)) < 0.03 /* % */) {
            RecordHistogram.recordLongTimesHistogram(
                    USAGE_ACCESSIBILITY_ALWAYS_ON_TIME, now - mTimeOfNativeInitialization);
        }

        // Reset values.
        mTimeOfFirstShown = -1;
    }
}
