// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

import org.chromium.base.TerminationStatus;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class receives callbacks that act as hooks for various a native web contents events related
 * to loading a url. A single web contents can have multiple WebContentObservers.
 */
@NullMarked
public abstract class WebContentsObserver {
    /**
     * Indicates this is a class controlling a {@link WebContents} that can be observed.
     *
     * <p>This exists primarily for testability / mocking above the //content layer. If that was not
     * a requirement, this class could internally cast to WebContentsImpl.
     */
    public interface Observable {
        /**
         * Add an observer to the WebContents
         *
         * @param observer The observer to add.
         */
        void addObserver(WebContentsObserver observer);

        /**
         * Remove an observer from the WebContents
         *
         * @param observer The observer to remove.
         */
        void removeObserver(WebContentsObserver observer);
    }

    private @Nullable WebContents mWebContents;

    public WebContentsObserver(@Nullable WebContents webContents) {
        observe(webContents);
    }

    /** Return the web contents associated with the observer. */
    @Nullable
    public WebContents getWebContents() {
        return mWebContents;
    }

    /**
     * Called when a RenderFrame for renderFrameHost is created in the renderer process. To avoid
     * creating a RenderFrameHost object without necessity, only its id is passed. Call
     * WebContents#getRenderFrameHostFromId() to get the RenderFrameHost object if needed.
     */
    public void renderFrameCreated(GlobalRenderFrameHostId id) {}

    /** Called when a RenderFrame for renderFrameHost is deleted in the renderer process. */
    public void renderFrameDeleted(GlobalRenderFrameHostId id) {}

    /**
     * Called when a new Page has been committed as the primary page.
     *
     * @param page The Page that is now the primary page.
     */
    public void primaryPageChanged(Page page) {}

    public void primaryMainFrameRenderProcessGone(@TerminationStatus int terminationStatus) {}

    /**
     * Called when the browser process starts a navigation in the primary main frame.
     *
     * @param navigationHandle NavigationHandle are provided to several WebContentsObserver methods
     *     to allow observers to track specific navigations. Observers should clear any references
     *     to a NavigationHandle at didFinishNavigationInPrimaryMainFrame();
     */
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {}

    /**
     * Called when the browser process redirect a navigation.
     * @param navigationHandle
     *        NavigationHandle are proided to several WebContentsObserver methods to allow
     *        observers to track specific navigations. Observers should clear any references to a
     *        NavigationHandle at didFinishNavigationInPrimaryMainFrame();
     */
    public void didRedirectNavigation(NavigationHandle navigationHandle) {}

    /**
     * Called when the current navigation on the primary main frame is finished. This happens when a
     * navigation is committed, aborted or replaced by a new one.
     * @param navigationHandle
     *        NavigationHandle are provided to several WebContentsObserver methods to allow
     *        observers to track specific navigations. Observers should clear any references to a
     *        NavigationHandle at the end of this function.
     */
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {}

    /**
     * Called when the a page starts loading.
     * @param url The validated url for the loading page.
     */
    public void didStartLoading(GURL url) {}

    /**
     * Called when the a page finishes loading.
     * @param url The url for the page.
     * @param isKnownValid Whether the url is known to be valid.
     * TODO(yfriedman): There's currently a layering violation and this is needed for aw/
     *     For chrome, the url will always be valid.
     *
     */
    public void didStopLoading(GURL url, boolean isKnownValid) {}

    /**
     * Called when a page's load progress has changed.
     * @param progress The load progress in the range of [0,1].
     */
    public void loadProgressChanged(float progress) {}

    /** Called when a page's visible security state has changed. */
    public void didChangeVisibleSecurityState() {}

    /**
     * Called when an error occurs while loading a document that fails to load.
     *
     * @param isInPrimaryMainFrame Whether the navigation occurred in the primary main frame.
     * @param errorCode Error code for the occurring error.
     * @param failingUrl The url that was loading when the error occurred.
     * @param frameLifecycleState The lifecycle state of the associated RenderFrameHost.
     */
    public void didFailLoad(
            boolean isInPrimaryMainFrame,
            int errorCode,
            GURL failingUrl,
            @LifecycleState int rfhLifecycleState) {}

    /** Called when the page had painted something non-empty. */
    public void didFirstVisuallyNonEmptyPaint() {}

    /** The web contents visibility changed. */
    public void onVisibilityChanged(@Visibility int visibility) {}

    /**
     * Title was set.
     *
     * @param title The updated title.
     */
    public void titleWasSet(String title) {}

    /** Called once the window.document object of the main frame was created. */
    public void primaryMainDocumentElementAvailable() {}

    /**
     * Notifies that a load has finished for the primary main frame.
     *
     * @param page The Page that has finished loading.
     * @param rfhId Identifier of the navigating frame.
     * @param url The validated URL that is being navigated to.
     * @param isKnownValid Whether the URL is known to be valid.
     * @param rfhLifecycleState The lifecycle state of the associated frame.
     */
    public void didFinishLoadInPrimaryMainFrame(
            Page page,
            GlobalRenderFrameHostId rfhId,
            GURL url,
            boolean isKnownValid,
            @LifecycleState int rfhLifecycleState) {}

