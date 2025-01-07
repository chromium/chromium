// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

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
public class ChildProcessConnectionMetrics {
    @VisibleForTesting private static final long INITIAL_EMISSION_DELAY_MS = 60 * 1000; // 1 min.
    private static final long REGULAR_EMISSION_DELAY_MS = 5 * 60 * 1000; // 5 min.

    private static ChildProcessConnectionMetrics sInstance;

    // Whether the main application is currently brought to the foreground.
    private boolean mApplicationInForegroundOnUiThread;
    private BindingManager mBindingManager;

    private final Set<ChildProcessConnection> mConnections = new ArraySet<>();
    private final Random mRandom = new Random();
    private final Runnable mEmitMetricsRunnable;

    @VisibleForTesting
    ChildProcessConnectionMetrics() {
        mEmitMetricsRunnable =
                () -> {
                    emitMetrics();
                    postEmitMetrics(REGULAR_EMISSION_DELAY_MS);
                };
    }

    public static ChildProcessConnectionMetrics getInstance() {
        assert LauncherThread.runningOnLauncherThread();
        if (sInstance == null) {
            sInstance = new ChildProcessConnectionMetrics();
            sInstance.registerActivityStateListenerAndStartEmitting();
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

    private void startEmitting() {
        assert ThreadUtils.runningOnUiThread();
        postEmitMetrics(INITIAL_EMISSION_DELAY_MS);
    }

    private void cancelEmitting() {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(
                () -> {
                    LauncherThread.removeCallbacks(mEmitMetricsRunnable);
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

    private boolean bindingManagerHasExclusiveVisibleBinding(ChildProcessConnection connection) {
        if (mBindingManager != null) {
            return mBindingManager.hasExclusiveVisibleBinding(connection);
        }
        return false;
    }

    // These metrics are only emitted in the foreground.
    @VisibleForTesting
    void emitMetrics() {
        assert LauncherThread.runningOnLauncherThread();

        // Binding counts from all connections.
        int strongBindingCount = 0;
        int visibleBindingCount = 0;
        int notPerceptibleBindingCount = 0;
        int waivedBindingCount = 0;

        // Bindings from BindingManager which could be waived.
        int waivableBindingCount = 0;

        // Visible and waived connections if BindingManager didn't exist.
        int contentVisibleBindingCount = 0;
        int contentWaivedBindingCount = 0;

        if (mBindingManager != null) {
            waivableBindingCount = mBindingManager.getExclusiveBindingCount();
        }

        for (ChildProcessConnection connection : mConnections) {
            if (connection.isStrongBindingBound()) {
                strongBindingCount++;
            } else if (connection.isVisibleBindingBound()) {
                visibleBindingCount++;
                if (bindingManagerHasExclusiveVisibleBinding(connection)) {
                    contentWaivedBindingCount++;
                } else {
                    contentVisibleBindingCount++;
                }
            } else if (connection.isNotPerceptibleBindingBound()) {
                notPerceptibleBindingCount++;
                contentWaivedBindingCount++;
            } else {
                waivedBindingCount++;
                contentWaivedBindingCount++;
            }
        }

        assert strongBindingCount
                        + visibleBindingCount
                        + notPerceptibleBindingCount
                        + waivedBindingCount
                == mConnections.size();
        final int totalConnections = mConnections.size();

        // Count 100 is sufficient here as we are limited to 100 live sandboxed services. See
        // ChildConnectionAllocator.MAX_VARIABLE_ALLOCATED.

        // Actual effecting binding state counts.
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.TotalConnections", totalConnections);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.StrongConnections", strongBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.VisibleConnections", visibleBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.NotPerceptibleConnections",
                notPerceptibleBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.WaivedConnections", waivedBindingCount);

        // Metrics if BindingManager wasn't running.
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.ContentVisibleConnections",
                contentVisibleBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.ContentWaivedConnections", contentWaivedBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.WaivableConnections", waivableBindingCount);
    }
}
