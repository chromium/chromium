// Copyright 2022 The Chromium Authors. All rights reserved.
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
import org.chromium.content_public.browser.UiThreadTaskTraits;

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
    @VisibleForTesting
    private static final long INITIAL_EMISSION_DELAY_MS = 60 * 1000; // 1 min.
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
        mEmitMetricsRunnable = () -> {
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
        LauncherThread.post(() -> { LauncherThread.removeCallbacks(mEmitMetricsRunnable); });
    }

    private void registerActivityStateListenerAndStartEmitting() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            assert ThreadUtils.runningOnUiThread();
            mApplicationInForegroundOnUiThread = ApplicationStatus.getStateForApplication()
                            == ApplicationState.HAS_RUNNING_ACTIVITIES
                    || ApplicationStatus.getStateForApplication()
                            == ApplicationState.HAS_PAUSED_ACTIVITIES;

            ApplicationStatus.registerApplicationStateListener(newState -> {
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

    private boolean bindingManagerHasExclusiveModerateBinding(ChildProcessConnection connection) {
        if (mBindingManager != null) {
            return mBindingManager.hasExclusiveModerateBinding(connection);
        }
        return false;
    }

    // These metrics are only emitted in the foreground.
    @VisibleForTesting
    void emitMetrics() {
        assert LauncherThread.runningOnLauncherThread();

        // Binding counts from all connections.
        int strongBindingCount = 0;
        int moderateBindingCount = 0;
        int waivedBindingCount = 0;

        // Moderate binding from BindingManager which could be waived.
        int waivableBindingCount = 0;

        // Moderate and waived connections if BindingManager didn't exist.
        int contentModerateBindingCount = 0;
        int contentWaivedBindingCount = 0;

        if (mBindingManager != null) {
            waivableBindingCount = mBindingManager.getExclusiveModerateBindingCount();
        }

        for (ChildProcessConnection connection : mConnections) {
            if (connection.isStrongBindingBound()) {
                strongBindingCount++;
            } else if (connection.isModerateBindingBound()) {
                moderateBindingCount++;
                if (bindingManagerHasExclusiveModerateBinding(connection)) {
                    contentWaivedBindingCount++;
                } else {
                    contentModerateBindingCount++;
                }
            } else {
                waivedBindingCount++;
                contentWaivedBindingCount++;
            }
        }

        assert strongBindingCount + moderateBindingCount + waivedBindingCount
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
                "Android.ChildProcessBinding.ModerateConnections", moderateBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.WaivedConnections", waivedBindingCount);

        // Metrics if BindingManager wasn't running.
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.ContentModerateConnections",
                contentModerateBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.ContentWaivedConnections", contentWaivedBindingCount);
        RecordHistogram.recordCount100Histogram(
                "Android.ChildProcessBinding.WaivableConnections", waivableBindingCount);

        // Percentages only if there are connections.
        if (totalConnections > 0) {
            String bucket = getBucket(totalConnections);
            RecordHistogram.recordPercentageHistogram(
                    "Android.ChildProcessBinding.PercentageStrongConnections_" + bucket,
                    Math.round((float) strongBindingCount / totalConnections * 100));
            RecordHistogram.recordPercentageHistogram(
                    "Android.ChildProcessBinding.PercentageModerateConnections_" + bucket,
                    Math.round((float) moderateBindingCount / totalConnections * 100));
            RecordHistogram.recordPercentageHistogram(
                    "Android.ChildProcessBinding.PercentageWaivedConnections_" + bucket,
                    Math.round((float) waivedBindingCount / totalConnections * 100));

            RecordHistogram.recordPercentageHistogram(
                    "Android.ChildProcessBinding.PercentageContentModerateConnections_" + bucket,
                    Math.round((float) contentModerateBindingCount / totalConnections * 100));
            RecordHistogram.recordPercentageHistogram(
                    "Android.ChildProcessBinding.PercentageContentWaivedConnections_" + bucket,
                    Math.round((float) contentWaivedBindingCount / totalConnections * 100));
            RecordHistogram.recordPercentageHistogram(
                    "Android.ChildProcessBinding.PercentageWaivableConnections_" + bucket,
                    Math.round((float) waivableBindingCount / totalConnections * 100));
        }
    }

    private static String getBucket(int totalConnections) {
        if (totalConnections < 3) {
            return "LessThan3Connections";
        } else if (totalConnections < 6) {
            return "3To5Connections";
        } else if (totalConnections < 11) {
            return "6To10Connections";
        } else if (totalConnections < 21) {
            return "11To20Connections";
        } else if (totalConnections < 51) {
            return "21To50Connections";
        }
        return "MoreThan51Connections";
    }
}
