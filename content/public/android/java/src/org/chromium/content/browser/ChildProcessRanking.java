// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;

import org.chromium.base.ChildBindingState;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentFeatureList;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

/** Ranking of ChildProcessConnections for a particular ChildConnectionAllocator. */
@NullMarked
public class ChildProcessRanking implements Iterable<ChildProcessConnection> {
    private static final boolean ENABLE_CHECKS = BuildConfig.ENABLE_ASSERTS;
    private static final int NO_GROUP = 0;
    private static final int LOW_RANK_GROUP = 1;

    // If there is a gap in importance that's larger than 2 * FROM_RIGHT, insert connection
    // with importance right - FROM_RIGHT rather than in the middle. Use 15 out of 31 bits
    // so should support 2^16 connections, which should be way more than enough.
    private static final int FROM_RIGHT = 32768;

    // Delay after group is rebound so that higher ranked processes are more recent.
    // Note the post and delay is to avoid extra rebinds when rank for multiple connections
    // change together, eg if visibility changes for a tab with a number of out-of-process
    // iframes.
    private static final int REBIND_DELAY_MS = 1000;

    private static class ConnectionWithRank {
        public final ChildProcessConnection connection;

        // Info for ranking a connection.
        public boolean visible;
        public long frameDepth;
        public boolean intersectsViewport;
        public boolean isSpareRenderer;
        @ChildProcessImportance public int importance;

        public ConnectionWithRank(
                ChildProcessConnection connection,
                boolean visible,
                long frameDepth,
                boolean intersectsViewport,
                boolean isSpareRenderer,
                @ChildProcessImportance int importance) {
            this.connection = connection;
            this.visible = visible;
            this.frameDepth = frameDepth;
            this.intersectsViewport = intersectsViewport;
            this.isSpareRenderer = isSpareRenderer;
            this.importance = importance;
        }

        // Returns true for low ranked connection that it should be in the low rank group.
        // Note this must be kept up-to-date with RankComparator so that all shouldBeInLowRankGroup
        // connections are sorted to the end of the list.
        // Note being in the low rank group does not necessarily imply the connection is not
        // important or that it only has waived binding.
        public boolean shouldBeInLowRankGroup() {
            boolean inViewport = visible && (frameDepth == 0 || intersectsViewport);
            return !inViewport
                    && ((isSpareRenderer && ChildProcessRanking.isSpareRendererOfLowestRanking())
                            || (importance <= ChildProcessImportance.PERCEPTIBLE));
        }
    }

    private static class RankComparator implements Comparator<ConnectionWithRank> {
        private static int compareByIntersectsViewportAndDepth(
                ConnectionWithRank o1, ConnectionWithRank o2) {
            if (o1.intersectsViewport && !o2.intersectsViewport) {
                return -1;
            } else if (!o1.intersectsViewport && o2.intersectsViewport) {
                return 1;
            }

            return Long.signum(o1.frameDepth - o2.frameDepth);
        }

