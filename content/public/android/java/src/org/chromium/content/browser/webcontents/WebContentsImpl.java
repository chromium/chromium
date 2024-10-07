// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.Parcel;
import android.os.ParcelUuid;
import android.os.Parcelable;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.ViewStructure;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.TerminationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.blink_public.input.SelectionGranularity;
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.content.browser.AppWebMessagePort;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.RenderWidgetHostViewImpl;
import org.chromium.content.browser.ViewEventSinkImpl;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.accessibility.ViewStructureBuilder;
import org.chromium.content.browser.framehost.RenderFrameHostDelegate;
import org.chromium.content.browser.framehost.RenderFrameHostImpl;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.ImageDownloadCallback;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.ViewEventSink.InternalAccessDelegate;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsInternals;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.UUID;

/**
 * The WebContentsImpl Java wrapper to allow communicating with the native WebContentsImpl object.
 */
@JNINamespace("content")
public class WebContentsImpl implements WebContents, RenderFrameHostDelegate, WindowEventObserver {
    private static final String TAG = "WebContentsImpl";

    private static final String PARCEL_VERSION_KEY = "version";
    private static final String PARCEL_WEBCONTENTS_KEY = "webcontents";
    private static final String PARCEL_PROCESS_GUARD_KEY = "processguard";

    private static final long PARCELABLE_VERSION_ID = 0;
    // Non-final for testing purposes, so resetting of the UUID can happen.
    private static UUID sParcelableUUID = UUID.randomUUID();

    /**
     * Used to reset the internal tracking for whether or not a serialized {@link WebContents}
     * was created in this process or not.
     */
    public static void invalidateSerializedWebContentsForTesting() {
        sParcelableUUID = UUID.randomUUID();
    }

    /**
     * A {@link android.os.Parcelable.Creator} instance that is used to build {@link
     * WebContentsImpl} objects from a {@link Parcel}.
     */
    // TODO(crbug.com/40479664): Fix this properly.
    @SuppressLint("ParcelClassLoader")
    public static final Parcelable.Creator<WebContents> CREATOR =
            new Parcelable.Creator<WebContents>() {
                @Override
                public WebContents createFromParcel(Parcel source) {
                    Bundle bundle = source.readBundle();

                    // Check the version.
                    if (bundle.getLong(PARCEL_VERSION_KEY, -1) != 0) return null;

                    // Check that we're in the same process.
                    ParcelUuid parcelUuid = bundle.getParcelable(PARCEL_PROCESS_GUARD_KEY);
                    if (sParcelableUUID.compareTo(parcelUuid.getUuid()) != 0) return null;

                    // Attempt to retrieve the WebContents object from the native pointer.
                    return WebContentsImplJni.get()
                            .fromNativePtr(bundle.getLong(PARCEL_WEBCONTENTS_KEY));
                }

                @Override
                public WebContents[] newArray(int size) {
                    return new WebContents[size];
                }
            };

