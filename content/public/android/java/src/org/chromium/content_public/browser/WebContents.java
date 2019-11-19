// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Rect;
import android.os.Handler;
import android.os.Parcelable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * The WebContents Java wrapper to allow communicating with the native WebContents object.
 *
 * Note about serialization and {@link Parcelable}:
 *   This object is serializable and deserializable as long as it is done in the same process.  That
 * means it can be passed between Activities inside this process, but not preserved beyond the
 * process lifetime.  This class will automatically deserialize into {@code null} if a deserialize
 * attempt happens in another process.
 *
 * To properly deserialize a custom Parcelable the right class loader must be used.  See below for
 * some examples.
 *
 * Intent Serialization/Deserialization Example:
 * intent.putExtra("WEBCONTENTSKEY", webContents);
 * // ... send to other location ...
 * intent.setExtrasClassLoader(WebContents.class.getClassLoader());
 * webContents = intent.getParcelableExtra("WEBCONTENTSKEY");
 *
 * Bundle Serialization/Deserialization Example:
 * bundle.putParcelable("WEBCONTENTSKEY", webContents);
 * // ... send to other location ...
 * bundle.setClassLoader(WebContents.class.getClassLoader());
 * webContents = bundle.get("WEBCONTENTSKEY");
 */
public interface WebContents extends Parcelable {
    /**
     * Interface used to transfer the internal objects (but callers should own) from WebContents.
     */
    interface InternalsHolder {
        /**
         * Called when WebContents sets the internals to the caller.
         *
         * @param internals a {@link WebContentsInternals} object.
         */
        void set(WebContentsInternals internals);

        /**
         * Returns {@link WebContentsInternals} object. Can be {@code null}.
         */
        WebContentsInternals get();
    }

    /**
     * @return a default implementation of {@link InternalsHolder} that holds a reference to
     * {@link WebContentsInternals} object owned by {@link WebContents} instance.
     */
    public static InternalsHolder createDefaultInternalsHolder() {
        return new InternalsHolder() {
            private WebContentsInternals mInternals;

            @Override
            public void set(WebContentsInternals internals) {
                mInternals = internals;
            }

            @Override
            public WebContentsInternals get() {
                return mInternals;
            }
        };
    }

    /**
     * Initialize various content objects of {@link WebContents} lifetime.
     * @param productVersion Product version for accessibility.
     * @param viewDelegate Delegate to add/remove anchor views.
     * @param accessDelegate Handles dispatching all hidden or super methods to the containerView.
     * @param windowAndroid An instance of the WindowAndroid.
     * @param internalsHolder A holder of objects used internally by WebContents.
     */
    void initialize(String productVersion, ViewAndroidDelegate viewDelegate,
            ViewEventSink.InternalAccessDelegate accessDelegate, WindowAndroid windowAndroid,
            @NonNull InternalsHolder internalsHolder);

    /**
     * @return The top level WindowAndroid associated with this WebContents.  This can be null.
     */
    @Nullable
    WindowAndroid getTopLevelNativeWindow();

    /*
     * Updates the native {@link WebContents} with a new window. This moves the NativeView and
     * attached it to the new NativeWindow linked with the given {@link WindowAndroid}.
     * TODO(jinsukkim): This should happen through view android tree instead.
     * @param windowAndroid The new {@link WindowAndroid} for this {@link WebContents}.
     */
    void setTopLevelNativeWindow(WindowAndroid windowAndroid);

    /**
     * @return The {@link ViewAndroidDelegate} from which to get the container view.
     *         This can be null.
     */
    @Nullable
    ViewAndroidDelegate getViewAndroidDelegate();

    /**
     * Deletes the Web Contents object.
     */
    void destroy();

    /**
     * @return Whether or not the native object associated with this WebContent is destroyed.
     */
    boolean isDestroyed();

    /**
     * Removes the native WebContents' reference to this object. This is used when we want to
     * destroy this object without destroying its native counterpart.
     */
    void clearNativeReference();

    /**
     * @return The navigation controller associated with this WebContents.
     */
    NavigationController getNavigationController();

    /**
     * @return The main frame associated with this WebContents.
     */
    RenderFrameHost getMainFrame();

    /**
     * @return The focused frame associated with this WebContents. Will be null if the WebContents
     * does not have focus.
     */
    @Nullable
    RenderFrameHost getFocusedFrame();

    /**
     * @return The root level view from the renderer, or {@code null} in some cases where there is
     *         none.
     */
    @Nullable
    RenderWidgetHostView getRenderWidgetHostView();

    /**
     * @return The title for the current visible page.
     */
    String getTitle();

