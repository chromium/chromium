// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Serves as a compound observer proxy for dispatching WebContentsObserver callbacks,
 * avoiding redundant JNI-related work when there are multiple Java-based observers.
 */
@JNINamespace("content")
class WebContentsObserverProxy extends WebContentsObserver {
    private long mNativeWebContentsObserverProxy;
    private final ObserverList<WebContentsObserver> mObservers;
    private final RewindableIterator<WebContentsObserver> mObserversIterator;
    private int mObserverCallsCurrentlyHandling;

    /**
     * Constructs a new WebContentsObserverProxy for a given WebContents
     * instance. A native WebContentsObserver instance will be created, which
     * will observe the native counterpart to the provided WebContents.
     *
     * @param webContents The WebContents instance to observe.
     */
    public WebContentsObserverProxy(WebContentsImpl webContents) {
        ThreadUtils.assertOnUiThread();
        mNativeWebContentsObserverProxy =
                WebContentsObserverProxyJni.get().init(WebContentsObserverProxy.this, webContents);
        mObservers = new ObserverList<WebContentsObserver>();
        mObserversIterator = mObservers.rewindableIterator();
        mObserverCallsCurrentlyHandling = 0;
    }

    /**
     * Add an observer to the list of proxied observers.
     * @param observer The WebContentsObserver instance to add.
     */
    void addObserver(WebContentsObserver observer) {
        assert mNativeWebContentsObserverProxy != 0;
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer from the list of proxied observers.
     * @param observer The WebContentsObserver instance to remove.
     */
    void removeObserver(WebContentsObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Whether there are any active, proxied observers.
     */
    boolean hasObservers() {
        return !mObservers.isEmpty();
    }

    public boolean isHandlingObserverCall() {
        return mObserverCallsCurrentlyHandling > 0;
    }

    private void handleObserverCall() {
        mObserverCallsCurrentlyHandling++;
    }

    private void finishObserverCall() {
        mObserverCallsCurrentlyHandling--;
    }

    @CalledByNative
    public void renderFrameCreated(int renderProcessId, int renderFrameId) {
        renderFrameCreated(new GlobalRenderFrameHostId(renderProcessId, renderFrameId));
    }

    @Override
    public void renderFrameCreated(GlobalRenderFrameHostId id) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderFrameCreated(id);
        }
        finishObserverCall();
    }

    @CalledByNative
    public void renderFrameDeleted(int renderProcessId, int renderFrameId) {
        renderFrameDeleted(new GlobalRenderFrameHostId(renderProcessId, renderFrameId));
    }

