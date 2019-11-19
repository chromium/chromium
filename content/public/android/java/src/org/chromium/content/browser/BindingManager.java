// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;
import android.support.v4.util.ArraySet;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
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

    // Low reduce ratio of moderate binding.
    private static final float MODERATE_BINDING_LOW_REDUCE_RATIO = 0.25f;
    // High reduce ratio of moderate binding.
    private static final float MODERATE_BINDING_HIGH_REDUCE_RATIO = 0.5f;

    // Delays used when clearing moderate binding pool when onSentToBackground happens.
    private static final long MODERATE_BINDING_POOL_CLEARER_DELAY_MILLIS = 10 * 1000;

    private final Set<ChildProcessConnection> mConnections = new ArraySet<ChildProcessConnection>();
    // Can be -1 to mean no max size.
    private final int mMaxSize;
    private final Iterable<ChildProcessConnection> mRanking;
    private final Runnable mDelayedClearer;

    // If not null, this is a connection in |mConnections| that does not have a moderate binding
    // added by BindingManager.
    private ChildProcessConnection mWaivedConnection;

    @Override
    public void onTrimMemory(final int level) {
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "onTrimMemory: level=%d, size=%d", level, mConnections.size());
                if (mConnections.isEmpty()) {
                    return;
                }
                if (level <= TRIM_MEMORY_RUNNING_MODERATE) {
                    reduce(MODERATE_BINDING_LOW_REDUCE_RATIO);
                } else if (level <= TRIM_MEMORY_RUNNING_LOW) {
                    reduce(MODERATE_BINDING_HIGH_REDUCE_RATIO);
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
        LauncherThread.post(new Runnable() {
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
                removeModerateBindingIfNeeded(connection);
                mConnections.remove(connection);
                if (++numRemoved == numberOfConnections) break;
            }
        }
    }

    private void removeModerateBindingIfNeeded(ChildProcessConnection connection) {
        if (connection == mWaivedConnection) {
            mWaivedConnection = null;
        } else {
            connection.removeModerateBinding();
        }
    }

    private void ensureLowestRankIsWaived() {
        Iterator<ChildProcessConnection> itr = mRanking.iterator();
        if (!itr.hasNext()) return;
        ChildProcessConnection lowestRanked = itr.next();

        if (lowestRanked == mWaivedConnection) return;
        if (mWaivedConnection != null) {
            assert mConnections.contains(mWaivedConnection);
            mWaivedConnection.addModerateBinding();
            mWaivedConnection = null;
        }
        if (!mConnections.contains(lowestRanked)) return;
        lowestRanked.removeModerateBinding();
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
        if (mConnections.isEmpty()) return;
        LauncherThread.postDelayed(mDelayedClearer, MODERATE_BINDING_POOL_CLEARER_DELAY_MILLIS);
    }

    /** Called when the embedding application is brought to foreground. */
    void onBroughtToForeground() {
        assert LauncherThread.runningOnLauncherThread();
        LauncherThread.removeCallbacks(mDelayedClearer);
    }

    /**
     * Construct instance without maxsize and can support arbitrary number of connections.
     */
    BindingManager(Context context, Iterable<ChildProcessConnection> ranking) {
        this(-1, ranking, context);
    }

    /**
     * Construct instance with maxSize.
     */
    BindingManager(Context context, int maxSize, Iterable<ChildProcessConnection> ranking) {
        this(maxSize, ranking, context);
        assert maxSize > 0;
    }

    private BindingManager(int maxSize, Iterable<ChildProcessConnection> ranking, Context context) {
        assert LauncherThread.runningOnLauncherThread();
        Log.i(TAG, "Moderate binding enabled: maxSize=%d", maxSize);

        mMaxSize = maxSize;
        mRanking = ranking;
        assert mMaxSize > 0 || mMaxSize == -1;

        mDelayedClearer = new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "Release moderate connections: %d", mConnections.size());
                // Tests may not load the native library which is required for
                // recording histograms.
                if (LibraryLoader.getInstance().isInitialized()) {
                    RecordHistogram.recordCountHistogram(
                            "Android.ModerateBindingCount", mConnections.size());
                }
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
        if (!alreadyInQueue) connection.addModerateBinding();
        assert mMaxSize == -1 || mConnections.size() <= mMaxSize;
    }

    public void removeConnection(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();
        boolean alreadyInQueue = mConnections.remove(connection);
        if (alreadyInQueue) removeModerateBindingIfNeeded(connection);
        assert !mConnections.contains(connection);
    }

    // Separate from other public methods so it allows client to update ranking after
    // adding and removing connection.
    public void rankingChanged() {
        ensureLowestRankIsWaived();
    }
}