    /**
     * Factory interface passed to {@link #getOrSetUserData()} for instantiation of
     * class as user data.
     *
     * Constructor method reference comes handy for class Foo to provide the factory.
     * Use lazy initialization to avoid having to generate too many anonymous references.
     *
     * <code>
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
    public interface UserDataFactory<T> {
        T create(WebContents webContents);
    }

    // Note this list may be incomplete. Frames that never had to initialize java side would
    // not have an entry here. This is here mainly to keep the java RenderFrameHosts alive, since
    // native side generally cannot safely hold strong references to them.
    private final List<RenderFrameHostImpl> mFrames = new ArrayList<>();

    private long mNativeWebContentsAndroid;
    private NavigationController mNavigationController;

    // Lazily created proxy observer for handling all Java-based WebContentsObservers.
    private WebContentsObserverProxy mObserverProxy;

    // The media session for this WebContents. It is constructed by the native MediaSession and has
    // the same life time as native MediaSession.
    private MediaSessionImpl mMediaSession;

    class SmartClipCallback {
        public SmartClipCallback(final Handler smartClipHandler) {
            mHandler = smartClipHandler;
        }

        public void onSmartClipDataExtracted(String text, String html, Rect clipRect) {
            RenderCoordinatesImpl coordinateSpace = getRenderCoordinates();
            clipRect.offset(0, (int) coordinateSpace.getContentOffsetYPix());
            Bundle bundle = new Bundle();
            bundle.putString("url", getVisibleUrl().getSpec());
            bundle.putString("title", getTitle());
            bundle.putString("text", text);
            bundle.putString("html", html);
            bundle.putParcelable("rect", clipRect);

            Message msg = Message.obtain(mHandler, 0);
            msg.setData(bundle);
            msg.sendToTarget();
        }

        final Handler mHandler;
    }

    private SmartClipCallback mSmartClipCallback;

    private EventForwarder mEventForwarder;

    private StylusWritingHandler mStylusWritingHandler;

    // Cached copy of all positions and scales as reported by the renderer.
    private RenderCoordinatesImpl mRenderCoordinates;

    private InternalsHolder mInternalsHolder;

    private String mProductVersion;

    private boolean mInitialized;

    // Remember the stack for clearing native the native stack for debugging use after destroy.
    private Throwable mNativeDestroyThrowable;

    private ObserverList<Runnable> mTearDownDialogOverlaysHandlers;

    private static class WebContentsInternalsImpl implements WebContentsInternals {
        public UserDataHost userDataHost;
        public ViewAndroidDelegate viewAndroidDelegate;
    }

    private WebContentsImpl(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        assert nativeWebContentsAndroid != 0;
        mNativeWebContentsAndroid = nativeWebContentsAndroid;
        mNavigationController = navigationController;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static WebContentsImpl create(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        return new WebContentsImpl(nativeWebContentsAndroid, navigationController);
    }

    @Override
    public void setDelegates(
            String productVersion,
            ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate accessDelegate,
            WindowAndroid windowAndroid,
            InternalsHolder internalsHolder) {
        assert internalsHolder != null;

        mProductVersion = productVersion;

        WebContentsInternalsImpl internals;
        if (mInternalsHolder != null) {
            internals = (WebContentsInternalsImpl) mInternalsHolder.get();
        } else {
            internals = new WebContentsInternalsImpl();
            internals.userDataHost = new UserDataHost();
        }
        mInternalsHolder = internalsHolder;
        mInternalsHolder.set(internals);

        if (mRenderCoordinates == null) {
            mRenderCoordinates = new RenderCoordinatesImpl();
        }

        mInitialized = true;

        setViewAndroidDelegate(viewDelegate);
        setTopLevelNativeWindow(windowAndroid);

        if (accessDelegate == null) {
            accessDelegate = new EmptyInternalAccessDelegate();
        }
        ViewEventSinkImpl.from(this).setAccessDelegate(accessDelegate);

        if (windowAndroid != null) {
            getRenderCoordinates().setDeviceScaleFactor(windowAndroid.getDisplay().getDipScale());
        }

        // Create GestureListenerManagerImpl so it updates `mRenderCoordinates`.
        GestureListenerManagerImpl.fromWebContents(this);
    }

    @Override
    public void clearJavaWebContentsObservers() {
        // Clear all the Android specific observers.
        if (mObserverProxy != null) {
            mObserverProxy.destroy();
            mObserverProxy = null;
        }
    }

    @Nullable
    public Context getContext() {
        assert mInitialized;

        WindowAndroid window = getTopLevelNativeWindow();
        return window != null ? window.getContext().get() : null;
    }

    public String getProductVersion() {
        assert mInitialized;
        return mProductVersion;
    }

    @CalledByNative
    @VisibleForTesting
    void clearNativePtr() {
        mNativeDestroyThrowable = new RuntimeException("clearNativePtr");
        mNativeWebContentsAndroid = 0;
        mNavigationController = null;
        if (mObserverProxy != null) {
            mObserverProxy.destroy();
            mObserverProxy = null;
        }
    }

    // =================== RenderFrameHostDelegate overrides ===================
    @Override
    public void renderFrameCreated(RenderFrameHostImpl host) {
        assert !mFrames.contains(host);
        mFrames.add(host);
    }

    @Override
    public void renderFrameDeleted(RenderFrameHostImpl host) {
        assert mFrames.contains(host);
        mFrames.remove(host);
    }

    // ================= end RenderFrameHostDelegate overrides =================

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        // This is wrapped in a Bundle so that failed deserialization attempts don't corrupt the
        // overall Parcel.  If we failed a UUID or Version check and didn't read the rest of the
        // fields it would corrupt the serialized stream.
        Bundle data = new Bundle();
        data.putLong(PARCEL_VERSION_KEY, PARCELABLE_VERSION_ID);
        data.putParcelable(PARCEL_PROCESS_GUARD_KEY, new ParcelUuid(sParcelableUUID));
        data.putLong(PARCEL_WEBCONTENTS_KEY, mNativeWebContentsAndroid);
        dest.writeBundle(data);
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeWebContentsAndroid;
    }

    @Override
    public WindowAndroid getTopLevelNativeWindow() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getTopLevelNativeWindow(mNativeWebContentsAndroid);
    }

    @Override
    public void setTopLevelNativeWindow(WindowAndroid windowAndroid) {
        checkNotDestroyed();
        WebContentsImplJni.get().setTopLevelNativeWindow(mNativeWebContentsAndroid, windowAndroid);
        WindowEventObserverManager.from(this).onWindowAndroidChanged(windowAndroid);
        if (mObserverProxy != null) mObserverProxy.onTopLevelNativeWindowChanged(windowAndroid);
    }

    @Override
    public ViewAndroidDelegate getViewAndroidDelegate() {
        // TODO(crbug.com/343119998): Investigate why this can be null and possibly fix that.
        if (mInternalsHolder == null) return null;
        WebContentsInternals internals = mInternalsHolder.get();
        if (internals == null) return null;
        return ((WebContentsInternalsImpl) internals).viewAndroidDelegate;
    }

    private void setViewAndroidDelegate(ViewAndroidDelegate viewDelegate) {
        checkNotDestroyed();
        WebContentsInternals internals = mInternalsHolder.get();
        assert internals != null;
        WebContentsInternalsImpl impl = (WebContentsInternalsImpl) internals;
        impl.viewAndroidDelegate = viewDelegate;
        WebContentsImplJni.get().setViewAndroidDelegate(mNativeWebContentsAndroid, viewDelegate);
    }

    @Override
    public void destroy() {
        // Note that |WebContents.destroy| is not guaranteed to be invoked.
        // Any resource release relying on this method will likely be leaked.

        if (!ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("Attempting to destroy WebContents on non-UI thread");
        }

        if (mObserverProxy != null && mObserverProxy.isHandlingObserverCall()) {
            throw new RuntimeException(
                    "Attempting to destroy WebContents while handling observer calls");
        }

        if (mNativeWebContentsAndroid != 0) {
            WebContentsImplJni.get().destroyWebContents(mNativeWebContentsAndroid);
        }
    }

    @Override
    public boolean isDestroyed() {
        return mNativeWebContentsAndroid == 0
                || WebContentsImplJni.get().isBeingDestroyed(mNativeWebContentsAndroid);
    }

    @Override
    public void clearNativeReference() {
        if (mNativeWebContentsAndroid != 0) {
            WebContentsImplJni.get().clearNativeReference(mNativeWebContentsAndroid);
        }
    }

    @Override
    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    @Override
    public RenderFrameHost getMainFrame() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getMainFrame(mNativeWebContentsAndroid);
    }

    @Override
    public RenderFrameHost getFocusedFrame() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getFocusedFrame(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isFocusedElementEditable() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isFocusedElementEditable(mNativeWebContentsAndroid);
    }

    @Override
    public RenderFrameHost getRenderFrameHostFromId(GlobalRenderFrameHostId id) {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .getRenderFrameHostFromId(
                        mNativeWebContentsAndroid, id.childId(), id.frameRoutingId());
    }

    // The RenderFrameHosts that are every RenderFrameHost in this WebContents.
    // See C++'s WebContents::ForEachRenderFrameHost for details.
    public List<RenderFrameHost> getAllRenderFrameHosts() {
        checkNotDestroyed();
        RenderFrameHost[] frames =
                WebContentsImplJni.get().getAllRenderFrameHosts(mNativeWebContentsAndroid);
        return Collections.unmodifiableList(Arrays.asList(frames));
    }

    @CalledByNative
    private static RenderFrameHost[] createRenderFrameHostArray(int size) {
        return new RenderFrameHost[size];
    }

    @CalledByNative
    private static void addRenderFrameHostToArray(
            RenderFrameHost[] frames, int index, RenderFrameHost frame) {
        frames[index] = frame;
    }

    /**
     * Thrown by reportDanglingPtrToBrowserContext(), indicating that WebContentsImpl is deleted
     * after its BrowserContext.
     */
    private static class DanglingPointerException extends RuntimeException {
        DanglingPointerException(String msg, Throwable causedBy) {
            super(msg, causedBy);
        }
    }

