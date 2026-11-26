// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ChildBindingState;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class is used to create a single spare ChildProcessConnection (usually early on during
 * start-up) that can then later be retrieved when a connection to a service is needed.
 */
@NullMarked
public class SpareChildConnection {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(SpareChildConnectionLifecycleEvent)
    @IntDef({
        LifecycleEvent.CREATED,
        LifecycleEvent.CONSUMED,
        LifecycleEvent.UNUSED_DIED,
        LifecycleEvent.CONSUMED_DIED_WHILE_LAUNCHING,
        LifecycleEvent.PRIORITY_LOWERED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface LifecycleEvent {
        // The spare connection was allocated.
        int CREATED = 0;
        // A child process launch consumed the spare connection.
        int CONSUMED = 1;
        // The process died before a launch consumed the spare connection, e.g. because the OS
        // reclaimed it.
        int UNUSED_DIED = 2;
        // A launch consumed the spare connection, but the process died before the launch finished,
        // so the launch failed.
        int CONSUMED_DIED_WHILE_LAUNCHING = 3;
        // The spare connection's binding was lowered while it was unused.
        int PRIORITY_LOWERED = 4;
        int COUNT = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:SpareChildConnectionLifecycleEvent)

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(SpareChildConnectionLoweredStatus)
    @IntDef({
        LoweredStatus.CONSUMED_BEFORE_LOWERED,
        LoweredStatus.CONSUMED_AFTER_LOWERED,
        LoweredStatus.UNUSED_DIED_BEFORE_LOWERED,
        LoweredStatus.UNUSED_DIED_AFTER_LOWERED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface LoweredStatus {
        int CONSUMED_BEFORE_LOWERED = 0;
        int CONSUMED_AFTER_LOWERED = 1;
        int UNUSED_DIED_BEFORE_LOWERED = 2;
        int UNUSED_DIED_AFTER_LOWERED = 3;
        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:SpareChildConnectionLoweredStatus)

    @VisibleForTesting
    static final String LIFECYCLE_EVENT_HISTOGRAM = "Android.SpareChildConnection.LifecycleEvent";

    @VisibleForTesting
    static final String ALIVE_WHEN_REQUESTED_HISTOGRAM =
            "Android.SpareChildConnection.AliveWhenRequested";

    @VisibleForTesting
    static final String LOWERED_STATUS_HISTOGRAM = "Android.SpareChildConnection.LoweredStatus";

    @VisibleForTesting
    static final String TIME_FROM_CONSUMED_TO_DEATH_HISTOGRAM =
            "Android.SpareChildConnection.TimeFromConsumedToDeath";

    // The allocator used to create the connection.
    private final ChildConnectionAllocator mConnectionAllocator;

    // The actual spare connection.
    private @Nullable ChildProcessConnection mConnection;

    // True when there is a spare connection and it is bound.
    private boolean mConnectionReady;

    // The callback that should be called when the connection becomes bound. Set when the connection
    // is retrieved.
    private ChildProcessConnection.@Nullable ServiceCallback mConnectionServiceCallback;

    // The requested initial binding state passed in getConnection.
    // If the connection is not yet bound when getConnection is called, we need to update
    // the binding state in the onChildStarted callback.
    @ChildBindingState @Nullable Integer mRequestedBindingState;

    // True if the process died before the spare connection was consumed.
    private boolean mDiedUnused;

    // True once AliveWhenRequested has been recorded for this spare connection.
    private boolean mAliveWhenRequestedRecorded;

    // Time at which a launch consumed the spare connection.
    private long mConsumedTimeMs;

    // True if lowering the binding was scheduled for this spare connection.
    private boolean mLowerPriorityScheduled;

    // True if the binding was lowered.
    private boolean mPriorityLowered;

    /** Creates and binds a ChildProcessConnection using the specified parameters. */
    public SpareChildConnection(
            Context context, ChildConnectionAllocator connectionAllocator, Bundle serviceBundle) {
        assert LauncherThread.runningOnLauncherThread();

        mConnectionAllocator = connectionAllocator;

        ChildProcessConnection.ServiceCallback serviceCallback =
                new ChildProcessConnection.ServiceCallback() {
                    @Override
                    public void onChildStarted() {
                        assert LauncherThread.runningOnLauncherThread();
                        if (mRequestedBindingState != null) {
                            updateInitialBindingState(mRequestedBindingState);
                        }
                        mConnectionReady = true;
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildStarted();
                            clearConnection();
                        }
                        // If there is no chained callback, that means the spare connection has not
                        // been used yet. It will be cleared when used.
                    }

                    @Override
                    public void onChildProcessDied(ChildProcessConnection connection) {
                        if (mConnectionServiceCallback != null) {
                            recordDeathAfterConsumed(connection);
                            mConnectionServiceCallback.onChildProcessDied(connection);
                        } else if (connection.isServiceDisconnected()) {
                            mDiedUnused = true;
                            recordLifecycleEvent(LifecycleEvent.UNUSED_DIED);
                            recordLoweredStatus(
                                    mPriorityLowered
                                            ? LoweredStatus.UNUSED_DIED_AFTER_LOWERED
                                            : LoweredStatus.UNUSED_DIED_BEFORE_LOWERED);
                        }
                        if (mConnection != null) {
                            assert connection == mConnection;
                            clearConnection();
                        }
                    }
                };

        mConnection =
                mConnectionAllocator.allocate(
                        context, serviceBundle, serviceCallback, ChildBindingState.VISIBLE);
        // allocate() returns null if there is no free slot.
        if (mConnection != null) recordLifecycleEvent(LifecycleEvent.CREATED);
    }

    /**
     * @return a connection that has been bound or is being bound if one was created with the same
     *     allocator as the one provided, null otherwise.
     */
    public @Nullable ChildProcessConnection getConnection(
            ChildConnectionAllocator allocator,
            final ChildProcessConnection.ServiceCallback serviceCallback,
            @ChildBindingState int requestedBindingState) {
        assert LauncherThread.runningOnLauncherThread();
        if (mConnectionAllocator != allocator || mConnectionServiceCallback != null) {
            return null;
        }
        if (mConnection == null) {
            if (mDiedUnused) recordAliveWhenRequested(false);
            return null;
        }

        mConnectionServiceCallback = serviceCallback;
        mConsumedTimeMs = SystemClock.elapsedRealtime();
        recordAliveWhenRequested(true);
        recordLifecycleEvent(LifecycleEvent.CONSUMED);
        recordLoweredStatus(
                mPriorityLowered
                        ? LoweredStatus.CONSUMED_AFTER_LOWERED
                        : LoweredStatus.CONSUMED_BEFORE_LOWERED);

        ChildProcessConnection connection = mConnection;
        if (mConnectionReady) {
            updateInitialBindingState(requestedBindingState);
            // onChildStarted was already run. Call it explicitly on the passed serviceCallback.
            if (serviceCallback != null) {
                // Post a task so the callback happens after the caller has retrieved the
                // connection.
                LauncherThread.post(
                        new Runnable() {
                            @Override
                            public void run() {
                                serviceCallback.onChildStarted();
                            }
                        });
            }
            clearConnection();
        } else {
            mRequestedBindingState = requestedBindingState;
        }
        return connection;
    }

    /** Returns true if no connection is available (so getConnection will always return null), */
    public boolean isEmpty() {
        // Note that if the connection was retrieved but was not yet ready mConnection is non null
        // but that connection is already used and will be cleared when it becomes ready. In that
        // case mConnectionServiceCallback is non null.
        return mConnection == null || mConnectionServiceCallback != null;
    }

    private void clearConnection() {
        assert LauncherThread.runningOnLauncherThread();
        mConnection = null;
        mConnectionReady = false;
    }

    @VisibleForTesting
    public @Nullable ChildProcessConnection getConnection() {
        return mConnection;
    }

    /** Lowers the binding to WAIVED after {@code delayMs}, if the connection is still unused. */
    public void lowerPriorityAfterDelay(int delayMs) {
        assert LauncherThread.runningOnLauncherThread();
        mLowerPriorityScheduled = true;
        LauncherThread.postDelayed(this::lowerPriority, delayMs);
    }

    private void lowerPriority() {
        assert LauncherThread.runningOnLauncherThread();
        if (isEmpty() || mPriorityLowered) return;
        ChildProcessConnection connection = mConnection;
        assert connection != null;
        // The spare connection is created with a visible binding. WAIVED is always held, so
        // removing the visible binding lowers it to WAIVED.
        int visibleBindingCount = connection.getVisibleBindingCount();
        connection.removeVisibleBinding();
        // Nothing is removed if the connection is already unbound, e.g. its process died but the
        // death callback hasn't arrived yet. Don't count that as lowered.
        if (connection.getVisibleBindingCount() == visibleBindingCount) return;
        mPriorityLowered = true;
        recordLifecycleEvent(LifecycleEvent.PRIORITY_LOWERED);
    }

    private void updateInitialBindingState(@ChildBindingState int requestedBindingState) {
        ChildProcessConnection connection = mConnection;
        assert connection != null;
        // The spare connection is created with a visible binding, which lowerPriority() removes.
        @ChildBindingState
        int currentBindingState =
                mPriorityLowered ? ChildBindingState.WAIVED : ChildBindingState.VISIBLE;
        if (requestedBindingState == currentBindingState) return;

        if (requestedBindingState == ChildBindingState.STRONG) {
            connection.addStrongBinding();
        } else if (requestedBindingState == ChildBindingState.VISIBLE) {
            connection.addVisibleBinding();
        } else if (requestedBindingState == ChildBindingState.NOT_PERCEPTIBLE) {
            connection.addNotPerceptibleBinding();
        }
        if (currentBindingState == ChildBindingState.VISIBLE) connection.removeVisibleBinding();
    }

    private void recordDeathAfterConsumed(ChildProcessConnection connection) {
        // Only count deaths the browser didn't cause.
        if (!connection.isServiceDisconnected()) return;
        // The pid is only set once the launch has finished.
        if (connection.getPid() == 0) {
            recordLifecycleEvent(LifecycleEvent.CONSUMED_DIED_WHILE_LAUNCHING);
            return;
        }
        RecordHistogram.recordMediumTimesHistogram(
                TIME_FROM_CONSUMED_TO_DEATH_HISTOGRAM,
                SystemClock.elapsedRealtime() - mConsumedTimeMs);
    }

    private void recordAliveWhenRequested(boolean alive) {
        if (mAliveWhenRequestedRecorded) return;
        mAliveWhenRequestedRecorded = true;
        RecordHistogram.recordBooleanHistogram(ALIVE_WHEN_REQUESTED_HISTOGRAM, alive);
    }

    private void recordLoweredStatus(@LoweredStatus int status) {
        if (!mLowerPriorityScheduled) return;
        RecordHistogram.recordEnumeratedHistogram(
                LOWERED_STATUS_HISTOGRAM, status, LoweredStatus.COUNT);
    }

    private static void recordLifecycleEvent(@LifecycleEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                LIFECYCLE_EVENT_HISTOGRAM, event, LifecycleEvent.COUNT);
    }
}