    /**
     * Notifies that the document has finished loading for the primary main frame.
     *
     * @param page The Page whose document has finished loading.
     * @param rfhId Identifier of the navigating frame.
     * @param rfhLifecycleState The lifecycle state of the associated frame.
     */
    public void documentLoadedInPrimaryMainFrame(
            Page page, GlobalRenderFrameHostId rfhId, @LifecycleState int rfhLifecycleState) {}

    /**
     * Notifies that the first contentful paint happened on the primary main frame.
     *
     * @param page The Page where the first contentful paint happened.
     * @param durationUs The time taken for the first contentful paint to occur from navigation
     *     start in microseconds (μs).
     */
    public void firstContentfulPaintInPrimaryMainFrame(Page page, long durationUs) {}

    /**
     * Notifies that a navigation entry has been committed.
     *
     * @param details Details of committed navigation entry.
     */
    public void navigationEntryCommitted(LoadCommittedDetails details) {}

    /** Called when navigation entries were removed. */
    public void navigationEntriesDeleted() {}

    /** Called when navigation entries were changed. */
    public void navigationEntriesChanged() {}

    /** Called when a frame receives user activation. */
    public void frameReceivedUserActivation() {}

    /** Called when the theme color was changed. */
    public void didChangeThemeColor() {}

    /** Called when the background color was changed. */
    public void onBackgroundColorChanged() {}

    /**
     * Called when media started playing.
     *
     * <p>There may be multiple media elements in a single {@code Webcontents}, each of which has a
     * unique session id. The id can be used to keep track of independent sessions from the same
     * page.
     *
     * <p>See also: {@code WebContentsObserver::MediaPlayerInfo} in {@code web_contents_observer.h}.
     *
     * @param id a session id, also passed to {@code mediaStoppedPlaying()} when the session stops.
     * @param hasAudio whether the session has audio.
     * @param hasVideo whether the session has video.
     */
    public void mediaStartedPlaying(int id, boolean hasAudio, boolean hasVideo) {}

    /**
     * Called when media stopped playing.
     *
     * @param id the session id that was passed to {@code mediaStartedPlaying()} for this session
     *     when playback started.
     */
    public void mediaStoppedPlaying(int id) {}

    /**
     * Called when Media in the Web Contents leaves or enters fullscreen mode.
     *
     * @param isFullscreen whether fullscreen is being entered or left.
     */
    public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {}

    /**
     * Called when the Web Contents is toggled into or out of fullscreen mode by the renderer.
     *
     * @param enteredFullscreen whether fullscreen is being entered or left.
     * @param willCauseResize whether the change to fullscreen will cause the contents to resize.
     */
    public void didToggleFullscreenModeForTab(boolean enteredFullscreen, boolean willCauseResize) {}

    /**
     * The Viewport Fit Type passed to viewportFitChanged. This is mirrored
     * in an enum in display_cutout.mojom.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewportFit.AUTO, ViewportFit.CONTAIN, ViewportFit.COVER})
    public @interface ViewportFitType {}

    /**
     * Called when the viewport fit of the Web Contents changes.
     *
     * @param value the new viewport fit value.
     */
    public void viewportFitChanged(@ViewportFitType int value) {}

    /**
     * Called when the safe area constraint of the Web Contents changes.
     *
     * @param hasConstraint Whether there are safe area constraint.
     */
    public void safeAreaConstraintChanged(boolean hasConstraint) {}

    /**
     * Called when the virtual keyboard mode of the Web Contents changes.
     *
     * @param mode the new virtual keyboard mode.
     */
    public void virtualKeyboardModeChanged(@VirtualKeyboardMode.EnumType int mode) {}

    /** This method is invoked when a RenderWidgetHost for a WebContents gains focus. */
    public void onWebContentsFocused() {}

    /**
     * This method is invoked when a RenderWidgetHost for a WebContents loses focus. This may be
     * immediately followed by onWebContentsFocused if focus was moving between two
     * RenderWidgetHosts within the same WebContents.
     */
    public void onWebContentsLostFocus() {}

    /** Called when the top level WindowAndroid changes. */
    public void onTopLevelNativeWindowChanged(@Nullable WindowAndroid windowAndroid) {}

    /** Called when a MediaSession is created for the WebContents. */
    public void mediaSessionCreated(MediaSession mediaSession) {}

    /** Called when the WebContents is discarded. */
    public void wasDiscarded() {}

    /**
     * Called when {@link #getWebContents()} is being destroyed.
     *
     * <p>After this call, clients should assume that {@link #getWebContents()} will be imminently
     * destroyed and the C++ counterpart deleted.
     */
    public void webContentsDestroyed() {}

    /**
     * Updates the {@link WebContents} that this class is observing, and if null, stops observing
     * any updates.
     *
     * @param webContents The WebContents to observe (or null to stop observing).
     */
    public final void observe(@Nullable WebContents webContents) {
        if (mWebContents == webContents) return;
        if (mWebContents != null) ((Observable) mWebContents).removeObserver(this);
        mWebContents = webContents;
        if (mWebContents != null) {
            ((Observable) mWebContents).addObserver(this);
        }
    }

    protected WebContentsObserver() {}
}
