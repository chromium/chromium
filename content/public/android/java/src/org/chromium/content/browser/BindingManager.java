// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;

import androidx.collection.ArraySet;

import org.chromium.base.ChildBindingState;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Iterator;
import java.util.Set;
import java.util.function.Consumer;

/**
 * Manages oom bindings used to bound child services.
 * This object must only be accessed from the launcher thread.
 */
@NullMarked
class BindingManager implements ComponentCallbacks2 {
    private static final String TAG = "BindingManager";

    public static final int NO_MAX_SIZE = -1;

    // Low reduce ratio of bindings.
    private static final float BINDING_LOW_REDUCE_RATIO = 0.25f;
    // High reduce ratio of binding.
    private static final float BINDING_HIGH_REDUCE_RATIO = 0.5f;

    // Delays used when clearing moderate binding pool when onSentToBackground happens.
    private static final long BINDING_POOL_CLEARER_DELAY_MILLIS = 10 * 1000;

    private final Set<ChildProcessConnection> mConnections = new ArraySet<ChildProcessConnection>();
    // Can be -1 to mean no max size.
    private final int mMaxSize;
    private final Iterable<ChildProcessConnection> mRanking;
    private final Runnable mDelayedClearer;
    private final @Nullable Consumer<ChildProcessConnection> mOnChangedImplicitly;

    // If not null, this is the connection in |mConnections| that does not have a binding added
    // by BindingManager.
    private @Nullable ChildProcessConnection mWaivedConnection;

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
        ChildProcessConnection firstRemovedConnection = null;
        int numRemoved = 0;
        for (ChildProcessConnection connection : mRanking) {
            if (mConnections.contains(connection)) {
                boolean bindingRemoved = removeBindingIfNeeded(connection);
                mConnections.remove(connection);
                if (bindingRemoved && firstRemovedConnection == null) {
                    firstRemovedConnection = connection;
                }
                if (++numRemoved == numberOfConnections) break;
            }
        }
        if (firstRemovedConnection != null && mOnChangedImplicitly != null) {
            mOnChangedImplicitly.accept(firstRemovedConnection);
        }
    }

    private boolean removeBindingIfNeeded(ChildProcessConnection connection) {
        boolean removeBinding = connection != mWaivedConnection;
        if (removeBinding) {
            removeBinding(connection);
        } else {
            mWaivedConnection = null;
        }
        return removeBinding;
    }

    private void ensureLowestRankIsWaived() {
        Iterator<ChildProcessConnection> itr = mRanking.iterator();
        if (!itr.hasNext()) return;
        ChildProcessConnection lowestRanked = itr.next();

        if (lowestRanked == mWaivedConnection) return;
        if (mWaivedConnection != null) {
            assert mConnections.contains(mWaivedConnection);
            addBinding(mWaivedConnection);
            if (mOnChangedImplicitly != null) {
                mOnChangedImplicitly.accept(mWaivedConnection);
            }
            mWaivedConnection = null;
        }
        if (!mConnections.contains(lowestRanked)) return;
        removeBinding(lowestRanked);
        if (mOnChangedImplicitly != null) {
            mOnChangedImplicitly.accept(lowestRanked);
        }
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
            if (isExclusiveNotPerceptibleBinding(connection)) {
                exclusiveBindingCount++;
            }
        }
        return exclusiveBindingCount;
    }

    private boolean isExclusiveNotPerceptibleBinding(ChildProcessConnection connection) {
        return connection != mWaivedConnection
                && connection.bindingStateCurrent() < ChildBindingState.VISIBLE
                && connection.getNotPerceptibleBindingCount() == 1;
    }

    /**
     * Construct instance with maxSize.
     *
     * @param context Android's context.
     * @param maxSize The maximum number of connections or NO_MAX_SIZE for unlimited connections.
     * @param ranking The ranking of {@link ChildProcessConnection}s based on importance.
     * @param onChangedImplicitly A callback that is run when connections are bound/unbound to/from
     *     a service binding implicitly. Note that this does not report the binding change is for
     *     explict connection (e.g. the added connection on addConnection()), but this reports for
     *     the connections that are bound/unbound for ensureLowestRankIsWaived(),
     *     removeOldConnections(). When multiple connections are bound/unbound in a row, this skips
     *     some of them but just reports the lowest ranked connection.
     */
    BindingManager(
            Context context,
            int maxSize,
            Iterable<ChildProcessConnection> ranking,
            @Nullable Consumer<ChildProcessConnection> onChangedImplicitly) {
        assert LauncherThread.runningOnLauncherThread();
        Log.i(TAG, "Visible binding enabled: maxSize=%d", maxSize);

        mMaxSize = maxSize;
        mOnChangedImplicitly = onChangedImplicitly;
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
        connection.addNotPerceptibleBinding();
    }

    private void removeBinding(ChildProcessConnection connection) {
        connection.removeNotPerceptibleBinding();
    }
}
