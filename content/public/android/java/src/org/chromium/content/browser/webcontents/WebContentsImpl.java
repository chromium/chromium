// Copyright 2013 The Chromium Authors. All rights reserved.
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
import android.view.Surface;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content.browser.AppWebMessagePort;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.RenderWidgetHostViewImpl;
import org.chromium.content.browser.ViewEventSinkImpl;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content.browser.framehost.RenderFrameHostDelegate;
import org.chromium.content.browser.framehost.RenderFrameHostImpl;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ImageDownloadCallback;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.ViewEventSink.InternalAccessDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsInternals;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/**
 * The WebContentsImpl Java wrapper to allow communicating with the native WebContentsImpl
 * object.
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
    @VisibleForTesting
    public static void invalidateSerializedWebContentsForTesting() {
        sParcelableUUID = UUID.randomUUID();
    }

    /**
     * A {@link android.os.Parcelable.Creator} instance that is used to build
     * {@link WebContentsImpl} objects from a {@link Parcel}.
     */
    // TODO(crbug.com/635567): Fix this properly.
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
                    return WebContentsImplJni.get().fromNativePtr(
                            bundle.getLong(PARCEL_WEBCONTENTS_KEY));
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
    public interface UserDataFactory<T> { T create(WebContents webContents); }

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
            // The clipRect is in dip scale here. Add the contentOffset in same scale.
            RenderCoordinatesImpl coordinateSpace = getRenderCoordinates();
            clipRect.offset(0,
                    (int) (coordinateSpace.getContentOffsetYPix()
                            / coordinateSpace.getDeviceScaleFactor()));
            Bundle bundle = new Bundle();
            bundle.putString("url", getVisibleUrl());
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

    // Cached copy of all positions and scales as reported by the renderer.
    private RenderCoordinatesImpl mRenderCoordinates;

    private InternalsHolder mInternalsHolder;

    private String mProductVersion;

    private boolean mInitialized;

    // Remember the stack for clearing native the native stack for debugging use after destroy.
    private Throwable mNativeDestroyThrowable;

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
    private static WebContentsImpl create(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        return new WebContentsImpl(nativeWebContentsAndroid, navigationController);
    }

    @Override
    public void initialize(String productVersion, ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate accessDelegate, WindowAndroid windowAndroid,
            InternalsHolder internalsHolder) {
        // Makes sure |initialize| is not called more than once.
        assert !mInitialized;
        assert internalsHolder != null;

        mProductVersion = productVersion;

        mInternalsHolder = internalsHolder;
        WebContentsInternalsImpl internals = new WebContentsInternalsImpl();
        internals.userDataHost = new UserDataHost();
        mInternalsHolder.set(internals);

        mRenderCoordinates = new RenderCoordinatesImpl();
        mRenderCoordinates.reset();

        mInitialized = true;

        setViewAndroidDelegate(viewDelegate);
        setTopLevelNativeWindow(windowAndroid);

        ViewEventSinkImpl.from(this).setAccessDelegate(accessDelegate);
        getRenderCoordinates().setDeviceScaleFactor(windowAndroid.getDisplay().getDipScale());
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
    private void clearNativePtr() {
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
        return WebContentsImplJni.get().getTopLevelNativeWindow(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void setTopLevelNativeWindow(WindowAndroid windowAndroid) {
        checkNotDestroyed();
        WebContentsImplJni.get().setTopLevelNativeWindow(
                mNativeWebContentsAndroid, WebContentsImpl.this, windowAndroid);
        WindowEventObserverManager.from(this).onWindowAndroidChanged(windowAndroid);
    }

    @Override
    public ViewAndroidDelegate getViewAndroidDelegate() {
        WebContentsInternals internals = mInternalsHolder.get();
        if (internals == null) return null;
        return ((WebContentsInternalsImpl) internals).viewAndroidDelegate;
    }

    public void setViewAndroidDelegate(ViewAndroidDelegate viewDelegate) {
        checkNotDestroyed();
        WebContentsInternals internals = mInternalsHolder.get();
        assert internals != null;
        WebContentsInternalsImpl impl = (WebContentsInternalsImpl) internals;
        assert impl.viewAndroidDelegate == null;
        impl.viewAndroidDelegate = viewDelegate;
        WebContentsImplJni.get().setViewAndroidDelegate(
                mNativeWebContentsAndroid, WebContentsImpl.this, viewDelegate);
    }

    @Override
    public void destroy() {
        // Note that |WebContents.destroy| is not guaranteed to be invoked.
        // Any resource release relying on this method will likely be leaked.

        if (!ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("Attempting to destroy WebContents on non-UI thread");
        }

        if (mNativeWebContentsAndroid != 0) {
            WebContentsImplJni.get().destroyWebContents(mNativeWebContentsAndroid);
        }
    }

    @Override
    public boolean isDestroyed() {
        return mNativeWebContentsAndroid == 0
                || WebContentsImplJni.get().isBeingDestroyed(
                        mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void clearNativeReference() {
        if (mNativeWebContentsAndroid != 0) {
            WebContentsImplJni.get().clearNativeReference(
                    mNativeWebContentsAndroid, WebContentsImpl.this);
        }
    }

    @Override
    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    @Override
    public RenderFrameHost getMainFrame() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getMainFrame(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public RenderFrameHost getFocusedFrame() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getFocusedFrame(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public @Nullable RenderWidgetHostViewImpl getRenderWidgetHostView() {
        if (mNativeWebContentsAndroid == 0) return null;
        RenderWidgetHostViewImpl rwhvi = WebContentsImplJni.get().getRenderWidgetHostView(
                mNativeWebContentsAndroid, WebContentsImpl.this);
        if (rwhvi == null || rwhvi.isDestroyed()) return null;

        return rwhvi;
    }

    @Override
    public String getTitle() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getTitle(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public String getVisibleUrl() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getVisibleURL(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public String getEncoding() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getEncoding(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public boolean isLoading() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isLoading(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public boolean isLoadingToDifferentDocument() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isLoadingToDifferentDocument(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void stop() {
        checkNotDestroyed();
        WebContentsImplJni.get().stop(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    /**
     * Cut the selected content.
     */
    public void cut() {
        checkNotDestroyed();
        WebContentsImplJni.get().cut(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    /**
     * Copy the selected content.
     */
    public void copy() {
        checkNotDestroyed();
        WebContentsImplJni.get().copy(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    /**
     * Paste content from the clipboard.
     */
    public void paste() {
        checkNotDestroyed();
        WebContentsImplJni.get().paste(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    /**
     * Paste content from the clipboard without format.
     */
    public void pasteAsPlainText() {
        checkNotDestroyed();
        WebContentsImplJni.get().pasteAsPlainText(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    /**
     * Replace the selected text with the {@code word}.
     */
    public void replace(String word) {
        checkNotDestroyed();
        WebContentsImplJni.get().replace(mNativeWebContentsAndroid, WebContentsImpl.this, word);
    }

    /**
     * Select all content.
     */
    public void selectAll() {
        checkNotDestroyed();
        WebContentsImplJni.get().selectAll(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    /**
     * Collapse the selection to the end of selection range.
     */
    public void collapseSelection() {
        // collapseSelection may get triggered when certain selection-related widgets
        // are destroyed. As the timing for such destruction is unpredictable,
        // safely guard against this case.
        if (isDestroyed()) return;
        WebContentsImplJni.get().collapseSelection(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void onHide() {
        checkNotDestroyed();
        SelectionPopupControllerImpl controller = getSelectionPopupController();
        if (controller != null) controller.hidePopupsAndPreserveSelection();
        WebContentsImplJni.get().onHide(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void onShow() {
        checkNotDestroyed();
        WebContentsAccessibilityImpl wcax = WebContentsAccessibilityImpl.fromWebContents(this);
        if (wcax != null) wcax.refreshState();
        SelectionPopupControllerImpl controller = getSelectionPopupController();
        if (controller != null) controller.restoreSelectionPopupsIfNecessary();
        WebContentsImplJni.get().onShow(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    private SelectionPopupControllerImpl getSelectionPopupController() {
        return SelectionPopupControllerImpl.fromWebContents(this);
    }

    @Override
    public void setImportance(@ChildProcessImportance int mainFrameImportance) {
        checkNotDestroyed();
        WebContentsImplJni.get().setImportance(
                mNativeWebContentsAndroid, WebContentsImpl.this, mainFrameImportance);
    }

    @Override
    public void suspendAllMediaPlayers() {
        checkNotDestroyed();
        WebContentsImplJni.get().suspendAllMediaPlayers(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void setAudioMuted(boolean mute) {
        checkNotDestroyed();
        WebContentsImplJni.get().setAudioMuted(
                mNativeWebContentsAndroid, WebContentsImpl.this, mute);
    }

    @Override
    public boolean isShowingInterstitialPage() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isShowingInterstitialPage(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public boolean focusLocationBarByDefault() {
        checkNotDestroyed();
        return WebContentsImplJni.get().focusLocationBarByDefault(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }


    @Override
    public void exitFullscreen() {
        checkNotDestroyed();
        WebContentsImplJni.get().exitFullscreen(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void scrollFocusedEditableNodeIntoView() {
        checkNotDestroyed();
        // The native side keeps track of whether the zoom and scroll actually occurred. It is
        // more efficient to do it this way and sometimes fire an unnecessary message rather
        // than synchronize with the renderer and always have an additional message.
        WebContentsImplJni.get().scrollFocusedEditableNodeIntoView(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void selectWordAroundCaret() {
        checkNotDestroyed();
        WebContentsImplJni.get().selectWordAroundCaret(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void adjustSelectionByCharacterOffset(
            int startAdjust, int endAdjust, boolean showSelectionMenu) {
        WebContentsImplJni.get().adjustSelectionByCharacterOffset(mNativeWebContentsAndroid,
                WebContentsImpl.this, startAdjust, endAdjust, showSelectionMenu);
    }

    @Override
    public String getLastCommittedUrl() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getLastCommittedURL(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public boolean isIncognito() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isIncognito(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void resumeLoadingCreatedWebContents() {
        checkNotDestroyed();
        WebContentsImplJni.get().resumeLoadingCreatedWebContents(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void evaluateJavaScript(String script, JavaScriptCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (isDestroyed() || script == null) return;
        WebContentsImplJni.get().evaluateJavaScript(
                mNativeWebContentsAndroid, WebContentsImpl.this, script, callback);
    }

    @Override
    @VisibleForTesting
    public void evaluateJavaScriptForTests(String script, JavaScriptCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (script == null) return;
        checkNotDestroyed();
        WebContentsImplJni.get().evaluateJavaScriptForTests(
                mNativeWebContentsAndroid, WebContentsImpl.this, script, callback);
    }

    @Override
    public void addMessageToDevToolsConsole(int level, String message) {
        checkNotDestroyed();
        WebContentsImplJni.get().addMessageToDevToolsConsole(
                mNativeWebContentsAndroid, WebContentsImpl.this, level, message);
    }

    @Override
    public void postMessageToMainFrame(
            String message, String sourceOrigin, String targetOrigin, MessagePort[] ports) {
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
        WebContentsImplJni.get().postMessageToMainFrame(mNativeWebContentsAndroid,
                WebContentsImpl.this, message, sourceOrigin, targetOrigin, ports);
    }

    @Override
    public AppWebMessagePort[] createMessageChannel()
            throws IllegalStateException {
        return AppWebMessagePort.createPair();
    }

    @Override
    public boolean hasAccessedInitialDocument() {
        checkNotDestroyed();
        return WebContentsImplJni.get().hasAccessedInitialDocument(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResult(
            String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    @Override
    public int getThemeColor() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getThemeColor(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public float getLoadProgress() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getLoadProgress(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void requestSmartClipExtract(int x, int y, int width, int height) {
        if (mSmartClipCallback == null) return;
        checkNotDestroyed();
        RenderCoordinatesImpl coordinateSpace = getRenderCoordinates();
        float dpi = coordinateSpace.getDeviceScaleFactor();
        y = y - (int) coordinateSpace.getContentOffsetYPix();
        WebContentsImplJni.get().requestSmartClipExtract(mNativeWebContentsAndroid,
                WebContentsImpl.this, mSmartClipCallback, (int) (x / dpi), (int) (y / dpi),
                (int) (width / dpi), (int) (height / dpi));
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
    private static void onSmartClipDataExtracted(String text, String html, int left, int top,
            int right, int bottom, SmartClipCallback callback) {
        callback.onSmartClipDataExtracted(text, html, new Rect(left, top, right, bottom));
    }

    @Override
    public void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback) {
        checkNotDestroyed();
        WebContentsImplJni.get().requestAccessibilitySnapshot(
                mNativeWebContentsAndroid, WebContentsImpl.this, callback);
    }

    @VisibleForTesting
    public void simulateRendererKilledForTesting(boolean wasOomProtected) {
        if (mObserverProxy != null) {
            mObserverProxy.renderProcessGone(wasOomProtected);
        }
    }

    // root node can be null if parsing fails.
    @CalledByNative
    private static void onAccessibilitySnapshot(AccessibilitySnapshotNode root,
            AccessibilitySnapshotCallback callback) {
        callback.onAccessibilitySnapshot(root);
    }

    @CalledByNative
    private static void addAccessibilityNodeAsChild(AccessibilitySnapshotNode parent,
            AccessibilitySnapshotNode child) {
        parent.addChild(child);
    }

    @CalledByNative
    private static AccessibilitySnapshotNode createAccessibilitySnapshotNode(int parentRelativeLeft,
            int parentRelativeTop, int width, int height, boolean isRootNode, String text,
            int color, int bgcolor, float size, boolean bold, boolean italic, boolean underline,
            boolean lineThrough, String className) {
        AccessibilitySnapshotNode node = new AccessibilitySnapshotNode(text, className);

        // if size is smaller than 0, then style information does not exist.
        if (size >= 0.0) {
            node.setStyle(color, bgcolor, size, bold, italic, underline, lineThrough);
        }
        node.setLocationInfo(parentRelativeLeft, parentRelativeTop, width, height, isRootNode);
        return node;
    }

    @CalledByNative
    private static void setAccessibilitySnapshotSelection(
            AccessibilitySnapshotNode node, int start, int end) {
        node.setSelection(start, end);
    }

    @Override
    public EventForwarder getEventForwarder() {
        assert mNativeWebContentsAndroid != 0;
        if (mEventForwarder == null) {
            checkNotDestroyed();
            mEventForwarder = WebContentsImplJni.get().getOrCreateEventForwarder(
                    mNativeWebContentsAndroid, WebContentsImpl.this);
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
        WebContentsImplJni.get().setOverscrollRefreshHandler(
                mNativeWebContentsAndroid, WebContentsImpl.this, handler);
    }

    @Override
    public void setSpatialNavigationDisabled(boolean disabled) {
        checkNotDestroyed();
        WebContentsImplJni.get().setSpatialNavigationDisabled(
                mNativeWebContentsAndroid, WebContentsImpl.this, disabled);
    }

    @Override
    public int downloadImage(String url, boolean isFavicon, int maxBitmapSize,
            boolean bypassCache, ImageDownloadCallback callback) {
        checkNotDestroyed();
        return WebContentsImplJni.get().downloadImage(mNativeWebContentsAndroid,
                WebContentsImpl.this, url, isFavicon, maxBitmapSize, bypassCache, callback);
    }

    @CalledByNative
    private void onDownloadImageFinished(ImageDownloadCallback callback, int id, int httpStatusCode,
            String imageUrl, List<Bitmap> bitmaps, List<Rect> sizes) {
        callback.onFinishDownloadImage(id, httpStatusCode, imageUrl, bitmaps, sizes);
    }

    @Override
    public void setHasPersistentVideo(boolean value) {
        checkNotDestroyed();
        WebContentsImplJni.get().setHasPersistentVideo(
                mNativeWebContentsAndroid, WebContentsImpl.this, value);
    }

    @Override
    public boolean hasActiveEffectivelyFullscreenVideo() {
        checkNotDestroyed();
        return WebContentsImplJni.get().hasActiveEffectivelyFullscreenVideo(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public boolean isPictureInPictureAllowedForFullscreenVideo() {
        checkNotDestroyed();
        return WebContentsImplJni.get().isPictureInPictureAllowedForFullscreenVideo(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public @Nullable Rect getFullscreenVideoSize() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getFullscreenVideoSize(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public void setSize(int width, int height) {
        checkNotDestroyed();
        WebContentsImplJni.get().setSize(
                mNativeWebContentsAndroid, WebContentsImpl.this, width, height);
    }

    @Override
    public int getWidth() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getWidth(mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    @Override
    public int getHeight() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getHeight(mNativeWebContentsAndroid, WebContentsImpl.this);
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

    /**
     * Returns {@link RenderCoordinatesImpl}.
     */
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
        WebContentsImplJni.get().sendOrientationChangeEvent(
                mNativeWebContentsAndroid, WebContentsImpl.this, rotationDegrees);
    }

    @Override
    public void onDIPScaleChanged(float dipScale) {
        if (mNativeWebContentsAndroid == 0) return;
        mRenderCoordinates.setDeviceScaleFactor(dipScale);
        WebContentsImplJni.get().onScaleFactorChanged(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    public void setFocus(boolean hasFocus) {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().setFocus(
                mNativeWebContentsAndroid, WebContentsImpl.this, hasFocus);
    }

    @Override
    public void setDisplayCutoutSafeArea(Rect insets) {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().setDisplayCutoutSafeArea(mNativeWebContentsAndroid,
                WebContentsImpl.this, insets.top, insets.left, insets.bottom, insets.right);
    }

    @Override
    public void notifyRendererPreferenceUpdate() {
        if (mNativeWebContentsAndroid == 0) return;
        WebContentsImplJni.get().notifyRendererPreferenceUpdate(
                mNativeWebContentsAndroid, WebContentsImpl.this);
    }

    private void checkNotDestroyed() {
        if (mNativeWebContentsAndroid != 0) return;
        throw new IllegalStateException(
                "Native WebContents already destroyed", mNativeDestroyThrowable);
    }

    @NativeMethods
    interface Natives {
        // This is static to avoid exposing a public destroy method on the native side of this
        // class.
        void destroyWebContents(long webContentsAndroidPtr);

        WebContents fromNativePtr(long webContentsAndroidPtr);
        void clearNativeReference(long nativeWebContentsAndroid, WebContentsImpl caller);
        WindowAndroid getTopLevelNativeWindow(
                long nativeWebContentsAndroid, WebContentsImpl caller);
        void setTopLevelNativeWindow(
                long nativeWebContentsAndroid, WebContentsImpl caller, WindowAndroid windowAndroid);
        RenderFrameHost getMainFrame(long nativeWebContentsAndroid, WebContentsImpl caller);
        RenderFrameHost getFocusedFrame(long nativeWebContentsAndroid, WebContentsImpl caller);
        RenderWidgetHostViewImpl getRenderWidgetHostView(
                long nativeWebContentsAndroid, WebContentsImpl caller);
        String getTitle(long nativeWebContentsAndroid, WebContentsImpl caller);
        String getVisibleURL(long nativeWebContentsAndroid, WebContentsImpl caller);
        String getEncoding(long nativeWebContentsAndroid, WebContentsImpl caller);
        boolean isLoading(long nativeWebContentsAndroid, WebContentsImpl caller);
        boolean isLoadingToDifferentDocument(long nativeWebContentsAndroid, WebContentsImpl caller);
        void stop(long nativeWebContentsAndroid, WebContentsImpl caller);
        void cut(long nativeWebContentsAndroid, WebContentsImpl caller);
        void copy(long nativeWebContentsAndroid, WebContentsImpl caller);
        void paste(long nativeWebContentsAndroid, WebContentsImpl caller);
        void pasteAsPlainText(long nativeWebContentsAndroid, WebContentsImpl caller);
        void replace(long nativeWebContentsAndroid, WebContentsImpl caller, String word);
        void selectAll(long nativeWebContentsAndroid, WebContentsImpl caller);
        void collapseSelection(long nativeWebContentsAndroid, WebContentsImpl caller);
        void onHide(long nativeWebContentsAndroid, WebContentsImpl caller);
        void onShow(long nativeWebContentsAndroid, WebContentsImpl caller);
        void setImportance(long nativeWebContentsAndroid, WebContentsImpl caller, int importance);
        void suspendAllMediaPlayers(long nativeWebContentsAndroid, WebContentsImpl caller);
        void setAudioMuted(long nativeWebContentsAndroid, WebContentsImpl caller, boolean mute);
        boolean isShowingInterstitialPage(long nativeWebContentsAndroid, WebContentsImpl caller);
        boolean focusLocationBarByDefault(long nativeWebContentsAndroid, WebContentsImpl caller);
        void exitFullscreen(long nativeWebContentsAndroid, WebContentsImpl caller);
        void scrollFocusedEditableNodeIntoView(
                long nativeWebContentsAndroid, WebContentsImpl caller);
        void selectWordAroundCaret(long nativeWebContentsAndroid, WebContentsImpl caller);
        void adjustSelectionByCharacterOffset(long nativeWebContentsAndroid, WebContentsImpl caller,
                int startAdjust, int endAdjust, boolean showSelectionMenu);
        String getLastCommittedURL(long nativeWebContentsAndroid, WebContentsImpl caller);
        boolean isIncognito(long nativeWebContentsAndroid, WebContentsImpl caller);
        void resumeLoadingCreatedWebContents(long nativeWebContentsAndroid, WebContentsImpl caller);
        void evaluateJavaScript(long nativeWebContentsAndroid, WebContentsImpl caller,
                String script, JavaScriptCallback callback);
        void evaluateJavaScriptForTests(long nativeWebContentsAndroid, WebContentsImpl caller,
                String script, JavaScriptCallback callback);
        void addMessageToDevToolsConsole(
                long nativeWebContentsAndroid, WebContentsImpl caller, int level, String message);
        void postMessageToMainFrame(long nativeWebContentsAndroid, WebContentsImpl caller,
                String message, String sourceOrigin, String targetOrigin, MessagePort[] ports);
        boolean hasAccessedInitialDocument(long nativeWebContentsAndroid, WebContentsImpl caller);
        int getThemeColor(long nativeWebContentsAndroid, WebContentsImpl caller);
        float getLoadProgress(long nativeWebContentsAndroid, WebContentsImpl caller);
        void requestSmartClipExtract(long nativeWebContentsAndroid, WebContentsImpl caller,
                SmartClipCallback callback, int x, int y, int width, int height);
        void requestAccessibilitySnapshot(long nativeWebContentsAndroid, WebContentsImpl caller,
                AccessibilitySnapshotCallback callback);
        void setOverscrollRefreshHandler(long nativeWebContentsAndroid, WebContentsImpl caller,
                OverscrollRefreshHandler nativeOverscrollRefreshHandler);
        void setSpatialNavigationDisabled(
                long nativeWebContentsAndroid, WebContentsImpl caller, boolean disabled);
        int downloadImage(long nativeWebContentsAndroid, WebContentsImpl caller, String url,
                boolean isFavicon, int maxBitmapSize, boolean bypassCache,
                ImageDownloadCallback callback);
        void setHasPersistentVideo(
                long nativeWebContentsAndroid, WebContentsImpl caller, boolean value);
        boolean hasActiveEffectivelyFullscreenVideo(
                long nativeWebContentsAndroid, WebContentsImpl caller);
        boolean isPictureInPictureAllowedForFullscreenVideo(
                long nativeWebContentsAndroid, WebContentsImpl caller);
        Rect getFullscreenVideoSize(long nativeWebContentsAndroid, WebContentsImpl caller);
        void setSize(long nativeWebContentsAndroid, WebContentsImpl caller, int width, int height);
        int getWidth(long nativeWebContentsAndroid, WebContentsImpl caller);
        int getHeight(long nativeWebContentsAndroid, WebContentsImpl caller);
        EventForwarder getOrCreateEventForwarder(
                long nativeWebContentsAndroid, WebContentsImpl caller);
        void setViewAndroidDelegate(long nativeWebContentsAndroid, WebContentsImpl caller,
                ViewAndroidDelegate viewDelegate);
        void sendOrientationChangeEvent(
                long nativeWebContentsAndroid, WebContentsImpl caller, int orientation);
        void onScaleFactorChanged(long nativeWebContentsAndroid, WebContentsImpl caller);
        void setFocus(long nativeWebContentsAndroid, WebContentsImpl caller, boolean focused);
        void setDisplayCutoutSafeArea(long nativeWebContentsAndroid, WebContentsImpl caller,
                int top, int left, int bottom, int right);
        void notifyRendererPreferenceUpdate(long nativeWebContentsAndroid, WebContentsImpl caller);
        boolean isBeingDestroyed(long nativeWebContentsAndroid, WebContentsImpl caller);
    }
}
