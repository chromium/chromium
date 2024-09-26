// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.ChildProcessConnection;

import java.util.Iterator;
import java.util.Set;

/**
 * Manages oom bindings used to bound child services.
 * This object must only be accessed from the launcher thread.
 */
class BindingManager implements ComponentCallbacks2 {
    private static final String TAG = "BindingManager";

    public static final int NO_MAX_SIZE = -1;

    // Low reduce ratio of bindings.
    private static final float BINDING_LOW_REDUCE_RATIO = 0.25f;
    // High reduce ratio of binding.
    private static final float BINDING_HIGH_REDUCE_RATIO = 0.5f;

    // Delays used when clearing moderate binding pool when onSentToBackground happens.
    private static final long BINDING_POOL_CLEARER_DELAY_MILLIS = 10 * 1000;

    private static Boolean sUseNotPerceptibleBindingForTesting;

    private final Set<ChildProcessConnection> mConnections = new ArraySet<ChildProcessConnection>();
    // Can be -1 to mean no max size.
    private final int mMaxSize;
    private final Iterable<ChildProcessConnection> mRanking;
    private final Runnable mDelayedClearer;

    // If not null, this is the connection in |mConnections| that does not have a binding added
    // by BindingManager.
    private ChildProcessConnection mWaivedConnection;

    private int mConnectionsDroppedDueToMaxSize;

    @Override
    public void onTrimMemory(final int level) {
        LauncherThread.post(
                new Runnable() {
                    @Override
                    public void run() {
                        Log.i(TAG, "onTrimMemory: level=%d, size=%d", level, mConnections.size());
                        if (mConnections.isEmpty()) {
                            return;
                        }
                        if (level <= TRIM_MEMORY_RUNNING_MODERATE) {
                            reduce(BINDING_LOW_REDUCE_RATIO);
                        } else if (level <= TRIM_MEMORY_RUNNING_LOW) {
                            reduce(BINDING_HIGH_REDUCE_RATIO);
                        } else if (level == TRIM_MEMORY_UI_HIDDEN) {
                            // This will be handled by |mDelayedClearer|.
                            return;
                        } else {
                            removeAllConnections();
                        }
                    }
                });
    }

