// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.NONE;

import android.view.HapticFeedbackConstants;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.blink.mojom.EventType;
import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.ViewEventSink.InternalAccessDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.GestureEventType;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.util.Collections;
import java.util.HashMap;

/**
 * Implementation of the interface {@link GestureListenerManager}. Manages
 * the {@link GestureStateListener} instances, and invokes them upon
 * notification of various events.
 * Instantiated object is held inside {@link UserDataHost} that is managed by {@link WebContents}.
 */
@JNINamespace("content")
public class GestureListenerManagerImpl
        implements GestureListenerManager,
                WindowEventObserver,
                UserData,
                ViewAndroidDelegate.VerticalScrollDirectionChangeListener {
    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<GestureListenerManagerImpl> INSTANCE =
                GestureListenerManagerImpl::new;
    }

    private static GestureListenerManagerImpl sInstanceForTesting;

    private final WebContentsImpl mWebContents;
    private final ObserverList<GestureStateListener> mListeners;
    private final RewindableIterator<GestureStateListener> mIterator;
    private final HashMap<GestureStateListener, Integer> mListenerFrequency;
    private SelectionPopupControllerImpl mSelectionPopupController;
    private ViewAndroidDelegate mViewDelegate;
    private InternalAccessDelegate mScrollDelegate;
    private final boolean mHidePastePopupOnGSB;
    private final boolean mResetGestureDetectionOnLosingFocus;

    private long mNativeGestureListenerManager;

    /**
     * Whether a user scroll sequence is active, used to hide text selection
     * handles. Note that a scroll sequence will *always* bound a pinch
     * sequence, so this will also be true for the duration of a pinch gesture.
     * A fling is also always bounded by a gesture scroll sequence so this is
     * also true for the duration of the fling.
     */
    private boolean mIsGestureScrollInProgress;

    /** Whether a fling scroll is currently active. */
    private boolean mHasActiveFlingScroll;

    private @RootScrollOffsetUpdateFrequency.EnumType Integer mRootScrollOffsetUpdateFrequency;

    /**
     * @param webContents {@link WebContents} object.
     * @return {@link GestureListenerManager} object used for the give WebContents.
     *         Creates one if not present.
     */
    public static GestureListenerManagerImpl fromWebContents(WebContents webContents) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(
                        GestureListenerManagerImpl.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    // TODO(crbug.com/40850475): Mocking |#fromWebContents()| may be a better option, when
    // available.
    public static void setInstanceForTesting(GestureListenerManagerImpl instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public GestureListenerManagerImpl(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        mListeners = new ObserverList<GestureStateListener>();
        mIterator = mListeners.rewindableIterator();
        mListenerFrequency = new HashMap<>();
        mViewDelegate = mWebContents.getViewAndroidDelegate();
        mViewDelegate.addVerticalScrollDirectionChangeListener(this);
        WindowEventObserverManager.from(mWebContents).addObserver(this);
        mNativeGestureListenerManager =
                GestureListenerManagerImplJni.get()
                        .init(GestureListenerManagerImpl.this, mWebContents);
        mHidePastePopupOnGSB =
                ContentFeatureMap.isEnabled(ContentFeatureList.HIDE_PASTE_POPUP_ON_GSB);
        mResetGestureDetectionOnLosingFocus =
                !ContentFeatureMap.isEnabled(ContentFeatureList.CONTINUE_GESTURE_ON_LOSING_FOCUS);
    }

    public void resetGestureDetection() {
        if (mNativeGestureListenerManager != 0) {
            GestureListenerManagerImplJni.get()
                    .resetGestureDetection(
                            mNativeGestureListenerManager, GestureListenerManagerImpl.this);
        }
    }

    public void setScrollDelegate(InternalAccessDelegate scrollDelegate) {
        mScrollDelegate = scrollDelegate;
    }

    @Override
    public void addListener(GestureStateListener listener) {
        addListener(listener, NONE);
    }

    @Override
    public void addListener(
            GestureStateListener listener,
            @RootScrollOffsetUpdateFrequency.EnumType int frequency) {
        final boolean didAdd = mListeners.addObserver(listener);
        if (mNativeGestureListenerManager != 0 && didAdd) {
            mListenerFrequency.put(listener, frequency);
            boolean frequencyChanged = updateRootScrollOffsetUpdateFrequency();
            if (!frequencyChanged) {
                // If the frequency changed, this update will come from the renderer, so we don't
                // need to call this with the cached offset.
                listener.onScrollOffsetOrExtentChanged(
                        verticalScrollOffset(), verticalScrollExtent());
            }
        }
    }

    @Override
    public void removeListener(GestureStateListener listener) {
        final boolean didRemove = mListeners.removeObserver(listener);
        if (mNativeGestureListenerManager != 0 && didRemove) {
            assert mListenerFrequency.containsKey(listener);
            mListenerFrequency.remove(listener);
            updateRootScrollOffsetUpdateFrequency();
        }
    }

    @Override
    public boolean hasListener(GestureStateListener listener) {
        return mListeners.hasObserver(listener);
    }

    /**
     * Calculates and updates the root scroll offset update frequency.
     * @return Whether the root scroll offset update frequency changed.
     */
    private boolean updateRootScrollOffsetUpdateFrequency() {
        int newFrequency = calculateMaxRootScrollOffsetUpdateFrequency();
        boolean frequencyChanged =
                mRootScrollOffsetUpdateFrequency == null
                        || !mRootScrollOffsetUpdateFrequency.equals(newFrequency);
        mRootScrollOffsetUpdateFrequency = newFrequency;
        if (!frequencyChanged) return false;

        GestureListenerManagerImplJni.get()
                .setRootScrollOffsetUpdateFrequency(
                        mNativeGestureListenerManager, mRootScrollOffsetUpdateFrequency);
        return true;
    }

    private @RootScrollOffsetUpdateFrequency.EnumType int
            calculateMaxRootScrollOffsetUpdateFrequency() {
        if (mListenerFrequency.isEmpty()) return NONE;
        return Collections.max(mListenerFrequency.values());
    }

    @Override
    public void updateMultiTouchZoomSupport(boolean supportsMultiTouchZoom) {
        if (mNativeGestureListenerManager == 0) return;
        GestureListenerManagerImplJni.get()
                .setMultiTouchZoomSupportEnabled(
                        mNativeGestureListenerManager,
                        GestureListenerManagerImpl.this,
                        supportsMultiTouchZoom);
    }

    @Override
    public void updateDoubleTapSupport(boolean supportsDoubleTap) {
        if (mNativeGestureListenerManager == 0) return;
        GestureListenerManagerImplJni.get()
                .setDoubleTapSupportEnabled(
                        mNativeGestureListenerManager,
                        GestureListenerManagerImpl.this,
                        supportsDoubleTap);
    }

    /** Update all the listeners after touch down event occurred. */
    @CalledByNative
    private void updateOnTouchDown() {
        for (mIterator.rewind(); mIterator.hasNext(); ) mIterator.next().onTouchDown();
    }

    /** Returns whether there's an active, ongoing fling scroll. */
    public boolean hasActiveFlingScroll() {
        return mHasActiveFlingScroll;
    }

    @VisibleForTesting
    @RootScrollOffsetUpdateFrequency.EnumType
    public int getRootScrollOffsetUpdateFrequencyForTesting() {
        return calculateMaxRootScrollOffsetUpdateFrequency();
    }

    // WindowEventObserver

    @Override
    public void onWindowFocusChanged(boolean gainFocus) {
        if (mResetGestureDetectionOnLosingFocus) {
            if (!gainFocus) resetGestureDetection();
        }
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator.next().onWindowFocusChanged(gainFocus);
        }
    }

    /**
     * Update all the listeners after vertical scroll offset/extent has changed.
     * @param offset New vertical scroll offset.
     * @param extent New vertical scroll extent, or viewport height.
     */
    public void updateOnScrollChanged(int offset, int extent) {
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            GestureStateListener listener = mIterator.next();
            listener.onScrollOffsetOrExtentChanged(offset, extent);
        }
    }

    /** Update all the listeners after scrolling end event occurred. */
    public void updateOnScrollEnd() {
        setGestureScrollInProgress(false);
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator.next().onScrollEnded(verticalScrollOffset(), verticalScrollExtent());
        }
    }

    /**
     * Update all the listeners after min/max scale limit has changed.
     * @param minScale New minimum scale.
     * @param maxScale New maximum scale.
     */
    public void updateOnScaleLimitsChanged(float minScale, float maxScale) {
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator.next().onScaleLimitsChanged(minScale, maxScale);
        }
    }

    @Override
    public void onVerticalScrollDirectionChanged(boolean directionUp, float currentScrollRatio) {
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator.next().onVerticalScrollDirectionChanged(directionUp, currentScrollRatio);
        }
    }

    /* Called when ongoing fling gesture needs to be reset. */
    private void resetFlingGesture() {
        if (mHasActiveFlingScroll) {
            onFlingEnd();
            mHasActiveFlingScroll = false;
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onFlingEnd() {
        mHasActiveFlingScroll = false;
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator.next().onFlingEndGesture(verticalScrollOffset(), verticalScrollExtent());
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onEventAck(int event, boolean consumed) {
        switch (event) {
            case EventType.GESTURE_FLING_START:
                // If we're here, then |consumed| is false as otherwise #onFlingStart() would have
                // been called by native instead.
                assert !consumed;

                // If a scroll ends with a fling, a SCROLL_END event is never sent.
                // However, if that fling went unconsumed, we still need to let the
                // listeners know that scrolling has ended.
                updateOnScrollEnd();
                break;
            case EventType.GESTURE_SCROLL_UPDATE:
                if (!consumed) break;
                if (!mHidePastePopupOnGSB) {
                    destroyPastePopup();
                }
                for (mIterator.rewind(); mIterator.hasNext(); ) {
                    mIterator.next().onScrollUpdateGestureConsumed();
                }
                break;
            case EventType.GESTURE_SCROLL_END:
                updateOnScrollEnd();
                break;
            case EventType.GESTURE_PINCH_BEGIN:
                for (mIterator.rewind(); mIterator.hasNext(); ) mIterator.next().onPinchStarted();
                break;
            case EventType.GESTURE_PINCH_END:
                for (mIterator.rewind(); mIterator.hasNext(); ) mIterator.next().onPinchEnded();
                break;
            case EventType.GESTURE_TAP:
                destroyPastePopup();
                for (mIterator.rewind(); mIterator.hasNext(); ) {
                    mIterator.next().onSingleTap(consumed);
                }
                break;
            case EventType.GESTURE_LONG_PRESS:
                if (!consumed) break;
                mViewDelegate
                        .getContainerView()
                        .performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                break;
            case EventType.GESTURE_BEGIN:
                for (mIterator.rewind(); mIterator.hasNext(); ) {
                    mIterator.next().onGestureBegin();
                }
                break;
            case EventType.GESTURE_END:
                for (mIterator.rewind(); mIterator.hasNext(); ) {
                    mIterator.next().onGestureEnd();
                }
                break;
            default:
                break;
        }
    }

    /** Called when a gesture event ack happens for |EventType.GESTURE_SCROLL_BEGIN|. */
    @CalledByNative
    @VisibleForTesting
    void onScrollBegin(boolean isDirectionUp) {
        setGestureScrollInProgress(true);
        if (mHidePastePopupOnGSB) {
            destroyPastePopup();
        }
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator
                    .next()
                    .onScrollStarted(verticalScrollOffset(), verticalScrollExtent(), isDirectionUp);
        }
    }

    /** Called when a gesture event ack happens for |EventType.GESTURE_FLING_START|. */
    @CalledByNative
    @VisibleForTesting
    void onFlingStart(boolean isDirectionUp) {
        // The view expects the fling velocity in pixels/s.
        mHasActiveFlingScroll = true;
        for (mIterator.rewind(); mIterator.hasNext(); ) {
            mIterator
                    .next()
                    .onFlingStartGesture(
                            verticalScrollOffset(), verticalScrollExtent(), isDirectionUp);
        }
    }

    private void destroyPastePopup() {
        if (mSelectionPopupController == null) {
            mSelectionPopupController =
                    SelectionPopupControllerImpl.fromWebContentsNoCreate(mWebContents);
        }
        if (mSelectionPopupController != null
                && mSelectionPopupController.isPasteActionModeValid()) {
            mSelectionPopupController.destroyActionModeAndKeepSelection();
        }
    }

    @CalledByNative
    @VisibleForTesting
    void resetPopupsAndInput(boolean renderProcessGone) {
        PopupController.hidePopupsAndClearSelection(mWebContents);
        resetScrollInProgress();
        if (renderProcessGone) {
            ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(mWebContents);
            if (imeAdapter != null) imeAdapter.resetAndHideKeyboard();
        }
    }

    @CalledByNative
    private void onNativeDestroyed() {
        for (mIterator.rewind(); mIterator.hasNext(); ) mIterator.next().onDestroyed();
        mListeners.clear();
        mListenerFrequency.clear();
        mViewDelegate.removeVerticalScrollDirectionChangeListener(this);
        mNativeGestureListenerManager = 0;
    }

    /** Called just prior to a tap or press gesture being forwarded to the renderer. */
    @SuppressWarnings("unused")
    @CalledByNative
    private boolean filterTapOrPressEvent(int type, int x, int y) {
        if (type == GestureEventType.LONG_PRESS && offerLongPressToEmbedder()) {
            return true;
        }

        return false;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updateScrollInfo(
            float scrollOffsetX,
            float scrollOffsetY,
            float pageScaleFactor,
            float minPageScaleFactor,
            float maxPageScaleFactor,
            float contentWidth,
            float contentHeight,
            float viewportWidth,
            float viewportHeight,
            float topBarShownPix,
            boolean topBarChanged) {
        TraceEvent.begin("GestureListenerManagerImpl:updateScrollInfo");
        RenderCoordinatesImpl rc = mWebContents.getRenderCoordinates();

        // Adjust contentWidth/Height to be always at least as big as
        // the actual viewport (as set by onSizeChanged).
        final float deviceScale = rc.getDeviceScaleFactor();
        View containerView = mViewDelegate.getContainerView();
        contentWidth =
                Math.max(contentWidth, containerView.getWidth() / (deviceScale * pageScaleFactor));
        contentHeight =
                Math.max(
                        contentHeight, containerView.getHeight() / (deviceScale * pageScaleFactor));

        final boolean scaleLimitsChanged =
                minPageScaleFactor != rc.getMinPageScaleFactor()
                        || maxPageScaleFactor != rc.getMaxPageScaleFactor();
        final boolean pageScaleChanged = pageScaleFactor != rc.getPageScaleFactor();
        final boolean scrollChanged =
                pageScaleChanged
                        || scrollOffsetX != rc.getScrollX()
                        || scrollOffsetY != rc.getScrollY();

        if (scrollChanged) {
            onRootScrollOffsetChangedImpl(pageScaleFactor, scrollOffsetX, scrollOffsetY);
        }

        // TODO(jinsukkim): Consider updating the info directly through RenderCoordinates.
        rc.updateFrameInfo(
                contentWidth,
                contentHeight,
                viewportWidth,
                viewportHeight,
                minPageScaleFactor,
                maxPageScaleFactor,
                topBarShownPix);

        if (!scrollChanged && topBarChanged) {
            updateOnScrollChanged(verticalScrollOffset(), verticalScrollExtent());
        }
        if (scaleLimitsChanged) updateOnScaleLimitsChanged(minPageScaleFactor, maxPageScaleFactor);
        TraceEvent.end("GestureListenerManagerImpl:updateScrollInfo");
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRootScrollOffsetChanged(float scrollOffsetX, float scrollOffsetY) {
        onRootScrollOffsetChangedImpl(
                mWebContents.getRenderCoordinates().getPageScaleFactor(),
                scrollOffsetX,
                scrollOffsetY);
    }

    private void onRootScrollOffsetChangedImpl(
            float pageScaleFactor, float scrollOffsetX, float scrollOffsetY) {
        TraceEvent.begin("GestureListenerManagerImpl:onRootScrollOffsetChanged");

        notifyDelegateOfScrollChange(scrollOffsetX, scrollOffsetY);

        mWebContents
                .getRenderCoordinates()
                .updateScrollInfo(pageScaleFactor, scrollOffsetX, scrollOffsetY);

        updateOnScrollChanged(verticalScrollOffset(), verticalScrollExtent());
        TraceEvent.end("GestureListenerManagerImpl:onRootScrollOffsetChanged");
    }

    private void notifyDelegateOfScrollChange(float scrollOffsetX, float scrollOffsetY) {
        RenderCoordinatesImpl rc = mWebContents.getRenderCoordinates();
        mScrollDelegate.onScrollChanged(
                (int) rc.fromLocalCssToPix(scrollOffsetX),
                (int) rc.fromLocalCssToPix(scrollOffsetY),
                (int) rc.getScrollXPix(),
                (int) rc.getScrollYPix());
    }

    @Override
    @CalledByNative
    public boolean isScrollInProgress() {
        return mIsGestureScrollInProgress;
    }

    private void setGestureScrollInProgress(boolean gestureScrollInProgress) {
        mIsGestureScrollInProgress = gestureScrollInProgress;

        if (mSelectionPopupController == null) {
            mSelectionPopupController = SelectionPopupControllerImpl.fromWebContents(mWebContents);
        }
        // Use the active scroll signal for hiding. The animation movement by
        // fling will naturally hide the ActionMode by invalidating its content
        // rect.
        mSelectionPopupController.setScrollInProgress(isScrollInProgress());
    }

    /**
     * Reset scroll and fling accounting, notifying listeners as appropriate.
     * This is useful as a failsafe when the input stream may have been interruped.
     */
    private void resetScrollInProgress() {
        if (!isScrollInProgress()) return;

        final boolean gestureScrollInProgress = mIsGestureScrollInProgress;
        setGestureScrollInProgress(false);
        if (gestureScrollInProgress) {
            updateOnScrollEnd();
        }
        resetFlingGesture();
    }

    /**
     * Offer a long press gesture to the embedding View, primarily for WebView compatibility.
     *
     * @return true if the embedder handled the event.
     */
    private boolean offerLongPressToEmbedder() {
        return mViewDelegate.getContainerView().performLongClick();
    }

    private int verticalScrollOffset() {
        return mWebContents.getRenderCoordinates().getScrollYPixInt();
    }

    private int verticalScrollExtent() {
        return mWebContents.getRenderCoordinates().getLastFrameViewportHeightPixInt();
    }

    @NativeMethods
    interface Natives {
        long init(GestureListenerManagerImpl caller, WebContentsImpl webContents);

        void resetGestureDetection(
                long nativeGestureListenerManager, GestureListenerManagerImpl caller);

        void setDoubleTapSupportEnabled(
                long nativeGestureListenerManager,
                GestureListenerManagerImpl caller,
                boolean enabled);

        void setMultiTouchZoomSupportEnabled(
                long nativeGestureListenerManager,
                GestureListenerManagerImpl caller,
                boolean enabled);

        void setRootScrollOffsetUpdateFrequency(
                long nativeGestureListenerManager,
                @RootScrollOffsetUpdateFrequency.EnumType int frequency);
    }
}