        @Override
        public int compare(ConnectionWithRank o1, ConnectionWithRank o2) {
            assert o1 != null;
            assert o2 != null;

            // Ranking order:
            // * (visible and main frame) or ChildProcessImportance.IMPORTANT
            // * (visible and subframe and intersect viewport) or ChildProcessImportance.MODERATE
            // ---- cutoff for shouldBeInLowRankGroup ----
            // * visible subframe and not intersect viewport
            //   * These processes are bound with NotPerceptibleBinding by BindingManager in
            //   * practice.
            // * ChildProcessImportance.PERCEPTIBLE
            // * invisible main and sub frames (not ranked by frame depth)
            // * spare renderer (if lowest-ranking parameter is set).
            // Within each group, ties are broken by intersect viewport and then frame depth where
            // applicable. Note boostForPendingViews is not used for ranking.

            boolean o1IsVisibleMainOrImportant =
                    (o1.visible && o1.frameDepth == 0)
                            || o1.importance == ChildProcessImportance.IMPORTANT;
            boolean o2IsVisibleMainOrImportant =
                    (o2.visible && o2.frameDepth == 0)
                            || o2.importance == ChildProcessImportance.IMPORTANT;
            if (o1IsVisibleMainOrImportant && o2IsVisibleMainOrImportant) {
                return compareByIntersectsViewportAndDepth(o1, o2);
            } else if (o1IsVisibleMainOrImportant && !o2IsVisibleMainOrImportant) {
                return -1;
            } else if (!o1IsVisibleMainOrImportant && o2IsVisibleMainOrImportant) {
                return 1;
            }

            boolean o1VisibleIntersectSubframeOrModerate =
                    (o1.visible && o1.frameDepth > 0 && o1.intersectsViewport)
                            || o1.importance == ChildProcessImportance.MODERATE;
            boolean o2VisibleIntersectSubframeOrModerate =
                    (o2.visible && o2.frameDepth > 0 && o2.intersectsViewport)
                            || o2.importance == ChildProcessImportance.MODERATE;
            if (o1VisibleIntersectSubframeOrModerate && o2VisibleIntersectSubframeOrModerate) {
                return compareByIntersectsViewportAndDepth(o1, o2);
            } else if (o1VisibleIntersectSubframeOrModerate
                    && !o2VisibleIntersectSubframeOrModerate) {
                return -1;
            } else if (!o1VisibleIntersectSubframeOrModerate
                    && o2VisibleIntersectSubframeOrModerate) {
                return 1;
            }

            if (o1.visible && o2.visible) {
                return compareByIntersectsViewportAndDepth(o1, o2);
            } else if (o1.visible && !o2.visible) {
                return -1;
            } else if (!o1.visible && o2.visible) {
                return 1;
            }

            boolean o1Perceptible = o1.importance == ChildProcessImportance.PERCEPTIBLE;
            boolean o2Perceptible = o2.importance == ChildProcessImportance.PERCEPTIBLE;
            if (o1Perceptible && o2Perceptible) {
                return compareByIntersectsViewportAndDepth(o1, o2);
            } else if (o1Perceptible && !o2Perceptible) {
                return -1;
            } else if (!o1Perceptible && o2Perceptible) {
                return 1;
            }

            if (isSpareRendererOfLowestRanking()) {
                if (!o1.isSpareRenderer && o2.isSpareRenderer) {
                    return -1;
                } else if (o1.isSpareRenderer && !o2.isSpareRenderer) {
                    return 1;
                }
            }

            // Invisible are in one group and are purposefully not ranked by frame depth.
            // This is because a crashed sub frame will cause the whole tab to be reloaded
            // when it becomes visible, so there is no need to specifically protect the
            // main frame or lower depth frames.
            return 0;
        }
    }

    private class ReverseRankIterator implements Iterator<ChildProcessConnection> {
        private final int mSizeOnConstruction;
        private int mNextIndex;

        public ReverseRankIterator() {
            mSizeOnConstruction = ChildProcessRanking.this.mRankings.size();
            mNextIndex = mSizeOnConstruction - 1;
        }

        @Override
        public boolean hasNext() {
            modificationCheck();
            return mNextIndex >= 0;
        }

        @Override
        public ChildProcessConnection next() {
            modificationCheck();
            return ChildProcessRanking.this.mRankings.get(mNextIndex--).connection;
        }

        private void modificationCheck() {
            assert mSizeOnConstruction == ChildProcessRanking.this.mRankings.size();
        }
    }

    private static final RankComparator COMPARATOR = new RankComparator();

    private final Handler mHandler = new Handler();
    // |mMaxSize| can be -1 to indicate there can be arbitrary number of connections.
    private final int mMaxSize;
    // ArrayList is not the most theoretically efficient data structure, but is good enough
    // for sizes in production and more memory efficient than linked data structures.
    private final List<ConnectionWithRank> mRankings = new ArrayList<>();