    @Override
    public void onLowMemory() {
        LauncherThread.post(
                new Runnable() {
                    @Override
                    public void run() {
                        Log.i(TAG, "onLowMemory: evict %d bindings", mConnections.size());
                        removeAllConnections();
                    }
                });
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {}

    private void reduce(float reduceRatio) {
        int oldSize = mConnections.size();
        int newSize = (int) (oldSize * (1f - reduceRatio));
        Log.i(TAG, "Reduce connections from %d to %d", oldSize, newSize);
        removeOldConnections(oldSize - newSize);
        assert mConnections.size() == newSize;
        ensureLowestRankIsWaived();
    }

    private void removeAllConnections() {
        removeOldConnections(mConnections.size());
    }

    private void removeOldConnections(int numberOfConnections) {
        assert numberOfConnections <= mConnections.size();
        int numRemoved = 0;
        for (ChildProcessConnection connection : mRanking) {
            if (mConnections.contains(connection)) {
                removeBindingIfNeeded(connection);
                mConnections.remove(connection);
                if (++numRemoved == numberOfConnections) break;
            }
        }
    }

    private void removeBindingIfNeeded(ChildProcessConnection connection) {
        if (connection == mWaivedConnection) {
            mWaivedConnection = null;
        } else {
            removeBinding(connection);
        }
    }

    private void ensureLowestRankIsWaived() {
        Iterator<ChildProcessConnection> itr = mRanking.iterator();
        if (!itr.hasNext()) return;
        ChildProcessConnection lowestRanked = itr.next();

        if (lowestRanked == mWaivedConnection) return;
        if (mWaivedConnection != null) {
            assert mConnections.contains(mWaivedConnection);
            addBinding(mWaivedConnection);
            mWaivedConnection = null;
        }
        if (!mConnections.contains(lowestRanked)) return;
        removeBinding(lowestRanked);
        mWaivedConnection = lowestRanked;
    }

    /**
     * Called when the embedding application is sent to background.
     * The embedder needs to ensure that:
     *  - every onBroughtToForeground() is followed by onSentToBackground()
     *  - pairs of consecutive onBroughtToForeground() / onSentToBackground() calls do not overlap
     */
    void onSentToBackground() {
        assert LauncherThread.runningOnLauncherThread();

        RecordHistogram.recordCount1000Histogram(
                "Android.BindingManger.ConnectionsDroppedDueToMaxSize",
                mConnectionsDroppedDueToMaxSize);
        mConnectionsDroppedDueToMaxSize = 0;

        if (mConnections.isEmpty()) return;
        LauncherThread.postDelayed(mDelayedClearer, BINDING_POOL_CLEARER_DELAY_MILLIS);
    }

    /** Called when the embedding application is brought to foreground. */
    void onBroughtToForeground() {
        assert LauncherThread.runningOnLauncherThread();
        LauncherThread.removeCallbacks(mDelayedClearer);
    }

    /**
     * @return the number of exclusive bindings that are the result of just BindingManager.
     */
    int getExclusiveBindingCount() {
        int exclusiveBindingCount = 0;
        for (ChildProcessConnection connection : mConnections) {
            if (useNotPerceptibleBinding()
                    ? isExclusiveNotPerceptibleBinding(connection)
                    : isExclusiveVisibleBinding(connection)) {
                exclusiveBindingCount++;
            }
        }
        return exclusiveBindingCount;
    }

    /**
     * @param connection The connection to check if BindingManager has a binding for.
     * @return whether this BindingManager has an exclusive moderate connection.
     */
    boolean hasExclusiveVisibleBinding(ChildProcessConnection connection) {
        return !useNotPerceptibleBinding()
                && mConnections.contains(connection)
                && isExclusiveVisibleBinding(connection);
    }

    /**
     * Override the default behavior which is based on Android version. This can be removed once
     * Android P support ends.
     */
    static void setUseNotPerceptibleBindingForTesting(boolean useNotPerceptibleBinding) {
        sUseNotPerceptibleBindingForTesting = useNotPerceptibleBinding;
        ResettersForTesting.register(() -> sUseNotPerceptibleBindingForTesting = null);
    }

    @VisibleForTesting
    static boolean useNotPerceptibleBinding() {
        if (sUseNotPerceptibleBindingForTesting != null) {
            return sUseNotPerceptibleBindingForTesting;
        }
        return ChildProcessConnection.supportNotPerceptibleBinding();
    }

    private boolean isExclusiveNotPerceptibleBinding(ChildProcessConnection connection) {
        return connection != mWaivedConnection
                && !connection.isStrongBindingBound()
                && !connection.isVisibleBindingBound()
                && connection.getNotPerceptibleBindingCount() == 1;
    }

    private boolean isExclusiveVisibleBinding(ChildProcessConnection connection) {
        return connection != mWaivedConnection
                && !connection.isStrongBindingBound()
                && !connection.isNotPerceptibleBindingBound()
                && connection.getVisibleBindingCount() == 1;
    }

    /**
     * Construct instance with maxSize.
     * @param context Android's context.
     * @param maxSize The maximum number of connections or NO_MAX_SIZE for unlimited connections.
     * @param ranking The ranking of {@link ChildProcessConnection}s based on importance.
     */
    BindingManager(Context context, int maxSize, Iterable<ChildProcessConnection> ranking) {
        assert LauncherThread.runningOnLauncherThread();
        Log.i(TAG, "Visible binding enabled: maxSize=%d", maxSize);

        mMaxSize = maxSize;
        mRanking = ranking;
        if (mMaxSize <= 0 && mMaxSize != NO_MAX_SIZE) {
            throw new IllegalArgumentException(
                    "maxSize must be a positive integer or NO_MAX_SIZE. Was " + maxSize);
        }

        mDelayedClearer =
                new Runnable() {
                    @Override
                    public void run() {
                        Log.i(TAG, "Release visible connections: %d", mConnections.size());
                        removeAllConnections();
                    }
                };

        // Note that it is safe to call Context.registerComponentCallbacks from a background
        // thread.
        context.registerComponentCallbacks(this);
    }

    public void addConnection(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();

        // Note that the size of connections is currently fairly small (40).
        // If it became bigger we should consider using an alternate data structure.
        boolean alreadyInQueue = !mConnections.add(connection);
        if (alreadyInQueue) return;

        addBinding(connection);

        if (mMaxSize != NO_MAX_SIZE && mConnections.size() == mMaxSize + 1) {
            mConnectionsDroppedDueToMaxSize++;
            removeOldConnections(1);
            ensureLowestRankIsWaived();
        }
        assert mMaxSize == NO_MAX_SIZE || mConnections.size() <= mMaxSize;
    }

    public void removeConnection(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();
        boolean alreadyInQueue = mConnections.remove(connection);
        if (alreadyInQueue) removeBindingIfNeeded(connection);
        assert !mConnections.contains(connection);
    }

    // Separate from other public methods so it allows client to update ranking after
    // adding and removing connection.
    public void rankingChanged() {
        ensureLowestRankIsWaived();
    }

    private void addBinding(ChildProcessConnection connection) {
        if (useNotPerceptibleBinding()) {
            connection.addNotPerceptibleBinding();
            return;
        }
        connection.addVisibleBinding();
    }

    private void removeBinding(ChildProcessConnection connection) {
        if (useNotPerceptibleBinding()) {
            connection.removeNotPerceptibleBinding();
            return;
        }
        connection.removeVisibleBinding();
    }
}
