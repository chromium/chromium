// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.support.annotation.IntDef;
import android.support.annotation.Nullable;

import org.chromium.blink.mojom.ViewportFit;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/**
 * This class receives callbacks that act as hooks for various a native web contents events related
 * to loading a url. A single web contents can have multiple WebContentObservers.
 */
public abstract class WebContentsObserver {
    // TODO(jdduke): Remove the destroy method and hold observer embedders
    // responsible for explicit observer detachment.
    // Using a weak reference avoids cycles that might prevent GC of WebView's WebContents.
    protected WeakReference<WebContents> mWebContents;

    public WebContentsObserver(WebContents webContents) {
        mWebContents = new WeakReference<WebContents>(webContents);
        webContents.addObserver(this);
    }

    /**
     * Called when the RenderView of the current RenderViewHost is ready, e.g. because we recreated
     * it after a crash.
     */
    public void renderViewReady() {}

    public void renderProcessGone(boolean wasOomProtected) {}

    /**
     * Called when the browser process starts a navigation.
     * @param url The validated URL for the loading page.
     * @param isInMainFrame Whether the navigation is for the main frame.
     * @param isSameDocument Whether the main frame navigation did not cause changes to the
     *                   document (for example scrolling to a named anchor or PopState).
     * @param isErrorPage Whether the navigation shows an error page.
     */
    public void didStartNavigation(
            String url, boolean isInMainFrame, boolean isSameDocument, boolean isErrorPage) {}

    /**
     * Called when the current navigation is finished. This happens when a navigation is committed,
     * aborted or replaced by a new one.
     * @param url The validated URL for the loading page.
     * @param isInMainFrame Whether the navigation is for the main frame.
     * @param isErrorPage Whether the navigation shows an error page.
     * @param hasCommitted Whether the navigation has committed. This returns true for either
     *                     successful commits or error pages that replace the previous page
     *                     (distinguished by |isErrorPage|), and false for errors that leave the
     *                     user on the previous page. When false, |isSameDocument|,
     *                     |isFragmentNavigation|, |pageTransition| and |httpStatusCode| will have
     *                     default values.
     * @param isSameDocument Whether the main frame navigation did not cause changes to the
     *                   document (for example scrolling to a named anchor or PopState).
     * @param isFragmentNavigation Whether the navigation was to a different fragment.
     * @param isRendererInitiated Whether initiated by renderer. Eg clicking on a link.
     * @param isDownload See NavigationHandle::IsDownload.
     * @param pageTransition The page transition type associated with this navigation.
     * @param errorCode The net error code if an error occurred prior to commit, otherwise net::OK.
     * @param errorDescription The description for the net error code.
     * @param httpStatusCode The HTTP status code of the navigation.
     */
    public void didFinishNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
            boolean hasCommitted, boolean isSameDocument, boolean isFragmentNavigation,
            boolean isRendererInitiated, boolean isDownload, @Nullable Integer pageTransition,
            int errorCode, String errorDescription, int httpStatusCode) {}

    /**
     * Called when the a page starts loading.
     * @param url The validated url for the loading page.
     */
    public void didStartLoading(String url) {}

    /**
     * Called when the a page finishes loading.
     * @param url The validated url for the page.
     */
    public void didStopLoading(String url) {}

    /**
     * Called when an error occurs while loading a page and/or the page fails to load.
     * @param isMainFrame Whether the navigation occurred in main frame.
     * @param errorCode Error code for the occurring error.
     * @param description The description for the error.
     * @param failingUrl The url that was loading when the error occurred.
     */
    public void didFailLoad(
            boolean isMainFrame, int errorCode, String description, String failingUrl) {}

    /**
     * Called when the page had painted something non-empty.
     */
    public void didFirstVisuallyNonEmptyPaint() {}

    /**
     * The web contents was shown.
     */
    public void wasShown() {}

    /**
     * The web contents was hidden.
     */
    public void wasHidden() {}

    /**
     * Title was set.
     * @param title The updated title.
     */
    public void titleWasSet(String title) {}

    /**
     * Called once the window.document object of the main frame was created.
     */
    public void documentAvailableInMainFrame() {}

    /**
     * Notifies that a load has finished for a given frame.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     * @param validatedUrl The validated URL that is being navigated to.
     * @param isMainFrame Whether the load is happening for the main frame.
     */
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {}

    /**
     * Notifies that the document has finished loading for the given frame.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     */
    public void documentLoadedInFrame(long frameId, boolean isMainFrame) {}

    /**
     * Notifies that a navigation entry has been committed.
     */
    public void navigationEntryCommitted() {}

    /**
     * Called when navigation entries were removed.
     */
    public void navigationEntriesDeleted() {}

    /**
     * Called when an interstitial page gets attached to the tab content.
     */
    public void didAttachInterstitialPage() {}

    /**
     * Called when an interstitial page gets detached from the tab content.
     */
    public void didDetachInterstitialPage() {}

    /**
     * Called when the theme color was changed.
     * @param color the new color in ARGB format
     */
    public void didChangeThemeColor(int color) {}

    /**
     * Called when the Web Contents leaves or enters fullscreen mode.
     * @param isFullscreen whether fullscreen is being entered or left.
     */
    public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {}

    /**
     * The Viewport Fit Type passed to viewportFitChanged. This is mirrored
     * in an enum in display_cutout.mojom.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewportFit.AUTO, ViewportFit.CONTAIN, ViewportFit.COVER})
    public @interface ViewportFitType {}

    /**
     * Called when the viewport fit of the Web Contents changes.
     * @param value the new viewport fit value.
     */
    public void viewportFitChanged(@ViewportFitType int value) {}

    /**
     * This method is invoked when the WebContents reloads the LoFi images on the page.
     */
    public void didReloadLoFiImages() {}

    /**
     * Stop observing the web contents and clean up associated references.
     */
    public void destroy() {
        if (mWebContents == null) return;
        final WebContents webContents = mWebContents.get();
        mWebContents = null;
        if (webContents == null) return;
        webContents.removeObserver(this);
    }

    protected WebContentsObserver() {}
}
