// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ChildBindingState;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.BindService;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.ChildProcessConnectionState;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Random;
import java.util.Set;

/**
 * Tracks {@link ChildProcessConnection} importance and binding state for snaboxed connections. Uses
 * this information to emit metrics comparing the actual {@link ChildBindingState} of a connection
 * to the
 * {@link ChildBindingState} that would be applied based on the {@link ChildProcessImportance} in
 * {@link ChildProcessLauncherHelperImpl#setPriority} if {@link BindingManager} was not running.
 *
 * This class enforces that it is only used on the launcher thread other than during init.
 */
@NullMarked
public class ChildProcessConnectionMetrics {
    @VisibleForTesting private static final long INITIAL_EMISSION_DELAY_MS = 60 * 1000; // 1 min.
    private static final long REGULAR_EMISSION_DELAY_MS = 5 * 60 * 1000; // 5 min.

    private static @Nullable ChildProcessConnectionMetrics sInstance;

    // Whether the main application is currently brought to the foreground.
    private boolean mApplicationInForegroundOnUiThread;
    private @Nullable BindingManager mBindingManager;

    private final Set<ChildProcessConnection> mConnections = new ArraySet<>();
    private final Random mRandom = new Random();
    private final Runnable mEmitMetricsRunnable;
    private final Runnable mEmitBinderIpcCountRunnable;

    @VisibleForTesting
    ChildProcessConnectionMetrics() {
        mEmitMetricsRunnable =
                () -> {
                    emitMetrics();
                    postEmitMetrics(REGULAR_EMISSION_DELAY_MS);
                };
        mEmitBinderIpcCountRunnable =
                () -> {
                    emitBinderIpcCount();
                    postEmitBinderIpcCount();
                };
    }

    public static ChildProcessConnectionMetrics getInstance() {
        assert LauncherThread.runningOnLauncherThread();
        if (sInstance == null) {
            sInstance = new ChildProcessConnectionMetrics();
            sInstance.registerActivityStateListenerAndStartEmitting();
            BindService.setEnableCounting(true);
        }
        return sInstance;
    }

    public void setBindingManager(BindingManager bindingManager) {
        assert LauncherThread.runningOnLauncherThread();
        mBindingManager = bindingManager;
    }

    public void addConnection(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();
        mConnections.add(connection);
    }

    public void removeConnection(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();
        mConnections.remove(connection);
    }

    /**
     * Generate a Poisson distributed time delay.
     * @param meanTimeMs the mean time of the delay.
     */
    private long getTimeDelayMs(long meanTimeMs) {
        return Math.round(-1 * Math.log(mRandom.nextDouble()) * (double) meanTimeMs);
    }

    private void postEmitMetrics(long meanDelayMs) {
        // No thread affinity.
        LauncherThread.postDelayed(mEmitMetricsRunnable, getTimeDelayMs(meanDelayMs));
    }

    private void postEmitBinderIpcCount() {
        // Unlike emitMetrics(), which takes snapshots of the connections and is valid whenever it
        // is taken, emitBinderIpcCount() need to be emitted in every fixed duration because it
        // counts the number of IPC calls during the fixed duration.
        LauncherThread.postDelayed(mEmitBinderIpcCountRunnable, REGULAR_EMISSION_DELAY_MS);
    }

    private void startEmitting() {
        assert ThreadUtils.runningOnUiThread();
        postEmitMetrics(INITIAL_EMISSION_DELAY_MS);
        postEmitBinderIpcCount();
    }

    private void cancelEmitting() {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(
                () -> {
                    LauncherThread.removeCallbacks(mEmitMetricsRunnable);
                    LauncherThread.removeCallbacks(mEmitBinderIpcCountRunnable);
                });
    }

