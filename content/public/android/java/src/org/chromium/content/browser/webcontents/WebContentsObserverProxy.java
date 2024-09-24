// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.TerminationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

import java.util.Iterator;

/**
 * Serves as a compound observer proxy for dispatching WebContentsObserver callbacks,
 * avoiding redundant JNI-related work when there are multiple Java-based observers.
 */
@JNINamespace("content")
class WebContentsObserverProxy extends WebContentsObserver {
    private long mNativeWebContentsObserverProxy;
    private final ObserverList<WebContentsObserver> mObservers;
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
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().renderFrameCreated(id);
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
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().renderFrameDeleted(id);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void primaryMainFrameRenderProcessGone(@TerminationStatus int terminationStatus) {
        // Don't call handleObserverCall() and finishObserverCall() to explicitly allow a
        // WebContents to be destroyed while handling an this observer call. See
        // https://chromium-review.googlesource.com/c/chromium/src/+/2343269 for details
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().primaryMainFrameRenderProcessGone(terminationStatus);
        }
    }

    @Override
    @CalledByNative
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didStartNavigationInPrimaryMainFrame(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didRedirectNavigation(NavigationHandle navigation) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didRedirectNavigation(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didFinishNavigationInPrimaryMainFrame(navigation);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didStartLoading(GURL url) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didStartLoading(url);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didStopLoading(GURL url, boolean isKnownValid) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didStopLoading(url, isKnownValid);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void loadProgressChanged(float progress) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().loadProgressChanged(progress);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didChangeVisibleSecurityState() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didChangeVisibleSecurityState();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didFailLoad(
            boolean isInPrimaryMainFrame,
            int errorCode,
            GURL failingUrl,
            @LifecycleState int frameLifecycleState) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator
                    .next()
                    .didFailLoad(isInPrimaryMainFrame, errorCode, failingUrl, frameLifecycleState);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didFirstVisuallyNonEmptyPaint() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didFirstVisuallyNonEmptyPaint();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void onVisibilityChanged(@Visibility int visibility) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().onVisibilityChanged(visibility);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void titleWasSet(String title) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().titleWasSet(title);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void primaryMainDocumentElementAvailable() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().primaryMainDocumentElementAvailable();
        }
        finishObserverCall();
    }

    @CalledByNative
    private void didFinishLoadInPrimaryMainFrame(
            int renderProcessId,
            int renderFrameId,
            GURL url,
            boolean isKnownValid,
            @LifecycleState int frameLifecycleState) {
        didFinishLoadInPrimaryMainFrame(
                new GlobalRenderFrameHostId(renderProcessId, renderFrameId),
                url,
                isKnownValid,
                frameLifecycleState);
    }

    @Override
    public void didFinishLoadInPrimaryMainFrame(
            GlobalRenderFrameHostId rfhId,
            GURL url,
            boolean isKnownValid,
            @LifecycleState int rfhLifecycleState) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator
                    .next()
                    .didFinishLoadInPrimaryMainFrame(rfhId, url, isKnownValid, rfhLifecycleState);
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
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().documentLoadedInPrimaryMainFrame(rfhId, rfhLifecycleState);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void navigationEntryCommitted(LoadCommittedDetails details) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().navigationEntryCommitted(details);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void navigationEntriesDeleted() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().navigationEntriesDeleted();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void navigationEntriesChanged() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().navigationEntriesChanged();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void frameReceivedUserActivation() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().frameReceivedUserActivation();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didChangeThemeColor() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().didChangeThemeColor();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void onBackgroundColorChanged() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            WebContentsObserver obs = observersIterator.next();
            obs.onBackgroundColorChanged();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void mediaStartedPlaying() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().mediaStartedPlaying();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void mediaStoppedPlaying() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().mediaStoppedPlaying();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().hasEffectivelyFullscreenVideoChange(isFullscreen);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void didToggleFullscreenModeForTab(boolean enteredFullscreen, boolean willCauseResize) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator
                    .next()
                    .didToggleFullscreenModeForTab(enteredFullscreen, willCauseResize);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().viewportFitChanged(value);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void virtualKeyboardModeChanged(@VirtualKeyboardMode.EnumType int mode) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().virtualKeyboardModeChanged(mode);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void onWebContentsFocused() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().onWebContentsFocused();
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void onWebContentsLostFocus() {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().onWebContentsLostFocus();
        }
        finishObserverCall();
    }

    @Override
    public void onTopLevelNativeWindowChanged(WindowAndroid windowAndroid) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().onTopLevelNativeWindowChanged(windowAndroid);
        }
        finishObserverCall();
    }

    @Override
    @CalledByNative
    public void mediaSessionCreated(MediaSession mediaSession) {
        handleObserverCall();
        Iterator<WebContentsObserver> observersIterator = mObservers.iterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().mediaSessionCreated(mediaSession);
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
        RewindableIterator<WebContentsObserver> observersIterator = mObservers.rewindableIterator();
        for (; observersIterator.hasNext(); ) {
            observersIterator.next().destroy();
        }
        // All observer destroy() implementations should result in their removal
        // from the proxy.
        String remainingObservers = "These observers were not removed: ";
        if (!mObservers.isEmpty()) {
            for (observersIterator.rewind(); observersIterator.hasNext(); ) {
                remainingObservers += observersIterator.next().getClass().getName() + " ";
            }
        }
        assert mObservers.isEmpty() : remainingObservers;
        mObservers.clear();

        if (mNativeWebContentsObserverProxy != 0) {
            WebContentsObserverProxyJni.get()
                    .destroy(mNativeWebContentsObserverProxy, WebContentsObserverProxy.this);
            mNativeWebContentsObserverProxy = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(WebContentsObserverProxy caller, WebContentsImpl webContents);

        void destroy(long nativeWebContentsObserverProxy, WebContentsObserverProxy caller);
    }
}