    @Override
    public void renderFrameDeleted(GlobalRenderFrameHostId id) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderFrameDeleted(id);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void renderProcessGone() {
        // Don't call handleObserverCall() and finishObserverCall() to explicitly allow a
        // WebContents to be destroyed while handling an this observer call. See
        // https://chromium-review.googlesource.com/c/chromium/src/+/2343269 for details
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderProcessGone();
        }
    }

    @Override
    @CalledByNative
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartNavigationInPrimaryMainFrame(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didStartNavigationNoop(NavigationHandle navigation) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartNavigationNoop(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didRedirectNavigation(NavigationHandle navigation) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didRedirectNavigation(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didFinishNavigation(NavigationHandle navigation) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishNavigation(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didStartLoading(GURL url) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartLoading(url);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didStopLoading(GURL url, boolean isKnownValid) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStopLoading(url, isKnownValid);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void loadProgressChanged(float progress) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().loadProgressChanged(progress);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didChangeVisibleSecurityState() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didChangeVisibleSecurityState();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didFailLoad(boolean isInPrimaryMainFrame, int errorCode, GURL failingUrl,
            @LifecycleState int frameLifecycleState) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFailLoad(
                    isInPrimaryMainFrame, errorCode, failingUrl, frameLifecycleState);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didFirstVisuallyNonEmptyPaint() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFirstVisuallyNonEmptyPaint();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void wasShown() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().wasShown();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void wasHidden() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().wasHidden();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void titleWasSet(String title) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().titleWasSet(title);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void primaryMainDocumentElementAvailable() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().primaryMainDocumentElementAvailable();
        }
        finishObserverCall();
    }

    @CalledByNative
    private void didFinishLoad(int renderProcessId, int renderFrameId, GURL url,
            boolean isKnownValid, boolean isInPrimaryMainFrame,
            @LifecycleState int frameLifecycleState) {
        didFinishLoad(new GlobalRenderFrameHostId(renderProcessId, renderFrameId), url,
                isKnownValid, isInPrimaryMainFrame, frameLifecycleState);
    }

    @Override
    public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid,
            boolean isInPrimaryMainFrame, @LifecycleState int rfhLifecycleState) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishLoad(
                    rfhId, url, isKnownValid, isInPrimaryMainFrame, rfhLifecycleState);
        }
        finishObserverCall();
    }

    @CalledByNative
    private void documentLoadedInPrimaryMainFrame(
            int renderProcessId, int renderFrameId, @LifecycleState int rfhLifecycleState) {
        documentLoadedInPrimaryMainFrame(
                new GlobalRenderFrameHostId(renderProcessId, renderFrameId), rfhLifecycleState);
    }

    @Override
    public void documentLoadedInPrimaryMainFrame(
            GlobalRenderFrameHostId rfhId, @LifecycleState int rfhLifecycleState) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentLoadedInPrimaryMainFrame(rfhId, rfhLifecycleState);
        }
        finishObserverCall();
    }

    @CalledByNative
    private void documentLoadedInFrameNoop(int renderProcessId, int renderFrameId,
            boolean isInPrimaryMainFrame, @LifecycleState int rfhLifecycleState) {
        documentLoadedInFrameNoop(new GlobalRenderFrameHostId(renderProcessId, renderFrameId),
                false, rfhLifecycleState);
    }

    @Override
    public void documentLoadedInFrameNoop(GlobalRenderFrameHostId rfhId,
            boolean isInPrimaryMainFrame, @LifecycleState int rfhLifecycleState) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentLoadedInFrameNoop(
                    rfhId, isInPrimaryMainFrame, rfhLifecycleState);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void navigationEntryCommitted(LoadCommittedDetails details) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntryCommitted(details);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void navigationEntriesDeleted() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntriesDeleted();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void navigationEntriesChanged() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntriesChanged();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void frameReceivedUserActivation() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().frameReceivedUserActivation();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didChangeThemeColor() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didChangeThemeColor();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void mediaStartedPlaying() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaStartedPlaying();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void mediaStoppedPlaying() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaStoppedPlaying();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().hasEffectivelyFullscreenVideoChange(isFullscreen);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didToggleFullscreenModeForTab(boolean enteredFullscreen, boolean willCauseResize) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didToggleFullscreenModeForTab(
                    enteredFullscreen, willCauseResize);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().viewportFitChanged(value);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void onWebContentsFocused() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().onWebContentsFocused();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void onWebContentsLostFocus() {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().onWebContentsLostFocus();
        }
        finishObserverCall();
    }

    @Override
    public void onTopLevelNativeWindowChanged(WindowAndroid windowAndroid) {
        handleObserverCall();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().onTopLevelNativeWindowChanged(windowAndroid);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void destroy() {
        // Super destruction semantics (removing the observer from the
        // Java-based WebContents) are quite different, so we explicitly avoid
        // calling it here.
        ThreadUtils.assertOnUiThread();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().destroy();
        }
        // All observer destroy() implementations should result in their removal
        // from the proxy.
        assert mObservers.isEmpty();
        mObservers.clear();

        if (mNativeWebContentsObserverProxy != 0) {
            WebContentsObserverProxyJni.get().destroy(
                    mNativeWebContentsObserverProxy, WebContentsObserverProxy.this);
            mNativeWebContentsObserverProxy = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(WebContentsObserverProxy caller, WebContentsImpl webContents);
        void destroy(long nativeWebContentsObserverProxy, WebContentsObserverProxy caller);
    }
}