    /**
     * @return The URL for the current visible page.
     */
    String getVisibleUrl();

    /**
     * @return The character encoding for the current visible page.
     */
    String getEncoding();

    /**
     * @return Whether this WebContents is loading a resource.
     */
    boolean isLoading();

    /**
     * @return Whether this WebContents is loading and and the load is to a different top-level
     *         document (rather than being a navigation within the same document).
     */
    boolean isLoadingToDifferentDocument();

    /**
     * Stop any pending navigation.
     */
    void stop();

    /**
     * To be called when the ContentView is hidden.
     */
    void onHide();

    /**
     * To be called when the ContentView is shown.
     */
    void onShow();

    /**
     * ChildProcessImportance on Android allows controls of the renderer process bindings
     * independent of visibility. Note this does not affect importance of subframe processes.
     * @param mainFrameImportance importance of the main frame process.
     */
    void setImportance(@ChildProcessImportance int mainFrameImportance);

    /**
     * Suspends all media players for this WebContents.  Note: There may still
     * be activities generating audio, so setAudioMuted() should also be called
     * to ensure all audible activity is silenced.
     */
    void suspendAllMediaPlayers();

    /**
     * Sets whether all audio output from this WebContents is muted.
     *
     * @param mute Set to true to mute the WebContents, false to unmute.
     */
    void setAudioMuted(boolean mute);

    /**
     * @return Whether the page is currently showing an interstitial, such as a bad HTTPS page.
     */
    boolean isShowingInterstitialPage();

    /**
     * @return Whether the location bar should be focused by default for this page.
     */
    boolean focusLocationBarByDefault();


     /**
     * Inform WebKit that Fullscreen mode has been exited by the user.
     */
    void exitFullscreen();

    /**
     * Brings the Editable to the visible area while IME is up to make easier for inputing text.
     */
    void scrollFocusedEditableNodeIntoView();

    /**
     * Selects the word around the caret, if any.
     * The caller can check if selection actually occurred by listening to OnSelectionChanged.
     */
    void selectWordAroundCaret();

    /**
     * Adjusts the selection starting and ending points by the given amount.
     * A negative amount moves the selection towards the beginning of the document, a positive
     * amount moves the selection towards the end of the document.
     * @param startAdjust The amount to adjust the start of the selection.
     * @param endAdjust The amount to adjust the end of the selection.
     * @param showSelectionMenu if true, show selection menu after adjustment.
     */
    void adjustSelectionByCharacterOffset(
            int startAdjust, int endAdjust, boolean showSelectionMenu);

    /**
     * Gets the last committed URL. It represents the current page that is
     * displayed in this WebContents. It represents the current security context.
     *
     * @return The last committed URL.
     */
    String getLastCommittedUrl();

    /**
     * Get the InCognito state of WebContents.
     *
     * @return whether this WebContents is in InCognito mode or not
     */
    boolean isIncognito();

    /**
     * Resumes the requests for a newly created window.
     */
    void resumeLoadingCreatedWebContents();

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     *
     * It is not possible to use this method to evaluate JavaScript on web
     * content, only on WebUI pages.
     *
     * @param script The Javascript to execute.
     * @param callback The callback to be fired off when a result is ready. The script's
     *                 result will be json encoded and passed as the parameter, and the call
     *                 will be made on the main thread.
     *                 If no result is required, pass null.
     */
    void evaluateJavaScript(String script, @Nullable JavaScriptCallback callback);

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     *
     * @param script The Javascript to execute.
     * @param callback The callback to be fired off when a result is ready. The script's
     *                 result will be json encoded and passed as the parameter, and the call
     *                 will be made on the main thread.
     *                 If no result is required, pass null.
     */
    @VisibleForTesting
    void evaluateJavaScriptForTests(String script, @Nullable JavaScriptCallback callback);

    /**
     * Adds a log message to dev tools console. |level| must be a value of
     * org.chromium.content_public.common.ConsoleMessageLevel.
     */
    void addMessageToDevToolsConsole(int level, String message);

    /**
     * Post a message to main frame.
     *
     * @param message   The message
     * @param targetOrigin  The target origin. If the target origin is a "*" or a
     *                  empty string, it indicates a wildcard target origin.
     * @param ports The sent message ports, if any. Pass null if there is no
     *                  message ports to pass.
     */
    void postMessageToMainFrame(String message, String sourceOrigin, String targetOrigin,
            @Nullable MessagePort[] ports);

    /**
     * Creates a message channel for sending postMessage requests and returns the ports for
     * each end of the channel.
     * @return The ports that forms the ends of the message channel created.
     */
    MessagePort[] createMessageChannel();

