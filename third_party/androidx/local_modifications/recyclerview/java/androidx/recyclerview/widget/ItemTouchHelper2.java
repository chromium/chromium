// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.recyclerview.widget;

import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

/** Extends functionality of the ItemTouchHelper to provide support for external drag actions. */
public class ItemTouchHelper2 extends ItemTouchHelper {
    private static final String TAG = "ItemTouchHelper2";
    private static final int ACTIVE_POINTER_ID_NONE = -1;

    private boolean mExternalDragInProgress = false;
    private RecyclerView.ViewHolder mExternalDragItem;
    private float mExternalDragItemInitialAlpha = 1f;
    private LongPressHandler mExternalLongPressHandler;
    private ItemTouchHelperGestureListenerOverride mItemTouchHelperGestureListenerOverride;

    /** Allows to handle long press events externally. */
    public interface LongPressHandler {
        boolean handleLongPress(MotionEvent motionEvent);
    }

    /**
     * Creates an ItemTouchHelper2 that will work with the given Callback. This is an extension of
     * the default {@link ItemTouchHelper} with extra support for external drag and drop.
     *
     * <p>You can attach ItemTouchHelper2 to a RecyclerView via {@link
     * #attachToRecyclerView(RecyclerView)}. Upon attaching, it will add an item decoration, an
     * onItemTouchListener and a Child attach / detach listener to the RecyclerView.
     *
     * @param callback The Callback which controls the behavior of this touch helper.
     * @param externalLongPressHandler The {@link LongPressHandler} to handle long press events
     *     externally.
     */
    public ItemTouchHelper2(Callback callback, LongPressHandler externalLongPressHandler) {
        super(callback);
        mExternalLongPressHandler = externalLongPressHandler;
    }

    @Override
    public void attachToRecyclerView(RecyclerView recyclerView) {
        super.attachToRecyclerView(recyclerView);
        overrideInternalGestureListener();
    }

    /** Stop listening for any new long press events. */
    private void stopGestureDetectionOverride() {
        if (mGestureDetector != null) {
            mGestureDetector.setIsLongpressEnabled(false);
            mGestureDetector = null;
        }
        if (mItemTouchHelperGestureListenerOverride != null) {
            mItemTouchHelperGestureListenerOverride.doNotReactToLongPress();
            mItemTouchHelperGestureListenerOverride = null;
        }
    }

    /** Override an internal gesture listener if the {@link mExternalLongPressHandler} was set. */
    private void overrideInternalGestureListener() {
        if (mRecyclerView != null && mExternalLongPressHandler != null) {

            // Clean up the currently set gesture detectors.
            stopGestureDetectionOverride();

            mItemTouchHelperGestureListenerOverride =
                    new ItemTouchHelperGestureListenerOverride(mExternalLongPressHandler);
            mGestureDetector =
                    new GestureDetector(
                            mRecyclerView.getContext(), mItemTouchHelperGestureListenerOverride);
        }
    }

