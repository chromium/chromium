// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Parcelable;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.blink_public.input.SelectionGranularity;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.BrowserControlsOffsetTagDefinitions;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

/**
 * The WebContents Java wrapper to allow communicating with the native WebContents object.
 *
 * <p>Note about serialization and {@link Parcelable}: This object is serializable and
 * deserializable as long as it is done in the same process. That means it can be passed between
 * Activities inside this process, but not preserved beyond the process lifetime. This class will
 * automatically deserialize into {@code null} if a deserialize attempt happens in another process.
 *
 * <p>To properly deserialize a custom Parcelable the right class loader must be used. See below for
 * some examples.
 *
 * <p>Intent Serialization/Deserialization Example: intent.putExtra("WEBCONTENTSKEY", webContents);
 * // ... send to other location ...
 * intent.setExtrasClassLoader(WebContents.class.getClassLoader()); webContents =
 * intent.getParcelableExtra("WEBCONTENTSKEY");
 *
 * <p>Bundle Serialization/Deserialization Example: bundle.putParcelable("WEBCONTENTSKEY",
 * webContents); // ... send to other location ...
 * bundle.setClassLoader(WebContents.class.getClassLoader()); webContents =
 * bundle.get("WEBCONTENTSKEY");
 */
@NullMarked
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
        void set(@Nullable WebContentsInternals internals);

        /** Returns {@link WebContentsInternals} object. Can be {@code null}. */
        @Nullable WebContentsInternals get();
    }

    /**
     * @return a default implementation of {@link InternalsHolder} that holds a reference to {@link
     *     WebContentsInternals} object owned by {@link WebContents} instance.
     */
    static InternalsHolder createDefaultInternalsHolder() {
        return new InternalsHolder() {
            private @Nullable WebContentsInternals mInternals;

            @Override
            public void set(@Nullable WebContentsInternals internals) {
                mInternals = internals;
            }

            @Override
            public @Nullable WebContentsInternals get() {
                return mInternals;
            }
        };
    }

    /**
     * Initialize various content objects of {@link WebContents} lifetime.
     *
     * <p>Note: This method is more of to set the {@link ViewAndroidDelegate} and {@link
     * ViewEventSink.InternalAccessDelegate}, most of the embedder should only call this once during
     * the whole lifecycle of the {@link WebContents}, but it is safe to call it multiple times.
     *
     * @param productVersion Product version for accessibility.
     * @param viewDelegate Delegate to add/remove anchor views.
     * @param accessDelegate Handles dispatching all hidden or super methods to the containerView.
     * @param windowAndroid An instance of the WindowAndroid.
     * @param internalsHolder A holder of objects used internally by WebContents.
     */
    void setDelegates(
            String productVersion,
            ViewAndroidDelegate viewDelegate,
            ViewEventSink.@Nullable InternalAccessDelegate accessDelegate,
            @Nullable WindowAndroid windowAndroid,
            InternalsHolder internalsHolder);

    /**
     * Clear Java WebContentsObservers so we can put this WebContents to the background. Use this
     * method only when the WebContents will not be destroyed shortly. Currently only used by Chrome
     * for swapping WebContents in Tab.
     *
     * Note: This is a temporary workaround for Chrome until it can clean up Observers directly.
     * Avoid new calls to this method.
     */
    void clearJavaWebContentsObservers();

    /**
     * @return The top level WindowAndroid associated with this WebContents. This can be null.
     */
    @Nullable WindowAndroid getTopLevelNativeWindow();

    /**
     * Updates the native {@link WebContents} with a new window. This moves the NativeView and
     * attached it to the new NativeWindow linked with the given {@link WindowAndroid}.
     * TODO(jinsukkim): This should happen through view android tree instead.
     *
     * @param windowAndroid The new {@link WindowAndroid} for this {@link WebContents}.
     */
    void setTopLevelNativeWindow(@Nullable WindowAndroid windowAndroid);

    /**
     * If called too early, the {@link ViewAndroidDelegate} might not be yet available. One can
     * subscribe to `ViewAndroidObserver::OnDelegateSet` to be notified when the {@link
     * ViewAndroidDelegate} becomes available/changes.
     *
     * @return The {@link ViewAndroidDelegate} from which to get the container view. This can be
     *     null.
     */
    @Nullable ViewAndroidDelegate getViewAndroidDelegate();

    /** Deletes the Web Contents object. */
    void destroy();

    /**
     * @return Whether or not the native object associated with this WebContent is destroyed.
     */
    boolean isDestroyed();

    /**
     * Removes the native WebContents' reference to this object. This is used when we want to
     * destroy this object without destroying its native counterpart.
     */
    @Deprecated
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
    @Nullable RenderFrameHost getFocusedFrame();

    /**
     * @return Whether the focused frame element in this WebContents is editable. Will be false if
     *         the WebContents does not have focus.
     */
    boolean isFocusedElementEditable();

    /**
     * @return The frame associated with the id. Will be null if the ID does not correspond to a
     *         live RenderFrameHost.
     */
    @Nullable RenderFrameHost getRenderFrameHostFromId(GlobalRenderFrameHostId id);

    /**
     * @return The root level view from the renderer, or {@code null} in some cases where there is
     *     none.
     */
    @Nullable RenderWidgetHostView getRenderWidgetHostView();

    /**
     * @return The WebContents Visibility. See native WebContents::GetVisibility.
     */
    @Visibility
    int getVisibility();

    /**
     * Updates WebContents Visibility and notifies all the observers about Visibility change event.
     * See native WebContents::UpdateWebContentsVisibility.
     */
    void updateWebContentsVisibility(@Visibility int visibility);

    /**
     * @return The title for the current visible page.
     */
    String getTitle();

    /**
     * @return The URL for the current visible page.
     */
    GURL getVisibleUrl();

    /**
     * @return The virtual keyboard mode of the WebContents' current primary page.
     */
    @VirtualKeyboardMode.EnumType
    int getVirtualKeyboardMode();

    /**
     * @return The character encoding for the current visible page.
     */
    String getEncoding();

    /**
     * Discards the RenderFrameHost associated with this WebContents.
     *
     * @param onDiscarded a callback to be called when the RenderFrameHost is discarded. May never
     *     be called if the operation fails.
     *     <p>TODO(crbug.com/441841249): Change the runnable to a callback.
     */
    void discard(Runnable onDiscarded);

    /**
     * @return Whether this WebContents is loading a resource.
     */
    boolean isLoading();

    /**
     * @return Whether this WebContents is loading and expects any loading UI to be displayed.
     */
    boolean shouldShowLoadingUI();

    /**
     * Returns whether this WebContents's primary frame tree node is navigating, i.e. it has an
     * associated NavigationRequest.
     */
    boolean hasUncommittedNavigationInPrimaryMainFrame();

    /**
     * Runs the beforeunload handler, if any. The tab will be closed if there's no beforeunload
     * handler or if the user accepts closing.
     *
     * @param autoCancel See C++ WebContents for explanation.
     */
    void dispatchBeforeUnload(boolean autoCancel);

    /** Stop any pending navigation. */
    void stop();

    /**
     * ChildProcessImportance on Android allows controls of the renderer process bindings
     * independent of visibility. Note this does not affect importance of processes for non-primary
     * pages.
     *
     * <p>The subframeImportance must be less than or equal to the mainFrameImportance.
     *
     * @param mainFrameImportance importance of the primary page's main frame process.
     * @param subframeImportance importance of the primary page's subframes process.
     */
    void setPrimaryPageImportance(
            @ChildProcessImportance int mainFrameImportance,
            @ChildProcessImportance int subframeImportance);

    /**
     * Suspends all media players for this WebContents. Note: There may still be activities
     * generating audio, so setAudioMuted() should also be called to ensure all audible activity is
     * silenced.
     */
    void suspendAllMediaPlayers();

    /**
     * Sets whether all audio output from this WebContents is muted.
     *
     * @param mute Set to true to mute the WebContents, false to unmute.
     */
    void setAudioMuted(boolean mute);

    /**
     * @return Whether all audio output from this WebContents is muted.
     */
    boolean isAudioMuted();

    /**
     * @return Whether the location bar should be focused by default for this page.
     */
    boolean focusLocationBarByDefault();

    /**
     * Sets or removes page level focus.
     * @param hasFocus Indicates if focus should be set or removed.
     */
    void setFocus(boolean hasFocus);

    /**
     * @return true if the renderer is in fullscreen mode.
     */
    boolean isFullscreenForCurrentTab();

    /** Inform WebKit that Fullscreen mode has been exited by the user. */
    void exitFullscreen();

    /** Brings the Editable to the visible area while IME is up to make easier for inputing text. */
    void scrollFocusedEditableNodeIntoView();

    /**
     * Selects at the specified granularity around the caret and potentially shows the selection
     * handles and context menu. The caller can check if selection actually occurred by listening to
     * OnSelectionChanged.
     *
     * @param granularity The granularity at which the selection should happen.
     * @param shouldShowHandle Whether the selection handles should be shown after selection.
     * @param shouldShowContextMenu Whether the context menu should be shown after selection.
     * @param startOffset The start offset of the selection.
     * @param endOffset The end offset of the selection.
     * @param surroundingTextLength The length of the text surrounding the selection (including the
     *     selection).
     */
    void selectAroundCaret(
            @SelectionGranularity int granularity,
            boolean shouldShowHandle,
            boolean shouldShowContextMenu,
            int startOffset,
            int endOffset,
            int surroundingTextLength);

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
    GURL getLastCommittedUrl();

    /**
     * Get the InCognito state of WebContents.
     *
     * @return whether this WebContents is in InCognito mode or not
     */
    boolean isIncognito();

    /** Resumes the requests for a newly created window. */
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
    void evaluateJavaScriptForTests(String script, @Nullable JavaScriptCallback callback);

    /**
     * Adds a log message to dev tools console. |level| must be a value of
     * org.chromium.content_public.common.ConsoleMessageLevel.
     */
    void addMessageToDevToolsConsole(int level, String message);

    /**
     * Post a message to main frame.
     *
     * @param messagePayload   The message payload.
     * @param targetOrigin  The target origin. If the target origin is a "*" or a
     *                  empty string, it indicates a wildcard target origin.
     * @param ports The sent message ports, if any. Pass null if there is no
     *                  message ports to pass.
     */
    void postMessageToMainFrame(
            MessagePayload messagePayload,
            @Nullable String sourceOrigin,
            String targetOrigin,
            MessagePort @Nullable [] ports);

    /**
     * Creates a message channel for sending postMessage requests and returns the ports for
     * each end of the channel.
     * @return The ports that forms the ends of the message channel created.
     */
    MessagePort[] createMessageChannel();

    /**
     * Returns whether the initial empty page has been accessed by a script from another page.
     * Always false after the first commit.
     *
     * @return Whether the initial empty page has been accessed by a script.
     */
    boolean hasAccessedInitialDocument();

    /**
     * Returns whether the current page has opted into same-origin view transitions.
     *
     * @return Whether the current page has the same-origin view transition opt-in.
     */
    boolean hasViewTransitionOptIn();

    /**
     * This returns the theme color as set by the theme-color meta tag.
     *
     * <p>The color returned may retain non-fully opaque alpha components. A value of {@link
     * android.graphics.Color#TRANSPARENT} means there was no theme color specified.
     *
     * @return The theme color for the content as set by the theme-color meta tag.
     */
    int getThemeColor();

    /** This returns the background color for the web contents. */
    int getBackgroundColor();

    /**
     * @return Current page load progress on a scale of 0 to 1.
     */
    float getLoadProgress();

    /**
     * Initiate extraction of text, HTML, and other information for clipping puposes (smart clip)
     * from the rectangle area defined by starting positions (x and y), and width and height.
     */
    void requestSmartClipExtract(int x, int y, int width, int height);

    /** Register a handler to handle smart clip data once extraction is done. */
    void setSmartClipResultHandler(final Handler smartClipHandler);

    /**
     * Set the handler that provides stylus handwriting recognition.
     *
     * @param stylusWritingHandler the object that implements StylusWritingHandler interface.
     */
    void setStylusWritingHandler(@Nullable StylusWritingHandler stylusWritingHandler);

    /**
     * @return {@link StylusWritingImeCallback} which is used to implement the IME functionality for
     *     the Stylus handwriting feature.
     */
    @Nullable StylusWritingImeCallback getStylusWritingImeCallback();

    /**
     * Returns {@link EventForwarder} which is used to forward input/view events to native content
     * layer.
     */
    EventForwarder getEventForwarder();

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
    int downloadImage(
            GURL url,
            boolean isFavicon,
            int maxBitmapSize,
            boolean bypassCache,
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
    @Nullable Rect getFullscreenVideoSize();

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
     * Sets the Display Cutout safe area of the WebContents. These are insets from each edge in
     * physical pixels
     *
     * @param insets The insets stored in a Rect.
     */
    void setDisplayCutoutSafeArea(Rect insets);

    /**
     * Instructs the web contents to "show interest" in the Element corresponding to the provided
     * nodeID.
     *
     * @param nodeID The DOMNodeID of the element that should receive interest.
     */
    void showInterestInElement(int nodeID);

    /** Notify that web preferences needs update for various properties. */
    void notifyRendererPreferenceUpdate();

    /**
     * Notify that the browser controls heights have changed. Any change to the top controls height,
     * bottom controls height, top controls min-height, and bottom controls min-height will call
     * this. Min-height is the minimum visible height the controls can have.
     */
    void notifyBrowserControlsHeightChanged();

    /**
     * Called before the dialog overlay dismissing e.g. Activity.onUserLeaveHint. It's a signal to
     * cleanup the tasks depending on the overlay surface, because the surface destroy may happen
     * before SurfaceHolder.Callback2.surfaceDestroyed returns.
     */
    void tearDownDialogOverlays();

    /**
     * This function checks all frames in this WebContents (not just the main frame) and returns
     * true if at least one frame has either a beforeunload or an unload/pagehide/visibilitychange
     * handler.
     */
    boolean needToFireBeforeUnloadOrUnloadEvents();

    /**
     * For cases where the content for a navigation entry is being drawn by the embedder (instead of
     * the web page), this notifies when the embedder has rendered the UI at its final state. This
     * is only called if the WebContents is showing an invoke animation for back forward
     * transitions, see {@link
     * org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid#didBackForwardTransitionAnimationChange},
     * when the navigation entry showing embedder provided UI commits.
     */
    void onContentForNavigationEntryShown();

    /**
     * @return {@link AnimationStage} the current stage of back forward transition.
     */
    @AnimationStage
    int getCurrentBackForwardTransitionStage();

    /**
     * Let long press on links select the link text instead of triggering context menu. Disabled by
     * default i.e. the context menu gets triggered.
     *
     * @param enabled {@code true} to enabled the behavior.
     */
    void setLongPressLinkSelectText(boolean enabled);

    /**
     * Allow drag-drop of files such as an image to load and replace contents.
     *
     * @param enabled whether the behavior should be enabled.
     */
    void setCanAcceptLoadDrops(boolean enabled);

    boolean getCanAcceptLoadDropsForTesting();

    /**
     * Update the OffsetTagDefinitions. This could be because the controls' visibility constraints
     * have changed, which requires adding/removing the OffsetTags, or because the
     * OffsetTagConstraints have changed due to a change in the controls' scrollable height.
     */
    void updateOffsetTagDefinitions(BrowserControlsOffsetTagDefinitions offsetTagDefinitions);

    void captureContentAsBitmapForTesting(Callback<Bitmap> callback);

    void setSupportsForwardTransitionAnimation(boolean supports);

    /**
     * @return whether this WebContents has an opener (corresponding to window.opener in JavaScript)
     *     associated with it.
     */
    boolean hasOpener();

    /**
     * @return The opener WebContents if this WebContents is in Document Picture-in-Picture mode, or
     *     {@code null} otherwise.
     */
    @Nullable WebContents getDocumentPictureInPictureOpener();

    /**
     * Returns the window open disposition that was originally requested when this WebContents was
     * created or navigated to. This method provides the disposition specified by the opener of this
     * WebContents, indicating how the content was initially intended to be displayed (e.g., as a
     * new foreground tab, a background tab, a new window, a popup, etc.). This value is determined
     * at the point of creation, such as during a navigation that results in a new WebContents
     * (e.g., from a link click with `target="_blank"`, `window.open()`, or a browser-initiated
     * action).
     *
     * @return an integer constant representing the original window open disposition.
     */
    int getOriginalWindowOpenDisposition();

    /**
     * Updates the Window Controls Overlay rect for a PWA.
     *
     * @param rect The rect of the overlay.
     */
    void updateWindowControlsOverlay(Rect rect);

    /** Enables draggable region calculation in this WebContents' primary main frame. */
    void setSupportsDraggableRegions(boolean supportsDraggableRegions);

    /**
     * Factory interface passed to {@link #getOrSetUserData()} for instantiation of class as user
     * data.
     *
     * <p>Constructor method reference comes handy for class Foo to provide the factory. Use lazy
     * initialization to avoid having to generate too many anonymous references. <code>
     * public class Foo {
     *     static final class FoofactoryLazyHolder {
     *         private static final UserDataFactory<Foo> INSTANCE = Foo::new;
     *     }
     *     ....
     *
     *     webContents.getOrsetUserData(Foo.class, FooFactoryLazyHolder.INSTANCE);
     *
     *     ....
     * }
     * </code>
     *
     * @param <T> Class to instantiate.
     */
    interface UserDataFactory<T> {
        T create(WebContents webContents);
    }

    /**
     * Retrieves or stores a user data object for this WebContents.
     *
     * @param key Class instance of the object used as the key.
     * @param userDataFactory Factory that creates an object of the generic class. A new object is
     *     created if it hasn't been created and non-null factory is given.
     * @return The created or retrieved user data object. Can be null if the object was not created
     *     yet, or {@code userDataFactory} is null, or the internal data storage is already
     *     garbage-collected.
     */
    <T extends UserData> @Nullable T getOrSetUserData(
            Class<T> key, @Nullable UserDataFactory<T> userDataFactory);

    /**
     * Removes the UserData object associated with the given key for this WebContents.
     *
     * @param <T> The type of the user data object to remove.
     * @param key The class object representing the type of user data to remove. If no user data
     *     object of this type exists, this method has no effect.
     */
    <T extends UserData> void removeUserData(Class<T> key);
}