    /**
     * Returns whether the initial empty page has been accessed by a script from another
     * page. Always false after the first commit.
     *
     * @return Whether the initial empty page has been accessed by a script.
     */
    boolean hasAccessedInitialDocument();

    /**
     * This returns the theme color as set by the theme-color meta tag.
     * <p>
     * The color returned may retain non-fully opaque alpha components.  A value of
     * {@link android.graphics.Color#TRANSPARENT} means there was no theme color specified.
     *
     * @return The theme color for the content as set by the theme-color meta tag.
     */
    int getThemeColor();

    /**
     * @return Current page load progress on a scale of 0 to 1.
     */
    float getLoadProgress();

    /**
     * Initiate extraction of text, HTML, and other information for clipping puposes (smart clip)
     * from the rectangle area defined by starting positions (x and y), and width and height.
     */
    void requestSmartClipExtract(int x, int y, int width, int height);

    /**
     * Register a handler to handle smart clip data once extraction is done.
     */
    void setSmartClipResultHandler(final Handler smartClipHandler);

    /**
     * Requests a snapshop of accessibility tree. The result is provided asynchronously
     * using the callback
     * @param callback The callback to be called when the snapshot is ready. The callback
     *                 cannot be null.
     */
    void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback);

    /**
     * Returns {@link EventForwarder} which is used to forward input/view events
     * to native content layer.
     */
    EventForwarder getEventForwarder();

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

    /**
     * Sets a handler to handle swipe to refresh events.
     *
     * @param handler The handler to install.
     */
    void setOverscrollRefreshHandler(OverscrollRefreshHandler handler);

    /**
     * Controls use of spatial-navigation mode.
     * @param disable True if spatial navigation should never be used.
     */
    void setSpatialNavigationDisabled(boolean disabled);

    /**
     * Sends a request to download the given image {@link url}.
     * This method delegates the call to the downloadImage() method of native WebContents.
     * @param url The URL of the image to download.
     * @param isFavicon Whether the image is a favicon. If true, the cookies are not sent and not
     *                 accepted during download.
     * @param maxBitmapSize The maximum bitmap size. Bitmaps with pixel sizes larger than {@link
     *                 max_bitmap_size} are filtered out from the bitmap results. If there are no
     *                 bitmap results <= {@link max_bitmap_size}, the smallest bitmap is resized to
     *                 {@link max_bitmap_size} and is the only result. A {@link max_bitmap_size} of
     *                 0 means unlimited.
     * @param bypassCache If true, {@link url} is requested from the server even if it is present in
     *                 the browser cache.
     * @param callback The callback which will be called when the bitmaps are received from the
     *                 renderer.
     * @return The unique id of the download request
     */
    int downloadImage(String url, boolean isFavicon, int maxBitmapSize, boolean bypassCache,
            ImageDownloadCallback callback);

    /**
     * Whether the WebContents has an active fullscreen video with native or custom controls.
     * The WebContents must be fullscreen when this method is called. Fullscreen videos may take a
     * moment to register.
     */
    boolean hasActiveEffectivelyFullscreenVideo();

    /**
     * Whether the WebContents is allowed to enter Picture-in-Picture when it has an active
     * fullscreen video with native or custom controls.
     */
    boolean isPictureInPictureAllowedForFullscreenVideo();

    /**
     * Gets a Rect containing the size of the currently playing fullscreen video. The position of
     * the rectangle is meaningless. Will return null if there is no such video. Fullscreen videos
     * may take a moment to register.
     */
    @Nullable
    Rect getFullscreenVideoSize();

    /**
     * Notifies the WebContents about the new persistent video status. It should be called whenever
     * the value changes.
     *
     * @param value Whether there is a persistent video associated with this WebContents.
     */
    void setHasPersistentVideo(boolean value);

    /**
     * Set the view size of the WebContents. The size is in physical pixels.
     *
     * @param width The width of the view.
     * @param height The height of the view.
     */
    void setSize(int width, int height);

    /**
     * Gets the view size width of the WebContents.
     *
     * @return The width of the view in dip.
     */
    int getWidth();

    /**
     * Gets the view size width of the WebContents.
     *
     * @return The width of the view in dip.
     */
    int getHeight();

    /**
     * Sets the Display Cutout safe area of the WebContents. These are insets from each edge
     * in physical pixels
     *
     * @param insets The insets stored in a Rect.
     */
    void setDisplayCutoutSafeArea(Rect insets);

    /**
     * Notify that web preferences needs update for various properties.
     */
    void notifyRendererPreferenceUpdate();
}
