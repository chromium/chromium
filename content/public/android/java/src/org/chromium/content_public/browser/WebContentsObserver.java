// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

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
     * @param navigationHandle
     *        NavigationHandle are provided to several WebContentsObserver methods to allow
     *        observers to track specific navigations. Observers should clear any references to a
     *        NavigationHandle at didFinishNavigation();
     */
    public void didStartNavigation(NavigationHandle navigationHandle) {}

    /**
     * Called when the browser process redirect a navigation.
     * @param navigationHandle
     *        NavigationHandle are provided to several WebContentsObserver methods to allow
     *        observers to track specific navigations. Observers should clear any references to a
     *        NavigationHandle at didFinishNavigation();
     */
    public void didRedirectNavigation(NavigationHandle navigationHandle) {}

    /**
     * Called when the current navigation is finished. This happens when a navigation is committed,
     * aborted or replaced by a new one.
     * @param navigationHandle
     *        NavigationHandle are provided to several WebContentsObserver methods to allow
     *        observers to track specific navigations. Observers should clear any references to a
     *        NavigationHandle at the end of this function.
     */
    public void didFinishNavigation(NavigationHandle navigationHandle) {}

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
     * Called when a page's load progress has changed.
     * @param progress The load progress in the range of [0,1].
     */
    public void loadProgressChanged(float progress) {}

    /**
     * Called when a page's visible security state has changed.
     */
    public void didChangeVisibleSecurityState() {}

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
     * Called when navigation entries were changed.
     */
    public void navigationEntriesChanged() {}

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
     * This method is invoked when a RenderWidgetHost for a WebContents gains focus.
     */
    public void onWebContentsFocused() {}

    /**
     * This method is invoked when a RenderWidgetHost for a WebContents loses focus. This may
     * be immediately followed by onWebContentsFocused if focus was moving between two
     * RenderWidgetHosts within the same WebContents.
     */
    public void onWebContentsLostFocus() {}

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
