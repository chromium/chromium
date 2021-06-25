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

    @CalledByNative
    public void renderFrameCreated(int renderProcessId, int renderFrameId) {
        renderFrameCreated(new GlobalRenderFrameHostId(renderProcessId, renderFrameId));
    }

    @Override
    public void renderFrameCreated(GlobalRenderFrameHostId id) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderFrameCreated(id);
        }
    }

    @CalledByNative
    public void renderFrameDeleted(int renderProcessId, int renderFrameId) {
        renderFrameDeleted(new GlobalRenderFrameHostId(renderProcessId, renderFrameId));
    }

    @Override
    public void renderFrameDeleted(GlobalRenderFrameHostId id) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderFrameDeleted(id);
        }
    }

    @Override
    @CalledByNative
    public void renderViewReady() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderViewReady();
        }
    }

    @Override
    @CalledByNative
    public void renderProcessGone(boolean wasOomProtected) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderProcessGone(wasOomProtected);
        }
    }

    @Override
    @CalledByNative
    public void didStartNavigation(NavigationHandle navigation) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartNavigation(navigation);
        }
    }

    @Override
    @CalledByNative
    public void didRedirectNavigation(NavigationHandle navigation) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didRedirectNavigation(navigation);
        }
    }

    @Override
    @CalledByNative
    public void didFinishNavigation(NavigationHandle navigation) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishNavigation(navigation);
        }
    }

    @Override
    @CalledByNative
    public void didStartLoading(GURL url) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartLoading(url);
        }
    }

    @Override
    @CalledByNative
    public void didStopLoading(GURL url, boolean isKnownValid) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStopLoading(url, isKnownValid);
        }
    }

    @Override
    @CalledByNative
    public void loadProgressChanged(float progress) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().loadProgressChanged(progress);
        }
    }

    @Override
    @CalledByNative
    public void didChangeVisibleSecurityState() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didChangeVisibleSecurityState();
        }
    }

    @Override
    @CalledByNative
    public void didFailLoad(boolean isMainFrame, int errorCode, GURL failingUrl,
            @LifecycleState int frameLifecycleState) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFailLoad(
                    isMainFrame, errorCode, failingUrl, frameLifecycleState);
        }
    }

    @Override
    @CalledByNative
    public void didFirstVisuallyNonEmptyPaint() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFirstVisuallyNonEmptyPaint();
        }
    }

    @Override
    @CalledByNative
    public void wasShown() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().wasShown();
        }
    }

    @Override
    @CalledByNative
    public void wasHidden() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().wasHidden();
        }
    }

    @Override
    @CalledByNative
    public void titleWasSet(String title) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().titleWasSet(title);
        }
    }

    @Override
    @CalledByNative
    public void documentAvailableInMainFrame() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentAvailableInMainFrame();
        }
    }

    @CalledByNative
    private void didFinishLoad(int renderProcessId, int renderFrameId, GURL url,
            boolean isKnownValid, boolean isMainFrame, @LifecycleState int frameLifecycleState) {
        didFinishLoad(new GlobalRenderFrameHostId(renderProcessId, renderFrameId), url,
                isKnownValid, isMainFrame, frameLifecycleState);
    }

    @Override
    public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid,
            boolean isMainFrame, @LifecycleState int rfhLifecycleState) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishLoad(
                    rfhId, url, isKnownValid, isMainFrame, rfhLifecycleState);
        }
    }

    @CalledByNative
    private void documentLoadedInFrame(int renderProcessId, int renderFrameId, boolean isMainFrame,
            @LifecycleState int rfhLifecycleState) {
        documentLoadedInFrame(new GlobalRenderFrameHostId(renderProcessId, renderFrameId),
                isMainFrame, rfhLifecycleState);
    }

    @Override
    public void documentLoadedInFrame(GlobalRenderFrameHostId rfhId, boolean isMainFrame,
            @LifecycleState int rfhLifecycleState) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentLoadedInFrame(rfhId, isMainFrame, rfhLifecycleState);
        }
    }

    @Override
    @CalledByNative
    public void navigationEntryCommitted(LoadCommittedDetails details) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntryCommitted(details);
        }
    }

    @Override
    @CalledByNative
    public void navigationEntriesDeleted() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntriesDeleted();
        }
    }

    @Override
    @CalledByNative
    public void navigationEntriesChanged() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntriesChanged();
        }
    }

    @Override
    @CalledByNative
    public void didChangeThemeColor() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didChangeThemeColor();
        }
    }

    @Override
    @CalledByNative
    public void mediaStartedPlaying() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaStartedPlaying();
        }
    }

    @Override
    @CalledByNative
    public void mediaStoppedPlaying() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaStoppedPlaying();
        }
    }

    @Override
    @CalledByNative
    public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().hasEffectivelyFullscreenVideoChange(isFullscreen);
        }
    }

    @Override
    @CalledByNative
    public void didToggleFullscreenModeForTab(boolean enteredFullscreen, boolean willCauseResize) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didToggleFullscreenModeForTab(
                    enteredFullscreen, willCauseResize);
        }
    }

    @Override
    @CalledByNative
    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().viewportFitChanged(value);
        }
    }

    @Override
    @CalledByNative
    public void onWebContentsFocused() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().onWebContentsFocused();
        }
    }

    @Override
    @CalledByNative
    public void onWebContentsLostFocus() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().onWebContentsLostFocus();
        }
    }

    @Override
    public void onTopLevelNativeWindowChanged(WindowAndroid windowAndroid) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().onTopLevelNativeWindowChanged(windowAndroid);
        }
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
