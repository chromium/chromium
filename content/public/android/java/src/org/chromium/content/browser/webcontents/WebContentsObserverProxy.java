// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContentsObserver;

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
        mNativeWebContentsObserverProxy = nativeInit(webContents);
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
    public void didStartNavigation(
            String url, boolean isInMainFrame, boolean isSameDocument, boolean isErrorPage) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartNavigation(
                    url, isInMainFrame, isSameDocument, isErrorPage);
        }
    }

    @CalledByNative
    private void didFinishNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
            boolean hasCommitted, boolean isSameDocument, boolean isFragmentNavigation,
            boolean isRendererInitiated, boolean isDownload, int transition, int errorCode,
            String errorDescription, int httpStatusCode) {
        Integer pageTransition = transition == -1 ? null : transition;
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishNavigation(url, isInMainFrame, isErrorPage,
                    hasCommitted, isSameDocument, isFragmentNavigation, isRendererInitiated,
                    isDownload, pageTransition, errorCode, errorDescription, httpStatusCode);
        }
    }

    @Override
    @CalledByNative
    public void didStartLoading(String url) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartLoading(url);
        }
    }

    @Override
    @CalledByNative
    public void didStopLoading(String url) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStopLoading(url);
        }
    }

    @Override
    @CalledByNative
    public void didFailLoad(
            boolean isMainFrame, int errorCode, String description, String failingUrl) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFailLoad(isMainFrame, errorCode, description, failingUrl);
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

    @Override
    @CalledByNative
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishLoad(frameId, validatedUrl, isMainFrame);
        }
    }

    @Override
    @CalledByNative
    public void documentLoadedInFrame(long frameId, boolean isMainFrame) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentLoadedInFrame(frameId, isMainFrame);
        }
    }

    @Override
    @CalledByNative
    public void navigationEntryCommitted() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntryCommitted();
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
    public void didAttachInterstitialPage() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didAttachInterstitialPage();
        }
    }

    @Override
    @CalledByNative
    public void didDetachInterstitialPage() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didDetachInterstitialPage();
        }
    }

    @Override
    @CalledByNative
    public void didChangeThemeColor(int color) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didChangeThemeColor(color);
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
    public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().viewportFitChanged(value);
        }
    }

    @Override
    @CalledByNative
    public void didReloadLoFiImages() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didReloadLoFiImages();
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
            nativeDestroy(mNativeWebContentsObserverProxy);
            mNativeWebContentsObserverProxy = 0;
        }
    }

    private native long nativeInit(WebContentsImpl webContents);
    private native void nativeDestroy(long nativeWebContentsObserverProxy);
}