    private void registerActivityStateListenerAndStartEmitting() {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    assert ThreadUtils.runningOnUiThread();
                    mApplicationInForegroundOnUiThread =
                            ApplicationStatus.getStateForApplication()
                                            == ApplicationState.HAS_RUNNING_ACTIVITIES
                                    || ApplicationStatus.getStateForApplication()
                                            == ApplicationState.HAS_PAUSED_ACTIVITIES;

                    ApplicationStatus.registerApplicationStateListener(
                            newState -> {
                                switch (newState) {
                                    case ApplicationState.UNKNOWN:
                                        break;
                                    case ApplicationState.HAS_RUNNING_ACTIVITIES:
                                    case ApplicationState.HAS_PAUSED_ACTIVITIES:
                                        if (!mApplicationInForegroundOnUiThread) onForegrounded();
                                        break;
                                    default:
                                        if (mApplicationInForegroundOnUiThread) onBackgrounded();
                                        break;
                                }
                            });
                    if (mApplicationInForegroundOnUiThread) {
                        startEmitting();
                    }
                });
    }

    private void onForegrounded() {
        assert ThreadUtils.runningOnUiThread();
        mApplicationInForegroundOnUiThread = true;
        startEmitting();
    }

    private void onBackgrounded() {
        assert ThreadUtils.runningOnUiThread();
        mApplicationInForegroundOnUiThread = false;
        cancelEmitting();
    }

    // These metrics are only emitted in the foreground.
    @VisibleForTesting
    void emitMetrics() {
        assert LauncherThread.runningOnLauncherThread();

        // Connection count per binding state.
        int strongConnectionCount = 0;
        int visibleConnectionCount = 0;
        int notPerceptibleConnectionCount = 0;
        int waivedConnectionCount = 0;

        // Raw service binding connection counts from all connections.
        int strongBindingCount = 0;
        int visibleBindingCount = 0;
        int notPerceptibleBindingCount = 0;
        int waivedBindingCount = 0;

        // Connections from BindingManager which could be waived.
        int waivableConnectionCount = 0;

        // Connections with Visible and waived binding if BindingManager didn't exist.
        int contentVisibleConnectionCount = 0;
        int contentWaivedConnectionCount = 0;

        if (mBindingManager != null) {
            waivableConnectionCount = mBindingManager.getExclusiveBindingCount();
        }

        for (ChildProcessConnection connection : mConnections) {
            // Connection count per binding state.
            @ChildBindingState int bindingState = connection.bindingStateCurrent();
            switch (bindingState) {
                case ChildBindingState.STRONG:
                    strongConnectionCount++;
                    break;
                case ChildBindingState.VISIBLE:
                    visibleConnectionCount++;
                    contentVisibleConnectionCount++;
                    break;
                case ChildBindingState.NOT_PERCEPTIBLE:
                    notPerceptibleConnectionCount++;
                    contentWaivedConnectionCount++;
                    break;
                case ChildBindingState.WAIVED:
                case ChildBindingState.UNBOUND:
                    // UNBOUND shouldn't be counted as waived, but we count them for the backward
                    // compatibility. But in practice it should happen rarely and does not matter
                    // much even if it does.
                    waivedConnectionCount++;
                    contentWaivedConnectionCount++;
                    break;
            }

            // Raw service binding connection counts from all connections.
            ChildProcessConnectionState connectionState =
                    connection.getConnectionStateForDebugging();
            if (connectionState.mIsStrongBound) {
                strongBindingCount++;
            }
            if (connectionState.mIsVisibleBound) {
                visibleBindingCount++;
            }
            if (connectionState.mIsNotPerceptibleBound) {
                notPerceptibleBindingCount++;
            }
            if (connectionState.mIsWaivedBound) {
                waivedBindingCount++;
            }
        }

        assert strongConnectionCount
                        + visibleConnectionCount
                        + notPerceptibleConnectionCount
                        + waivedConnectionCount
                == mConnections.size();
        final int totalConnections = mConnections.size();

        // Count 100 is sufficient here as we are limited to 100 live sandboxed services. See
        // ChildConnectionAllocator.MAX_VARIABLE_ALLOCATED.

        // Actual effecting binding state counts.
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.TotalConnections", totalConnections);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.StrongConnections", strongConnectionCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.VisibleConnections", visibleConnectionCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.NotPerceptibleConnections",
                notPerceptibleConnectionCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.WaivedConnections", waivedConnectionCount);

        // Raw service binding connection counts.
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessBinding.StrongBindingCount", strongBindingCount);
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessBinding.VisibleBindingCount", visibleBindingCount);
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessBinding.NotPerceptibleBindingCount",
                notPerceptibleBindingCount);
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessBinding.WaivedBindingCount", waivedBindingCount);
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessBinding.TotalBindingCount",
                strongBindingCount
                        + visibleBindingCount
                        + notPerceptibleBindingCount
                        + waivedBindingCount);

        // Metrics if BindingManager wasn't running.
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.ContentVisibleConnections",
                contentVisibleConnectionCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.ContentWaivedConnections",
                contentWaivedConnectionCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.WaivableConnections", waivableConnectionCount);
    }

    private void emitBinderIpcCount() {
        assert LauncherThread.runningOnLauncherThread();
        BindService.BinderCallCounter counter = BindService.getAndResetBinderCallCounter();
        if (counter == null) {
            return;
        }
        RecordHistogram.recordCount100000Histogram(
                "Android.ChildProcessBinding.BinderIPC.BindService.Count",
                counter.mBindServiceCount);
        RecordHistogram.recordCount100000Histogram(
                "Android.ChildProcessBinding.BinderIPC.UnbindService.Count",
                counter.mUnbindServiceCount);
        RecordHistogram.recordCount100000Histogram(
                "Android.ChildProcessBinding.BinderIPC.UpdateServiceGroup.Count",
                counter.mUpdateServiceGroupCount);
        RecordHistogram.recordCount100000Histogram(
                "Android.ChildProcessBinding.BinderIPC.Total.Count",
                counter.mBindServiceCount
                        + counter.mUnbindServiceCount
                        + counter.mUpdateServiceGroupCount);
    }
}