    /**
     * Initiates an external drag.
     *
     * @param x current drag x pos
     * @param y current drag y pos
     */
    public void onExternalDragStart(float x, float y, boolean hideItemWhileDragging) {
        if (!mExternalDragInProgress && mItemTouchHelperGestureListenerOverride != null) {
            mActivePointerId = 0;
            MotionEvent tmpEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, x, y, 0);
            mItemTouchHelperGestureListenerOverride.onLongPressInternal(tmpEvent);
            tmpEvent.recycle();

            if (mSelected != null) {
                mExternalDragItem = mSelected;
                mExternalDragItemInitialAlpha = mSelected.itemView.getAlpha();
                if (hideItemWhileDragging) {
                    // Do not use View.setVisibility as this can interfere with context menus that
                    // can use the itemView as an anchor container.
                    mSelected.itemView.setAlpha(0f);
                }
                mExternalDragInProgress = true;
            }
        }
    }

    /**
     * Updates the external drag with the current position.
     *
     * @param x current drag x pos
     * @param y current drag y pos
     */
    public void onExternalDragLocation(float x, float y) {
        if (mExternalDragInProgress && mExternalDragItem != null) {
            MotionEvent tmpEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, x, y, 0);
            updateDxDy(tmpEvent, mSelectedFlags, 0);
            moveIfNecessary(mExternalDragItem);
            mRecyclerView.removeCallbacks(mScrollRunnable);
            mScrollRunnable.run();
            mRecyclerView.invalidate();
            tmpEvent.recycle();
        }
    }

    /**
     * Stops the external drag.
     *
     * @param recoverItem whether the item should be recovered at its' original state and place.
     */
    public void onExternalDragStop(boolean recoverItem) {
        if (mExternalDragInProgress) {
            ViewHolder holder = mExternalDragItem;
            mActivePointerId = ACTIVE_POINTER_ID_NONE;
            mExternalDragInProgress = false;
            mExternalDragItem = null;
            mExternalDragItemInitialAlpha = 1f;
            select(null, ACTION_STATE_IDLE);

            if (holder != null) {
                if (recoverItem) {
                    holder.itemView.setAlpha(mExternalDragItemInitialAlpha);
                }
                // Default recover animation is not required here as it will be provided by the
                // similar functionality of the drag and drop implementation and we do not want two
                // animations to show up at the same time.
                endRecoverAnimation(holder, false);
            }
        }
    }

    /**
     * This is a copy of internal {@link ItemTouchHelper.ItemTouchHelperGestureListener} with an
     * extra support for external handler for the long press events. {@see
     * https://cs.android.com/androidx/platform/frameworks/support/+/androidx-main:recyclerview/recyclerview/src/main/java/androidx/recyclerview/widget/ItemTouchHelper.java;drc=1f824a84c8546b1a51092518dddc8ba0203642ac;l=2309}
     */
    private class ItemTouchHelperGestureListenerOverride
            extends GestureDetector.SimpleOnGestureListener {

        /**
         * Whether to execute code in response to the the invoking of {@link
         * ItemTouchHelperGestureListenerOverride#onLongPress(MotionEvent)}.
         *
         * <p>It is necessary to control this here because {@link
         * GestureDetector.SimpleOnGestureListener} can only be set on a {@link GestureDetector} in
         * a GestureDetector's constructor, a GestureDetector will call onLongPress if an {@link
         * MotionEvent#ACTION_DOWN} event is not followed by another event that would cancel it
         * (like {@link MotionEvent#ACTION_UP} or {@link MotionEvent#ACTION_CANCEL}), the long press
         * responding to the long press event needs to be cancellable to prevent unexpected
         * behavior.
         *
         * @see #doNotReactToLongPress()
         */
        private boolean mShouldReactToLongPress = true;

        private LongPressHandler mExternalLongPressHandler;

        ItemTouchHelperGestureListenerOverride(LongPressHandler externalLongPressHandle) {
            mExternalLongPressHandler = externalLongPressHandle;
        }

        /**
         * Call to prevent executing code in response to {@link
         * ItemTouchHelperGestureListenerOverride#onLongPress(MotionEvent)} being called.
         */
        void doNotReactToLongPress() {
            mShouldReactToLongPress = false;
        }

        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }

        @Override
        public void onLongPress(MotionEvent e) {
            if (!mShouldReactToLongPress) {
                return;
            }

            if (!mExternalLongPressHandler.handleLongPress(e)) {
                onLongPressInternal(e);
            }
        }

        void onLongPressInternal(MotionEvent e) {
            View child = findChildView(e);
            if (child != null) {
                ViewHolder vh = mRecyclerView.getChildViewHolder(child);
                if (vh != null) {
                    if (!mCallback.hasDragFlag(mRecyclerView, vh)) {
                        return;
                    }
                    int pointerId = e.getPointerId(0);
                    // Long press is deferred.
                    // Check w/ active pointer id to avoid selecting after motion
                    // event is canceled.
                    if (pointerId == mActivePointerId) {
                        final int index = e.findPointerIndex(mActivePointerId);
                        final float x = e.getX(index);
                        final float y = e.getY(index);
                        mInitialTouchX = x;
                        mInitialTouchY = y;
                        mDx = mDy = 0f;
                        if (mCallback.isLongPressDragEnabled()) {
                            select(vh, ACTION_STATE_DRAG);
                        }
                    }
                }
            }
        }
    }
}