    // TODO(crbug.com/430428520): Remove this runnable after StrictHighRankProcessLRU launch
    private final Runnable mRebindRunnable = this::rebindHighRankConnections;
    private final Runnable mRebindForConflictRunnable = this::rebindHighRankConnectionsForConflict;

    private boolean mEnableServiceGroupImportance;
    private boolean mRebindRunnablePending;
    // This represents whether the application has window focus.
    //
    // While the application does not have focused window, we assume the application is not in the
    // top-app and has capped oom_score_adj. We use window focus instead of
    // onTopResumedActivityChanged. See the comment on onWindowFocusChanged() for details.
    private boolean mApplicationHasWindowFocus;
    private boolean mApplicationInBackground;

    private static boolean isSpareRendererOfLowestRanking() {
        return ContentFeatureList.sSpareRendererLowestRanking.getValue();
    }

    public ChildProcessRanking() {
        mMaxSize = -1;
    }

    /** Create with a maxSize. Trying to insert more will throw exceptions. */
    public ChildProcessRanking(int maxSize) {
        assert maxSize > 0;
        mMaxSize = maxSize;
    }

    public void enableServiceGroupImportance() {
        assert !mEnableServiceGroupImportance;
        mEnableServiceGroupImportance = true;
        reshuffleGroupImportance();
        if (isStrictHighRankProcessLRUEnabled()) {
            rebindHighRankConnectionsForConflict();
        } else {
            postRebindHighRankConnectionsIfNeeded();
        }
        if (ENABLE_CHECKS) checkGroupImportance();
    }

    /**
     * Called when the window focus changes.
     *
     * <p>This is called on the process launcher thread.
     *
     * <p>This is used to detect whether the application is in the top-app. While the application
     * does not have focused window, we assume the application is not in the top-app and has capped
     * oom_score_adj. We use window focus instead of onTopResumedActivityChanged because of the
     * implementation complexity. window focus and onTopResumedActivityChanged have some mismatch
     * (e.g. the window focus can be dropped by "non-activity windows like dialogs and popups"). we
     * can accept the mismatch because causing extra rebinding Binder IPC while showing
     * "non-activity windows like dialogs and popups" is not that costy.
     *
     * <p>TODO(crbug.com/456638294): Use onTopResumedActivityChanged instead.
     */
    public void onWindowFocusChanged(boolean hasFocus) {
        mApplicationHasWindowFocus = hasFocus;
        if (!hasFocus && isStrictHighRankProcessLRUEnabled()) {
            // Window focus is briefly lost when switching between two Chrome windows. Schedule the
            // deferred rebinding task to avoid unnecessary rebinding if focus is back soon.
            postRebindHighRankConnectionsForConflict();
        }
    }

    public void onSentToBackground() {
        mApplicationInBackground = true;
        rebindHighRankConnectionsForConflict();
    }

    public void onBroughtToForeground() {
        mApplicationInBackground = false;
    }

    /**
     * Called when a connection which may be in low rank group may be updated in terms of service
     * binding state.
     */
    public void onLowRankConnectionMayBeUpdated(ChildProcessConnection connection) {
        if (!isStrictHighRankProcessLRUEnabled()) {
            return;
        }
        assert connection != null;
        int i = indexOf(connection);
        assert i != -1;

        if (mRankings.get(i).shouldBeInLowRankGroup()) {
            postRebindHighRankConnectionsForConflict();
        }
    }

    /**
     * Iterate from lowest to highest rank. Ranking should not be modified during iteration,
     * including using Iterator.delete.
     */
    @Override
    public Iterator<ChildProcessConnection> iterator() {
        return new ReverseRankIterator();
    }

    public void addConnection(
            ChildProcessConnection connection,
            boolean visible,
            long frameDepth,
            boolean intersectsViewport,
            boolean isSpareRenderer,
            @ChildProcessImportance int importance) {
        assert connection != null;
        assert indexOf(connection) == -1;
        if (mMaxSize != -1 && mRankings.size() >= mMaxSize) {
            throw new RuntimeException(
                    "mRankings.size:" + mRankings.size() + " mMaxSize:" + mMaxSize);
        }
        mRankings.add(
                new ConnectionWithRank(
                        connection,
                        visible,
                        frameDepth,
                        intersectsViewport,
                        isSpareRenderer,
                        importance));
        reposition(mRankings.size() - 1);
    }

