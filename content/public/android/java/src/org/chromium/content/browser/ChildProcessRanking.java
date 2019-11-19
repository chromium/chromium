// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;

import org.chromium.base.BuildConfig;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.content_public.browser.ChildProcessImportance;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

/**
 * Ranking of ChildProcessConnections for a particular ChildConnectionAllocator.
 */
public class ChildProcessRanking implements Iterable<ChildProcessConnection> {
    private static final boolean ENABLE_CHECKS = BuildConfig.DCHECK_IS_ON;
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
        @ChildProcessImportance
        public int importance;

        public ConnectionWithRank(ChildProcessConnection connection, boolean visible,
                long frameDepth, boolean intersectsViewport,
                @ChildProcessImportance int importance) {
            this.connection = connection;
            this.visible = visible;
            this.frameDepth = frameDepth;
            this.intersectsViewport = intersectsViewport;
            this.importance = importance;
        }

        // Returns true for low ranked connection that it should be in the low rank group.
        // Note this must be kept up-to-date with RankComparator so that all shouldBeInLowRankGroup
        // connections are sorted to the end of the list.
        // Note being in the low rank group does not necessarily imply the connection is not
        // important or that it only has waived binding.
        public boolean shouldBeInLowRankGroup() {
            boolean inViewport = visible && (frameDepth == 0 || intersectsViewport);
            return importance == ChildProcessImportance.NORMAL && !inViewport;
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
            // * invisible main frame
            // * visible subframe and not intersect viewport
            // * invisible subframes
            // Within each group, ties are broken by intersect viewport and then frame depth where
            // applicable. Note boostForPendingViews is not used for ranking.

            boolean o1IsVisibleMainOrImportant = (o1.visible && o1.frameDepth == 0)
                    || o1.importance == ChildProcessImportance.IMPORTANT;
            boolean o2IsVisibleMainOrImportant = (o2.visible && o2.frameDepth == 0)
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

            boolean o1InvisibleMainFrame = !o1.visible && o1.frameDepth == 0;
            boolean o2InvisibleMainFrame = !o2.visible && o2.frameDepth == 0;
            if (o1InvisibleMainFrame && o2InvisibleMainFrame) {
                return 0;
            } else if (o1InvisibleMainFrame && !o2InvisibleMainFrame) {
                return -1;
            } else if (!o1InvisibleMainFrame && o2InvisibleMainFrame) {
                return 1;
            }

            // The rest of the groups can just be ranked by visibility, intersects viewport, and
            // frame depth.
            if (o1.visible && !o2.visible) {
                return -1;
            } else if (!o1.visible && o2.visible) {
                return 1;
            }
            return compareByIntersectsViewportAndDepth(o1, o2);
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

    private final Runnable mRebindRunnable = this::rebindHighRankConnections;

    private boolean mEnableServiceGroupImportance;
    private boolean mRebindRunnablePending;

    public ChildProcessRanking() {
        mMaxSize = -1;
    }

    /**
     * Create with a maxSize. Trying to insert more will throw exceptions.
     */
    public ChildProcessRanking(int maxSize) {
        assert maxSize > 0;
        mMaxSize = maxSize;
    }

    public void enableServiceGroupImportance() {
        assert !mEnableServiceGroupImportance;
        mEnableServiceGroupImportance = true;
        reshuffleGroupImportance();
        postRebindHighRankConnectionsIfNeeded();
        if (ENABLE_CHECKS) checkGroupImportance();
    }

    /**
     * Iterate from lowest to highest rank. Ranking should not be modified during iteration,
     * including using Iterator.delete.
     */
    @Override
    public Iterator<ChildProcessConnection> iterator() {
        return new ReverseRankIterator();
    }

    public void addConnection(ChildProcessConnection connection, boolean visible, long frameDepth,
            boolean intersectsViewport, @ChildProcessImportance int importance) {
        assert connection != null;
        assert indexOf(connection) == -1;
        if (mMaxSize != -1 && mRankings.size() >= mMaxSize) {
            throw new RuntimeException(
                    "mRankings.size:" + mRankings.size() + " mMaxSize:" + mMaxSize);
        }
        mRankings.add(new ConnectionWithRank(
                connection, visible, frameDepth, intersectsViewport, importance));
        reposition(mRankings.size() - 1);
    }

    public void removeConnection(ChildProcessConnection connection) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;

        // Null is sorted to the end.
        mRankings.remove(i);
        if (ENABLE_CHECKS) checkOrder();
    }