    @CalledByNative
    private static void reportDanglingPtrToBrowserContext(Throwable creator) {
        JavaExceptionReporter.reportException(
                new DanglingPointerException(
                        "Dangling pointer to BrowserContext in WebContents", creator));
    }

    @Override
    public @Nullable RenderWidgetHostViewImpl getRenderWidgetHostView() {
        if (mNativeWebContentsAndroid == 0) return null;
        RenderWidgetHostViewImpl rwhvi =
                WebContentsImplJni.get().getRenderWidgetHostView(mNativeWebContentsAndroid);
        if (rwhvi == null || rwhvi.isDestroyed()) return null;

        return rwhvi;
    }

    @Override
    public @Visibility int getVisibility() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getVisibility(mNativeWebContentsAndroid);
    }

    @Override
    public void updateWebContentsVisibility(@Visibility int visibility) {
        checkNotDestroyed();
        if (visibility == Visibility.VISIBLE) {
            SelectionPopupControllerImpl controller = getSelectionPopupController();
            if (controller != null) controller.restoreSelectionPopupsIfNecessary();
        }
        if (visibility == Visibility.HIDDEN) {
            SelectionPopupControllerImpl controller = getSelectionPopupController();
            if (controller != null) controller.hidePopupsAndPreserveSelection();
        }
        WebContentsImplJni.get().updateWebContentsVisibility(mNativeWebContentsAndroid, visibility);
    }

    @Override
    public String getTitle() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getTitle(mNativeWebContentsAndroid);
    }

    @Override
    public GURL getVisibleUrl() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getVisibleURL(mNativeWebContentsAndroid);
    }

    @Override
    @VirtualKeyboardMode.EnumType
    public int getVirtualKeyboardMode() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getVirtualKeyboardMode(mNativeWebContentsAndroid);
    }

    @Override
    public String getEncoding() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getEncoding(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isLoading() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isLoading(mNativeWebContentsAndroid);
    }

    @Override
    public boolean shouldShowLoadingUI() {
        checkNotDestroyed();
        return WebContentsImplJni.get().shouldShowLoadingUI(mNativeWebContentsAndroid);
    }

    @Override
    public boolean hasUncommittedNavigationInPrimaryMainFrame() {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .hasUncommittedNavigationInPrimaryMainFrame(mNativeWebContentsAndroid);
    }

    @Override
    public void dispatchBeforeUnload(boolean autoCancel) {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().dispatchBeforeUnload(mNativeWebContentsAndroid, autoCancel);
    }

    @Override
    public void stop() {
        checkNotDestroyed();
        WebContentsImplJni.get().stop(mNativeWebContentsAndroid);
    }

    /** Cut the selected content. */
    public void cut() {
        checkNotDestroyed();
        WebContentsImplJni.get().cut(mNativeWebContentsAndroid);
    }

    /** Copy the selected content. */
    public void copy() {
        checkNotDestroyed();
        WebContentsImplJni.get().copy(mNativeWebContentsAndroid);
    }

    /** Paste content from the clipboard. */
    public void paste() {
        checkNotDestroyed();
        WebContentsImplJni.get().paste(mNativeWebContentsAndroid);
    }

    /** Paste content from the clipboard without format. */
    public void pasteAsPlainText() {
        checkNotDestroyed();
        WebContentsImplJni.get().pasteAsPlainText(mNativeWebContentsAndroid);
    }

    /** Replace the selected text with the {@code word}. */
    public void replace(String word) {
        checkNotDestroyed();
        WebContentsImplJni.get().replace(mNativeWebContentsAndroid, word);
    }

    /** Select all content. */
    public void selectAll() {
        checkNotDestroyed();
        WebContentsImplJni.get().selectAll(mNativeWebContentsAndroid);
    }

    /** Collapse the selection to the end of selection range. */
    public void collapseSelection() {
        // collapseSelection may get triggered when certain selection-related widgets
        // are destroyed. As the timing for such destruction is unpredictable,
        // safely guard against this case.
        if (isDestroyed()) return;
        WebContentsImplJni.get().collapseSelection(mNativeWebContentsAndroid);
    }

    private SelectionPopupControllerImpl getSelectionPopupController() {
        return SelectionPopupControllerImpl.fromWebContents(this);
    }

    @Override
    public void setImportance(@ChildProcessImportance int primaryMainFrameImportance) {
        checkNotDestroyed();
        WebContentsImplJni.get()
                .setImportance(mNativeWebContentsAndroid, primaryMainFrameImportance);
    }

    @Override
    public void suspendAllMediaPlayers() {
        checkNotDestroyed();
        WebContentsImplJni.get().suspendAllMediaPlayers(mNativeWebContentsAndroid);
    }

    @Override
    public void setAudioMuted(boolean mute) {
        checkNotDestroyed();
        WebContentsImplJni.get().setAudioMuted(mNativeWebContentsAndroid, mute);
    }

    @Override
    public boolean isAudioMuted() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isAudioMuted(mNativeWebContentsAndroid);
    }

    @Override
    public boolean focusLocationBarByDefault() {
        checkNotDestroyed();
        return WebContentsImplJni.get().focusLocationBarByDefault(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isFullscreenForCurrentTab() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isFullscreenForCurrentTab(mNativeWebContentsAndroid);
    }

    @Override
    public void exitFullscreen() {
        checkNotDestroyed();
        WebContentsImplJni.get().exitFullscreen(mNativeWebContentsAndroid);
    }

    @Override
    public void scrollFocusedEditableNodeIntoView() {
        checkNotDestroyed();
        // The native side keeps track of whether the zoom and scroll actually occurred. It is
        // more efficient to do it this way and sometimes fire an unnecessary message rather
        // than synchronize with the renderer and always have an additional message.
        WebContentsImplJni.get().scrollFocusedEditableNodeIntoView(mNativeWebContentsAndroid);
    }

    @Override
    public void selectAroundCaret(
            @SelectionGranularity int granularity,
            boolean shouldShowHandle,
            boolean shouldShowContextMenu,
            int startOffset,
            int endOffset,
            int surroundingTextLength) {
        checkNotDestroyed();
        WebContentsImplJni.get()
                .selectAroundCaret(
                        mNativeWebContentsAndroid,
                        granularity,
                        shouldShowHandle,
                        shouldShowContextMenu,
                        startOffset,
                        endOffset,
                        surroundingTextLength);
    }

    @Override
    public void adjustSelectionByCharacterOffset(
            int startAdjust, int endAdjust, boolean showSelectionMenu) {
        WebContentsImplJni.get()
                .adjustSelectionByCharacterOffset(
                        mNativeWebContentsAndroid, startAdjust, endAdjust, showSelectionMenu);
    }

    @Override
    public GURL getLastCommittedUrl() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getLastCommittedURL(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isIncognito() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isIncognito(mNativeWebContentsAndroid);
    }

    @Override
    public void resumeLoadingCreatedWebContents() {
        checkNotDestroyed();
        WebContentsImplJni.get().resumeLoadingCreatedWebContents(mNativeWebContentsAndroid);
    }

    @Override
    public void evaluateJavaScript(String script, JavaScriptCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (isDestroyed() || script == null) return;
        WebContentsImplJni.get().evaluateJavaScript(mNativeWebContentsAndroid, script, callback);
    }

    @Override
    public void evaluateJavaScriptForTests(String script, JavaScriptCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (script == null) return;
        checkNotDestroyed();
        WebContentsImplJni.get()
                .evaluateJavaScriptForTests(mNativeWebContentsAndroid, script, callback);
    }

    @Override
    public void addMessageToDevToolsConsole(int level, String message) {
        checkNotDestroyed();
        WebContentsImplJni.get()
                .addMessageToDevToolsConsole(mNativeWebContentsAndroid, level, message);
    }

    @Override
    public void postMessageToMainFrame(
            MessagePayload messagePayload,
            String sourceOrigin,
            String targetOrigin,
            MessagePort[] ports) {
        if (ports != null) {
            for (MessagePort port : ports) {
                if (port.isClosed() || port.isTransferred()) {
                    throw new IllegalStateException("Port is already closed or transferred");
                }
                if (port.isStarted()) {
                    throw new IllegalStateException("Port is already started");
                }
            }
        }
        // Treat "*" as a wildcard. Internally, a wildcard is a empty string.
        if (targetOrigin.equals("*")) {
            targetOrigin = "";
        }
        WebContentsImplJni.get()
                .postMessageToMainFrame(
                        mNativeWebContentsAndroid,
                        messagePayload,
                        sourceOrigin,
                        targetOrigin,
                        ports);
    }

    @Override
    public AppWebMessagePort[] createMessageChannel() throws IllegalStateException {
        return AppWebMessagePort.createPair();
    }

    @Override
    public boolean hasAccessedInitialDocument() {
        checkNotDestroyed();
        return WebContentsImplJni.get().hasAccessedInitialDocument(mNativeWebContentsAndroid);
    }

    @Override
    public boolean hasViewTransitionOptIn() {
        checkNotDestroyed();
        return WebContentsImplJni.get().hasViewTransitionOptIn(mNativeWebContentsAndroid);
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResult(String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    @Override
    public int getThemeColor() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getThemeColor(mNativeWebContentsAndroid);
    }

    @Override
    public int getBackgroundColor() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getBackgroundColor(mNativeWebContentsAndroid);
    }

    @Override
    public float getLoadProgress() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getLoadProgress(mNativeWebContentsAndroid);
    }

    @Override
    public void requestSmartClipExtract(int x, int y, int width, int height) {
        if (mSmartClipCallback == null) return;
        checkNotDestroyed();
        RenderCoordinatesImpl coordinateSpace = getRenderCoordinates();
        y = y - (int) coordinateSpace.getContentOffsetYPix();
        WebContentsImplJni.get()
                .requestSmartClipExtract(
                        mNativeWebContentsAndroid, mSmartClipCallback, x, y, width, height);
    }

    @Override
    public void setSmartClipResultHandler(final Handler smartClipHandler) {
        if (smartClipHandler == null) {
            mSmartClipCallback = null;
            return;
        }
        mSmartClipCallback = new SmartClipCallback(smartClipHandler);
    }

    @CalledByNative
    private static void onSmartClipDataExtracted(
            String text,
            String html,
            int left,
            int top,
            int right,
            int bottom,
            SmartClipCallback callback) {
        callback.onSmartClipDataExtracted(text, html, new Rect(left, top, right, bottom));
    }

    /**
     * Requests a snapshop of accessibility tree. The result is provided asynchronously
     * using the callback
     * @param callback The callback to be called when the snapshot is ready. The callback
     *                 cannot be null.
     */
    public void requestAccessibilitySnapshot(ViewStructure root, Runnable doneCallback) {
        checkNotDestroyed();
        ViewStructureBuilder builder = new ViewStructureBuilder(mRenderCoordinates);

        WebContentsImplJni.get()
                .requestAccessibilitySnapshot(
                        mNativeWebContentsAndroid, root, builder, doneCallback);
    }

    public void simulateRendererKilledForTesting() {
        if (mObserverProxy != null) {
            mObserverProxy.primaryMainFrameRenderProcessGone(TerminationStatus.PROCESS_WAS_KILLED);
        }
    }

    @Override
    public void setStylusWritingHandler(StylusWritingHandler stylusWritingHandler) {
        mStylusWritingHandler = stylusWritingHandler;
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get()
                .setStylusHandwritingEnabled(
                        mNativeWebContentsAndroid, mStylusWritingHandler != null);
    }

    @Override
    public StylusWritingImeCallback getStylusWritingImeCallback() {
        ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(this);
        if (imeAdapter == null) return null;
        return imeAdapter.getStylusWritingImeCallback();
    }

    public StylusWritingHandler getStylusWritingHandler() {
        return mStylusWritingHandler;
    }

    @Override
    public EventForwarder getEventForwarder() {
        assert mNativeWebContentsAndroid != 0;
        if (mEventForwarder == null) {
            checkNotDestroyed();
            mEventForwarder =
                    WebContentsImplJni.get().getOrCreateEventForwarder(mNativeWebContentsAndroid);
            mEventForwarder.setStylusWritingDelegate(
                    new EventForwarder.StylusWritingDelegate() {
                        @Override
                        public boolean handleTouchEvent(MotionEvent motionEvent) {
                            return mStylusWritingHandler != null
                                    && mStylusWritingHandler.handleTouchEvent(
                                            motionEvent,
                                            getViewAndroidDelegate().getContainerView());
                        }

                        @Override
                        public void handleHoverEvent(MotionEvent motionEvent) {
                            if (mStylusWritingHandler != null) {
                                mStylusWritingHandler.handleHoverEvent(
                                        motionEvent, getViewAndroidDelegate().getContainerView());
                            }
                        }
                    });
        }
        return mEventForwarder;
    }

    @Override
    public void addObserver(WebContentsObserver observer) {
        assert mNativeWebContentsAndroid != 0;
        if (mObserverProxy == null) mObserverProxy = new WebContentsObserverProxy(this);
        mObserverProxy.addObserver(observer);
    }

    @Override
    public void removeObserver(WebContentsObserver observer) {
        if (mObserverProxy == null) return;
        mObserverProxy.removeObserver(observer);
    }

    @Override
    public void setOverscrollRefreshHandler(OverscrollRefreshHandler handler) {
        checkNotDestroyed();
        WebContentsImplJni.get().setOverscrollRefreshHandler(mNativeWebContentsAndroid, handler);
    }

    @Override
    public void setSpatialNavigationDisabled(boolean disabled) {
        checkNotDestroyed();
        WebContentsImplJni.get().setSpatialNavigationDisabled(mNativeWebContentsAndroid, disabled);
    }

    @Override
    public int downloadImage(
            GURL url,
            boolean isFavicon,
            int maxBitmapSize,
            boolean bypassCache,
            ImageDownloadCallback callback) {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .downloadImage(
                        mNativeWebContentsAndroid,
                        url,
                        isFavicon,
                        maxBitmapSize,
                        bypassCache,
                        callback);
    }

    @CalledByNative
    private void onDownloadImageFinished(
            ImageDownloadCallback callback,
            int id,
            int httpStatusCode,
            GURL imageUrl,
            List<Bitmap> bitmaps,
            List<Rect> sizes) {
        callback.onFinishDownloadImage(id, httpStatusCode, imageUrl, bitmaps, sizes);
    }

    @Override
    public void setHasPersistentVideo(boolean value) {
        checkNotDestroyed();
        WebContentsImplJni.get().setHasPersistentVideo(mNativeWebContentsAndroid, value);
    }

    @Override
    public boolean hasActiveEffectivelyFullscreenVideo() {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .hasActiveEffectivelyFullscreenVideo(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isPictureInPictureAllowedForFullscreenVideo() {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .isPictureInPictureAllowedForFullscreenVideo(mNativeWebContentsAndroid);
    }

    @Override
    public @Nullable Rect getFullscreenVideoSize() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getFullscreenVideoSize(mNativeWebContentsAndroid);
    }

    @Override
    public void setSize(int width, int height) {
        checkNotDestroyed();
        WebContentsImplJni.get().setSize(mNativeWebContentsAndroid, width, height);
    }

    @Override
    public int getWidth() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getWidth(mNativeWebContentsAndroid);
    }

    @Override
    public int getHeight() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getHeight(mNativeWebContentsAndroid);
    }

    @CalledByNative
    private final void setMediaSession(MediaSessionImpl mediaSession) {
        mMediaSession = mediaSession;
    }

    @CalledByNative
    private static List<Bitmap> createBitmapList() {
        return new ArrayList<Bitmap>();
    }

    @CalledByNative
    private static void addToBitmapList(List<Bitmap> bitmaps, Bitmap bitmap) {
        bitmaps.add(bitmap);
    }

    @CalledByNative
    private static List<Rect> createSizeList() {
        return new ArrayList<Rect>();
    }

    @CalledByNative
    private static void createSizeAndAddToList(List<Rect> sizes, int width, int height) {
        sizes.add(new Rect(0, 0, width, height));
    }

    @CalledByNative
    private static Rect createSize(int width, int height) {
        return new Rect(0, 0, width, height);
    }

    /** Returns {@link RenderCoordinatesImpl}. */
    public RenderCoordinatesImpl getRenderCoordinates() {
        return mRenderCoordinates;
    }

    /**
     * Retrieves or stores a user data object for this WebContents.
     * @param key Class instance of the object used as the key.
     * @param userDataFactory Factory that creates an object of the generic class. A new object
     *        is created if it hasn't been created and non-null factory is given.
     * @return The created or retrieved user data object. Can be null if the object was
     *         not created yet, or {@code userDataFactory} is null, or the internal data
     *         storage is already garbage-collected.
     */
    public <T extends UserData> T getOrSetUserData(
            Class<T> key, UserDataFactory<T> userDataFactory) {
        // For tests that go without calling |initialize|.
        if (!mInitialized) return null;

        UserDataHost userDataHost = getUserDataHost();

        // Map can be null after WebView gets gc'ed on its way to destruction.
        if (userDataHost == null) {
            Log.d(TAG, "UserDataHost can't be found");
            return null;
        }

        T data = userDataHost.getUserData(key);
        if (data == null && userDataFactory != null) {
            assert userDataHost.getUserData(key) == null; // Do not allow overwriting

            T object = userDataFactory.create(this);
            assert key.isInstance(object);

            // Retrieves from the map again to return null in case |setUserData| fails
            // to store the object.
            data = userDataHost.setUserData(key, object);
        }
        return key.cast(data);
    }

    /** Convenience method to initialize test state. Only use for testing. */
    public void initializeForTesting() {
        if (mInternalsHolder == null) {
            mInternalsHolder = WebContents.createDefaultInternalsHolder();
        }
        WebContentsInternalsImpl internals = (WebContentsInternalsImpl) mInternalsHolder.get();
        if (internals == null) {
            internals = new WebContentsInternalsImpl();
            internals.userDataHost = new UserDataHost();
        }
        mInternalsHolder.set(internals);
        mInitialized = true;
    }

    /** Convenience method to set user data. Only use for testing. */
    public <T extends UserData> void setUserDataForTesting(Class<T> key, T userData) {
        // Be sure to call initializeForTesting() first.
        assert mInitialized;

        WebContentsInternalsImpl internals = (WebContentsInternalsImpl) mInternalsHolder.get();
        internals.userDataHost.setUserData(key, userData);
    }

    public <T extends UserData> void removeUserData(Class<T> key) {
        UserDataHost userDataHost = getUserDataHost();
        if (userDataHost == null) return;
        userDataHost.removeUserData(key);
    }

    /**
     * @return {@code UserDataHost} that contains internal user data. {@code null} if
     *         it is already gc'ed.
     */
    private UserDataHost getUserDataHost() {
        if (mInternalsHolder == null) return null;
        WebContentsInternals internals = mInternalsHolder.get();
        if (internals == null) return null;
        return ((WebContentsInternalsImpl) internals).userDataHost;
    }

    // WindowEventObserver

    @Override
    public void onRotationChanged(int rotation) {
        if (mNativeWebContentsAndroid == 0) return;
        int rotationDegrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0:
                rotationDegrees = 0;
                break;
            case Surface.ROTATION_90:
                rotationDegrees = 90;
                break;
            case Surface.ROTATION_180:
                rotationDegrees = 180;
                break;
            case Surface.ROTATION_270:
                rotationDegrees = -90;
                break;
            default:
                throw new IllegalStateException(
                        "Display.getRotation() shouldn't return that value");
        }
        WebContentsImplJni.get()
                .sendOrientationChangeEvent(mNativeWebContentsAndroid, rotationDegrees);
    }

    @Override
    public void onDIPScaleChanged(float dipScale) {
        if (mNativeWebContentsAndroid == 0) return;
        mRenderCoordinates.setDeviceScaleFactor(dipScale);
        WebContentsImplJni.get().onScaleFactorChanged(mNativeWebContentsAndroid);
    }

    @Override
    public void setFocus(boolean hasFocus) {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().setFocus(mNativeWebContentsAndroid, hasFocus);
    }

    @Override
    public void setDisplayCutoutSafeArea(Rect insets) {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get()
                .setDisplayCutoutSafeArea(
                        mNativeWebContentsAndroid,
                        insets.top,
                        insets.left,
                        insets.bottom,
                        insets.right);
    }

    @Override
    public void notifyRendererPreferenceUpdate() {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().notifyRendererPreferenceUpdate(mNativeWebContentsAndroid);
    }

    @Override
    public void notifyBrowserControlsHeightChanged() {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().notifyBrowserControlsHeightChanged(mNativeWebContentsAndroid);
    }

    @Override
    public void tearDownDialogOverlays() {
        if (mTearDownDialogOverlaysHandlers == null) return;
        Iterator<Runnable> it = mTearDownDialogOverlaysHandlers.iterator();
        while (it.hasNext()) {
            Runnable handler = it.next();
            handler.run();
        }
    }

    @Override
    public boolean needToFireBeforeUnloadOrUnloadEvents() {
        if (mNativeWebContentsAndroid == 0) return false;
        return WebContentsImplJni.get()
                .needToFireBeforeUnloadOrUnloadEvents(mNativeWebContentsAndroid);
    }

    public void addTearDownDialogOverlaysHandler(Runnable handler) {
        if (mTearDownDialogOverlaysHandlers == null) {
            mTearDownDialogOverlaysHandlers = new ObserverList<>();
        }

        assert !mTearDownDialogOverlaysHandlers.hasObserver(handler);
        mTearDownDialogOverlaysHandlers.addObserver(handler);
    }

    public void removeTearDownDialogOverlaysHandler(Runnable handler) {
        assert mTearDownDialogOverlaysHandlers != null;
        assert mTearDownDialogOverlaysHandlers.hasObserver(handler);

        mTearDownDialogOverlaysHandlers.removeObserver(handler);
    }

    @Override
    public void onContentForNavigationEntryShown() {
        checkNotDestroyed();
        WebContentsImplJni.get().onContentForNavigationEntryShown(mNativeWebContentsAndroid);
    }

    @Override
    @AnimationStage
    public int getCurrentBackForwardTransitionStage() {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .getCurrentBackForwardTransitionStage(mNativeWebContentsAndroid);
    }

    @Override
    public void setLongPressLinkSelectText(boolean enabled) {
        checkNotDestroyed();
        WebContentsImplJni.get().setLongPressLinkSelectText(mNativeWebContentsAndroid, enabled);
    }

    @Override
    public void notifyControlsConstraintsChanged(
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo) {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get()
                .notifyControlsConstraintsChanged(
                        mNativeWebContentsAndroid, oldOffsetTagsInfo, offsetTagsInfo);
    }

    private void checkNotDestroyed() {
        if (mNativeWebContentsAndroid != 0) return;
        throw new IllegalStateException(
                "Native WebContents already destroyed", mNativeDestroyThrowable);
    }

    @Override
    public void captureContentAsBitmapForTesting(Callback<Bitmap> callback) {
        WebContentsImplJni.get()
                .captureContentAsBitmapForTesting(mNativeWebContentsAndroid, callback);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {

        // This is static to avoid exposing a public destroy method on the native side of this
        // class.
        void destroyWebContents(long webContentsAndroidPtr);

        WebContents fromNativePtr(long webContentsAndroidPtr);

        void clearNativeReference(long nativeWebContentsAndroid);

        WindowAndroid getTopLevelNativeWindow(long nativeWebContentsAndroid);

        void setTopLevelNativeWindow(long nativeWebContentsAndroid, WindowAndroid windowAndroid);

        RenderFrameHost getMainFrame(long nativeWebContentsAndroid);

        RenderFrameHost getFocusedFrame(long nativeWebContentsAndroid);

        boolean isFocusedElementEditable(long nativeWebContentsAndroid);

        RenderFrameHost getRenderFrameHostFromId(
                long nativeWebContentsAndroid, int renderProcessId, int renderFrameId);

        RenderFrameHost[] getAllRenderFrameHosts(long nativeWebContentsAndroid);

        RenderWidgetHostViewImpl getRenderWidgetHostView(long nativeWebContentsAndroid);

        @Visibility
        int getVisibility(long nativeWebContentsAndroid);

        void updateWebContentsVisibility(long nativeWebContentsAndroid, int visibility);

        String getTitle(long nativeWebContentsAndroid);

        GURL getVisibleURL(long nativeWebContentsAndroid);

        int getVirtualKeyboardMode(long nativeWebContentsAndroid);

        String getEncoding(long nativeWebContentsAndroid);

        boolean isLoading(long nativeWebContentsAndroid);

        boolean shouldShowLoadingUI(long nativeWebContentsAndroid);

        boolean hasUncommittedNavigationInPrimaryMainFrame(long nativeWebContentsAndroid);

        void dispatchBeforeUnload(long nativeWebContentsAndroid, boolean autoCancel);

        void stop(long nativeWebContentsAndroid);

        void cut(long nativeWebContentsAndroid);

        void copy(long nativeWebContentsAndroid);

        void paste(long nativeWebContentsAndroid);

        void pasteAsPlainText(long nativeWebContentsAndroid);

        void replace(long nativeWebContentsAndroid, String word);

        void selectAll(long nativeWebContentsAndroid);

        void collapseSelection(long nativeWebContentsAndroid);

        void setImportance(long nativeWebContentsAndroid, int importance);

        void suspendAllMediaPlayers(long nativeWebContentsAndroid);

        void setAudioMuted(long nativeWebContentsAndroid, boolean mute);

        boolean isAudioMuted(long nativeWebContentsAndroid);

        boolean focusLocationBarByDefault(long nativeWebContentsAndroid);

        boolean isFullscreenForCurrentTab(long nativeWebContentsAndroid);

        void exitFullscreen(long nativeWebContentsAndroid);

        void scrollFocusedEditableNodeIntoView(long nativeWebContentsAndroid);

        void selectAroundCaret(
                long nativeWebContentsAndroid,
                int granularity,
                boolean shouldShowHandle,
                boolean shouldShowContextMenu,
                int startOffset,
                int endOffset,
                int surroundingTextLength);

        void adjustSelectionByCharacterOffset(
                long nativeWebContentsAndroid,
                int startAdjust,
                int endAdjust,
                boolean showSelectionMenu);

        GURL getLastCommittedURL(long nativeWebContentsAndroid);

        boolean isIncognito(long nativeWebContentsAndroid);

        void resumeLoadingCreatedWebContents(long nativeWebContentsAndroid);

        void evaluateJavaScript(
                long nativeWebContentsAndroid, String script, JavaScriptCallback callback);

        void evaluateJavaScriptForTests(
                long nativeWebContentsAndroid, String script, JavaScriptCallback callback);

        void addMessageToDevToolsConsole(long nativeWebContentsAndroid, int level, String message);

        void postMessageToMainFrame(
                long nativeWebContentsAndroid,
                MessagePayload payload,
                String sourceOrigin,
                String targetOrigin,
                MessagePort[] ports);

        boolean hasAccessedInitialDocument(long nativeWebContentsAndroid);

        boolean hasViewTransitionOptIn(long nativeWebContentsAndroid);

        int getThemeColor(long nativeWebContentsAndroid);

        int getBackgroundColor(long nativeWebContentsAndroid);

        float getLoadProgress(long nativeWebContentsAndroid);

        void requestSmartClipExtract(
                long nativeWebContentsAndroid,
                SmartClipCallback callback,
                int x,
                int y,
                int width,
                int height);

        void requestAccessibilitySnapshot(
                long nativeWebContentsAndroid,
                ViewStructure viewStructureRoot,
                ViewStructureBuilder viewStructureBuilder,
                Runnable doneCallback);

        void setOverscrollRefreshHandler(
                long nativeWebContentsAndroid,
                OverscrollRefreshHandler nativeOverscrollRefreshHandler);

        void setSpatialNavigationDisabled(long nativeWebContentsAndroid, boolean disabled);

        void setStylusHandwritingEnabled(long nativeWebContentsAndroid, boolean enabled);

        int downloadImage(
                long nativeWebContentsAndroid,
                GURL url,
                boolean isFavicon,
                int maxBitmapSize,
                boolean bypassCache,
                ImageDownloadCallback callback);

        void setHasPersistentVideo(long nativeWebContentsAndroid, boolean value);

        boolean hasActiveEffectivelyFullscreenVideo(long nativeWebContentsAndroid);

        boolean isPictureInPictureAllowedForFullscreenVideo(long nativeWebContentsAndroid);

        Rect getFullscreenVideoSize(long nativeWebContentsAndroid);

        void setSize(long nativeWebContentsAndroid, int width, int height);

        int getWidth(long nativeWebContentsAndroid);

        int getHeight(long nativeWebContentsAndroid);

        EventForwarder getOrCreateEventForwarder(long nativeWebContentsAndroid);

        void setViewAndroidDelegate(
                long nativeWebContentsAndroid, ViewAndroidDelegate viewDelegate);

        void sendOrientationChangeEvent(long nativeWebContentsAndroid, int orientation);

        void onScaleFactorChanged(long nativeWebContentsAndroid);

        void setFocus(long nativeWebContentsAndroid, boolean focused);

        void setDisplayCutoutSafeArea(
                long nativeWebContentsAndroid, int top, int left, int bottom, int right);

        void notifyRendererPreferenceUpdate(long nativeWebContentsAndroid);

        void notifyBrowserControlsHeightChanged(long nativeWebContentsAndroid);

        boolean isBeingDestroyed(long nativeWebContentsAndroid);

        boolean needToFireBeforeUnloadOrUnloadEvents(long nativeWebContentsAndroid);

        void onContentForNavigationEntryShown(long nativeWebContentsAndroid);

        @AnimationStage
        int getCurrentBackForwardTransitionStage(long nativeWebContentsAndroid);

        void setLongPressLinkSelectText(long nativeWebContentsAndroid, boolean enabled);

        void notifyControlsConstraintsChanged(
                long nativeWebContentsAndroid,
                BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                BrowserControlsOffsetTagsInfo offsetTagsInfo);

        void captureContentAsBitmapForTesting(
                long nativeWebContentsAndroid, Callback<Bitmap> callback);
    }
}