    public void removeConnection(ChildProcessConnection connection) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;

        // Null is sorted to the end.
        ConnectionWithRank removedConnection = mRankings.remove(i);
        assert removedConnection != null;
        if (isStrictHighRankProcessLRUEnabled() && removedConnection.shouldBeInLowRankGroup()) {
            // On stopping a process, we unbind all service bindings to the process. If the process
            // is in LOW_RANK_GROUP, it is possible that the process is unbound from non-waived
            // binding and all LOW_RANK_GROUP processes are moved to the LRU list in AMS
            // (ActivityManagerService).
            postRebindHighRankConnectionsForConflict();
        }
        if (ENABLE_CHECKS) checkOrder();
    }

    public void updateConnection(
            @Nullable ChildProcessConnection connection,
            boolean visible,
            long frameDepth,
            boolean intersectsViewport,
            boolean isSpareRenderer,
            @ChildProcessImportance int importance) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;

        ConnectionWithRank rank = mRankings.get(i);
        rank.visible = visible;
        rank.frameDepth = frameDepth;
        rank.intersectsViewport = intersectsViewport;
        rank.importance = importance;
        rank.isSpareRenderer = isSpareRenderer;
        reposition(i);
    }

    public @Nullable ChildProcessConnection getLowestRankedConnection() {
        if (mRankings.isEmpty()) return null;
        return mRankings.get(mRankings.size() - 1).connection;
    }

    public void recordProcessRanking() {
        int lowRankCount = 0;
        int highRankCount = 0;
        for (int i = 0; i < mRankings.size(); ++i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (connection.shouldBeInLowRankGroup()) {
                lowRankCount++;
            } else {
                highRankCount++;
            }
        }
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessRanking.LowRank.Count", lowRankCount);
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessRanking.HighRank.Count", highRankCount);
    }

    private int indexOf(ChildProcessConnection connection) {
        for (int i = 0; i < mRankings.size(); ++i) {
            if (mRankings.get(i).connection == connection) return i;
        }
        return -1;
    }

    private void reposition(final int originalIndex) {
        ConnectionWithRank connection = mRankings.remove(originalIndex);
        int newIndex = 0;
        while (newIndex < mRankings.size()
                && COMPARATOR.compare(mRankings.get(newIndex), connection) < 0) {
            ++newIndex;
        }
        mRankings.add(newIndex, connection);
        if (ENABLE_CHECKS) checkOrder();

        if (!mEnableServiceGroupImportance) return;

        if (!connection.shouldBeInLowRankGroup()) {
            if (connection.connection.getGroup() != NO_GROUP
                    && connection.connection.updateGroupImportance(NO_GROUP, 0)) {
                // Rebind a service binding to apply the group importance change.
                connection.connection.rebind();
            }
            if (isStrictHighRankProcessLRUEnabled() && !mApplicationHasWindowFocus) {
                // Need to rebind high rank processes when a high rank process moves within the
                // ranking list while the application is not the top-app. For example, when MODERATE
                // process is updated, we need to rebind high rank processes to keep the IMPORTANT
                // processes at newer position in the LRU list.
                postRebindHighRankConnectionsForConflict();
            }
            return;
        }

        final boolean atStart = newIndex == 0;
        final boolean atEnd = newIndex == mRankings.size() - 1;

        final int left =
                atStart ? 0 : mRankings.get(newIndex - 1).connection.getImportanceInGroup();

        assert atEnd || mRankings.get(newIndex + 1).connection.getGroup() > NO_GROUP;
        final int right =
                atEnd
                        ? Integer.MAX_VALUE
                        : mRankings.get(newIndex + 1).connection.getImportanceInGroup();

        if (connection.connection.getImportanceInGroup() > left
                && connection.connection.getImportanceInGroup() < right) {
            return;
        }

        assert right >= left;
        final int gap = right - left;

        // If there is a large enough gap, place connection close to the end. This is a heuristic
        // since updating a connection to be the highest ranked (lowest index) occurs very
        // frequently, eg when switching between tabs.
        // If gap is small, use average.
        // If there is no room left, reshuffle everything.
        if (gap > 2 * FROM_RIGHT) {
            if (connection.connection.updateGroupImportance(LOW_RANK_GROUP, right - FROM_RIGHT)) {
                // Rebind a service binding to apply the group importance change.
                connection.connection.rebind();
            }
        } else if (gap > 2) {
            if (connection.connection.updateGroupImportance(LOW_RANK_GROUP, left + gap / 2)) {
                // Rebind a service binding to apply the group importance change.
                connection.connection.rebind();
            }
        } else {
            reshuffleGroupImportance();
        }

        if (isStrictHighRankProcessLRUEnabled()) {
            // Post rebinding task with delay because we can expect multiple priority change in a
            // row by a single event (e.g. switching between tabs).
            postRebindHighRankConnectionsForConflict();
        } else {
            postRebindHighRankConnectionsIfNeeded();
        }
        if (ENABLE_CHECKS) checkGroupImportance();
    }

    private void reshuffleGroupImportance() {
        int importance = Integer.MAX_VALUE - FROM_RIGHT;
        ConnectionWithRank lastUpdatedConnection = null;
        for (int i = mRankings.size() - 1; i >= 0; --i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (!connection.shouldBeInLowRankGroup()) break;
            if (connection.connection.updateGroupImportance(LOW_RANK_GROUP, importance)) {
                lastUpdatedConnection = connection;
            }
            importance -= FROM_RIGHT;
        }
        if (lastUpdatedConnection != null) {
            // Rebind a service connection in the group to apply the group importance changes.
            lastUpdatedConnection.connection.rebind();
        }
    }

    private void postRebindHighRankConnectionsForConflict() {
        if (mRebindRunnablePending) {
            return;
        }
        mHandler.postDelayed(mRebindForConflictRunnable, REBIND_DELAY_MS);
        mRebindRunnablePending = true;
    }

    private void postRebindHighRankConnectionsIfNeeded() {
        if (mRebindRunnablePending) return;
        mHandler.postDelayed(mRebindRunnable, REBIND_DELAY_MS);
        mRebindRunnablePending = true;
    }

    // TODO(crbug.com/430428520): Remove this method after StrictHighRankProcessLRU launch.
    private void rebindHighRankConnections() {
        mRebindRunnablePending = false;
        for (int i = mRankings.size() - 1; i >= 0; --i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (connection.shouldBeInLowRankGroup()) continue;
            connection.connection.rebind();
        }
    }

    /**
     * Rebinds high rank connections if they are in conflict with low rank connections or between
     * high rank connections in terms of oom_score_adj.
     *
     * <p>LMK (Low-Memory-Kill) order is sorted by oom_score_adj which is calculated from the
     * service binding flags. The LRU order tracked by AMS(ActivityManagerService) is valued for
     * processes with the same oom_score_adj.
     *
     * <p>When a single low rank process's service binding is changed (i.e. rebinding/unbinding an
     * existing service binding, binding a new service binding), all the low rank processes are
     * moved to the earliest position and will come before any high rank processes in the LRU list
     * in AMS as a result of the service grouping feature of Android.
     *
     * <p>If the oom_score_adj of a high rank process is bigger than (i.e. lower priority) or equal
     * to the smallest oom_score_adj of low rank processes (which is a "conflict"), the high rank
     * connection needs to be re-bound to place it at earlier position in the LRU list.
     *
     * <p>Note that this method should be called when a low rank process is bound, unbound or
     * rebound.
     *
     * <p>This method is also used to keep the LRU order of high rank processes in sync while the
     * application is not in top-app visibility.
     */
    private void rebindHighRankConnectionsForConflict() {
        if (!isStrictHighRankProcessLRUEnabled()) {
            return;
        }
        mHandler.removeCallbacks(mRebindForConflictRunnable);
        mRebindRunnablePending = false;
        @ChildBindingState int targetBindingState = ChildBindingState.WAIVED;
        // While the application is in background, high rank processes may have the same
        // oom_score_adj as low rank processes regardless of the service bindings because
        // oom_score_adj is capped by the browser process. All high rank connections are targets for
        // rebinding. So we can skip find the target binding state if the application is in
        // background.
        //
        // While the application is visible but not in top-app visibility, the oom_score_adj of the
        // application process and child processes are capped by visible priority and oom_score_adj
        // difference between IMPORTANT and MODERATE bindings are gone. The LMKD kill order between
        // the processes are determined by the LRU list order in AMS. We need rebind all high rank
        // processes to keep the LRU order in sync.
        boolean isApplicationInForegroundPriority =
                !mApplicationInBackground && mApplicationHasWindowFocus;
        if (isApplicationInForegroundPriority) {
            // Ranking is independent of the effective binding state. Even low rank processes can
            // have high priority service bindings. For example:
            //
            // * On older Android versions which does not support not-perceptible binding,
            //   BindingManager can set MODERNATE importance for low rank processes.
            // * ChildProcessLauncherHelperImpl.setPriority() can set higher effective importance if
            //   the process in low rank group needs higher cpu priority (e.g. invisible audible
            //   frame).
            //
            // If high rank connection binding state is lower than or equal to the highest binding
            // state of low rank connections, the high rank connection needs to be re-bound to
            // position it
            // earlier in the LRU list in AMS.
            for (int i = mRankings.size() - 1; i >= 0; --i) {
                ConnectionWithRank connection = mRankings.get(i);
                if (connection.shouldBeInLowRankGroup()) {
                    // Call bindingStateCurrent() only once because it touches a lock inside.
                    int state = connection.connection.bindingStateCurrent();
                    if (state > targetBindingState) {
                        targetBindingState = state;
                    }
                } else {
                    break;
                }
            }
        }
        for (int i = mRankings.size() - 1; i >= 0; --i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (!connection.shouldBeInLowRankGroup()
                    && (!isApplicationInForegroundPriority
                            || (connection.connection.bindingStateCurrent()
                                    <= targetBindingState))) {
                connection.connection.rebind();
            }
        }
    }

    private void checkOrder() {
        boolean crossedLowRankGroupCutoff = false;
        for (int i = 0; i < mRankings.size(); ++i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (i > 0 && COMPARATOR.compare(mRankings.get(i - 1), connection) > 0) {
                throw new RuntimeException("Not sorted " + mRankings.get(i - 1) + " " + connection);
            }
            boolean inLowGroup = connection.shouldBeInLowRankGroup();
            if (crossedLowRankGroupCutoff && !inLowGroup) {
                throw new RuntimeException("Not in low rank " + connection);
            }
            crossedLowRankGroupCutoff = inLowGroup;
        }
    }

    private void checkGroupImportance() {
        int importance = -1;
        for (int i = 0; i < mRankings.size(); ++i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (connection.shouldBeInLowRankGroup()) {
                if (connection.connection.getGroup() != LOW_RANK_GROUP) {
                    throw new RuntimeException("Not in low rank group " + connection);
                }
                if (connection.connection.getImportanceInGroup() <= importance) {
                    throw new RuntimeException(
                            "Wrong group importance order "
                                    + connection
                                    + " "
                                    + connection.connection.getImportanceInGroup()
                                    + " "
                                    + importance);
                }
                importance = connection.connection.getImportanceInGroup();
            } else {
                if (connection.connection.getGroup() != NO_GROUP) {
                    throw new RuntimeException("Should not be in group " + connection);
                }
            }
        }
    }

    private boolean isStrictHighRankProcessLRUEnabled() {
        return mEnableServiceGroupImportance
                && ContentFeatureList.sStrictHighRankProcessLRU.isEnabled();
    }
}