    public void updateConnection(ChildProcessConnection connection, boolean visible,
            long frameDepth, boolean intersectsViewport, @ChildProcessImportance int importance) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;

        ConnectionWithRank rank = mRankings.get(i);
        rank.visible = visible;
        rank.frameDepth = frameDepth;
        rank.intersectsViewport = intersectsViewport;
        rank.importance = importance;
        reposition(i);
    }

    public ChildProcessConnection getLowestRankedConnection() {
        if (mRankings.isEmpty()) return null;
        return mRankings.get(mRankings.size() - 1).connection;
    }

    /**
     * @return reverse rank. Eg lowest ranked connection will have value 0.
     */
    public int getReverseRank(ChildProcessConnection connection) {
        assert connection != null;
        assert mRankings.size() > 0;
        int i = indexOf(connection);
        assert i != -1;
        return mRankings.size() - 1 - i;
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
            if (connection.connection.getGroup() != NO_GROUP) {
                connection.connection.updateGroupImportance(NO_GROUP, 0);
            }
            return;
        }

        final boolean atStart = newIndex == 0;
        final boolean atEnd = newIndex == mRankings.size() - 1;

        final int left =
                atStart ? 0 : mRankings.get(newIndex - 1).connection.getImportanceInGroup();

        assert atEnd || mRankings.get(newIndex + 1).connection.getGroup() > NO_GROUP;
        final int right = atEnd ? Integer.MAX_VALUE
                                : mRankings.get(newIndex + 1).connection.getImportanceInGroup();

        if (connection.connection.getImportanceInGroup() > left
                && connection.connection.getImportanceInGroup() < right) {
            return;
        }

        final int gap = right - left;

        // If there is a large enough gap, place connection close to the end. This is a heuristic
        // since updating a connection to be the highest ranked (lowest index) occurs very
        // frequently, eg when switching between tabs.
        // If gap is small, use average.
        // If there is no room left, reshuffle everything.
        if (gap > 2 * FROM_RIGHT) {
            connection.connection.updateGroupImportance(LOW_RANK_GROUP, right - FROM_RIGHT);
        } else if (gap > 2) {
            connection.connection.updateGroupImportance(LOW_RANK_GROUP, left + gap / 2);
        } else {
            reshuffleGroupImportance();
        }

        postRebindHighRankConnectionsIfNeeded();
        if (ENABLE_CHECKS) checkGroupImportance();
    }

    private void reshuffleGroupImportance() {
        int importance = Integer.MAX_VALUE - FROM_RIGHT;
        for (int i = mRankings.size() - 1; i >= 0; --i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (!connection.shouldBeInLowRankGroup()) break;
            connection.connection.updateGroupImportance(LOW_RANK_GROUP, importance);
            importance -= FROM_RIGHT;
        }
    }

    private void postRebindHighRankConnectionsIfNeeded() {
        if (mRebindRunnablePending) return;
        mHandler.postDelayed(mRebindRunnable, REBIND_DELAY_MS);
        mRebindRunnablePending = true;
    }

    private void rebindHighRankConnections() {
        mRebindRunnablePending = false;
        for (int i = mRankings.size() - 1; i >= 0; --i) {
            ConnectionWithRank connection = mRankings.get(i);
            if (connection.shouldBeInLowRankGroup()) continue;
            connection.connection.rebind();
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
                    throw new RuntimeException("Wrong group importance order " + connection);
                }
                importance = connection.connection.getImportanceInGroup();
            } else {
                if (connection.connection.getGroup() != NO_GROUP) {
                    throw new RuntimeException("Should not be in group " + connection);
                }
            }
        }
    }
}
