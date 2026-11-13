// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import android.util.LongSparseArray;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.ViewStructure;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
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
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.AppWebMessagePort;
import org.chromium.content.browser.GestureListenerManagerImpl;
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
import org.chromium.content_public.browser.BackForwardTransitionAnimationManager;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentFeatureMap;
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
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.BrowserControlsOffsetTagDefinitions;
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
@NullMarked
public class WebContentsImpl
        implements WebContents,
                WebContentsObserver.Observable,
                RenderFrameHostDelegate,
                WindowEventObserver {
    @JniType("::content::WebContentsAndroid")
    interface NativeWebContentsAndroid extends JniTypeToken {}

    private static final String TAG = "WebContentsImpl";

    // Map from peer ID to WebContentsImpl to allow scaling of unlimited web contents objects.
    // ScopedGlobalRef tables are finite.
    private static final LongSparseArray<WebContentsImpl> sWebContentsMap = new LongSparseArray<>();

    // Unlike the native address, a peer ID is never reused within a process, so a stale ID
    // (e.g. in a Parcel) cannot resolve to a different WebContentsImpl.
    private static long sNextPeerId = 1;

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
                public @Nullable WebContents createFromParcel(Parcel source) {
                    Bundle bundle = source.readBundle();

                    // Check the version.
                    if (bundle == null || bundle.getLong(PARCEL_VERSION_KEY, -1) != 0) {
                        return null;
                    }

                    // Check that we're in the same process.
                    ParcelUuid parcelUuid = bundle.getParcelable(PARCEL_PROCESS_GUARD_KEY);
                    if (parcelUuid == null
                            || sParcelableUUID.compareTo(parcelUuid.getUuid()) != 0) {
                        return null;
                    }

                    return sWebContentsMap.get(bundle.getLong(PARCEL_WEBCONTENTS_KEY));
                }

                @Override
                public WebContents[] newArray(int size) {
                    return new WebContents[size];
                }
            };

    // Note this list may be incomplete. Frames that never had to initialize java side would
    // not have an entry here. This is here mainly to keep the java RenderFrameHosts alive, since
    // native side generally cannot safely hold strong references to them.
    private final List<RenderFrameHostImpl> mFrames = new ArrayList<>();

    private @Nullable JniRawPtr<NativeWebContentsAndroid> mNativeWebContentsAndroid;
    private final long mPeerId;
    private final NavigationController mNavigationController;

    // Lazily created proxy observer for handling all Java-based WebContentsObservers.
    private @Nullable WebContentsObserverProxy mObserverProxy;

    class SmartClipCallback {
        public SmartClipCallback(final Handler smartClipHandler) {
            mHandler = smartClipHandler;
        }

        public void onSmartClipDataExtracted(String text, String html, Rect clipRect) {
            RenderCoordinatesImpl coordinateSpace = getRenderCoordinates();
            clipRect.offset(0, (int) coordinateSpace.getContentOffsetTopPix());
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

    private @Nullable SmartClipCallback mSmartClipCallback;

    private @Nullable EventForwarder mEventForwarder;

    private @Nullable StylusWritingHandler mStylusWritingHandler;

    // Cached copy of all positions and scales as reported by the renderer.
    private RenderCoordinatesImpl mRenderCoordinates;

    private InternalsHolder mInternalsHolder;

    private String mProductVersion;

    private boolean mInitialized;

    // Remember the stack for clearing native the native stack for debugging use after destroy.
    private @Nullable Throwable mNativeDestroyThrowable;

    private @Nullable ObserverList<Runnable> mTearDownDialogOverlaysHandlers;

    private static class WebContentsInternalsImpl implements WebContentsInternals {
        public final UserDataHost userDataHost = new UserDataHost();
        public @Nullable ViewAndroidDelegate viewAndroidDelegate;
    }

    private WebContentsImpl(
            JniRawPtr<NativeWebContentsAndroid> nativeWebContentsAndroid,
            NavigationController navigationController) {
        mNativeWebContentsAndroid = nativeWebContentsAndroid;
        mNavigationController = navigationController;
        mPeerId = sNextPeerId++;
        sWebContentsMap.put(mPeerId, this);
    }

    static WebContentsImpl create(
            JniRawPtr<NativeWebContentsAndroid> nativeWebContentsAndroid,
            NavigationController navigationController) {
        return new WebContentsImpl(nativeWebContentsAndroid, navigationController);
    }

    /** Creates an instance backed by a fake native pointer, for tests that mock the natives. */
    public static WebContentsImpl createForTesting(NavigationController navigationController) {
        return new WebContentsImpl(JniRawPtr.createForTesting(1), navigationController);
    }

    /** Native keeps only the returned peer ID and looks the instance up via getJavaObject(). */
    @CalledByNative
    static long createAndGetPeerId(
            JniRawPtr<NativeWebContentsAndroid> nativeWebContentsAndroid,
            NavigationController navigationController) {
        return create(nativeWebContentsAndroid, navigationController).mPeerId;
    }

    @CalledByNative
    public static @Nullable WebContentsImpl getJavaObject(long peerId) {
        return sWebContentsMap.get(peerId);
    }

    @Override
    @Initializer
    public void setDelegates(
            String productVersion,
            ViewAndroidDelegate viewDelegate,
            @Nullable InternalAccessDelegate accessDelegate,
            @Nullable WindowAndroid windowAndroid,
            InternalsHolder internalsHolder) {
        assert internalsHolder != null;

        mProductVersion = productVersion;

        WebContentsInternalsImpl internals;
        if (mInternalsHolder != null) {
            internals = (WebContentsInternalsImpl) mInternalsHolder.get();
        } else {
            internals = new WebContentsInternalsImpl();
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
            mObserverProxy.webContentsDestroyed();
            mObserverProxy = null;
        }
    }

    public @Nullable Context getContext() {
        assert mInitialized;

        WindowAndroid window = getTopLevelNativeWindow();
        return window != null ? window.getContext().get() : null;
    }

    public @Nullable String getProductVersion() {
        assert mInitialized;
        return mProductVersion;
    }

    @CalledByNative
    @VisibleForTesting
    void clearNativePtr() {
        mNativeDestroyThrowable = new RuntimeException("clearNativePtr");
        assert mNativeWebContentsAndroid != null;
        mNativeWebContentsAndroid.release();
        mNativeWebContentsAndroid = null;
        clearJavaWebContentsObservers();
        UserDataHost userDataHost = getUserDataHost();
        if (userDataHost != null) {
            userDataHost.destroy();
            mInternalsHolder.set(null);
        }
        var removedValue = sWebContentsMap.get(mPeerId);
        sWebContentsMap.remove(mPeerId);
        assert removedValue != null;
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
        data.putLong(PARCEL_WEBCONTENTS_KEY, mPeerId);
        dest.writeBundle(data);
    }

    @CalledByNative
    @Nullable JniPtr<NativeWebContentsAndroid> getNativePointer() {
        return mNativeWebContentsAndroid;
    }

    @Override
    public @Nullable WindowAndroid getTopLevelNativeWindow() {
        checkNotDestroyed();
        return WebContentsImplJni.get().getTopLevelNativeWindow(mNativeWebContentsAndroid);
    }

    @Override
    public void setTopLevelNativeWindow(@Nullable WindowAndroid windowAndroid) {
        checkNotDestroyed();
        WebContentsImplJni.get().setTopLevelNativeWindow(mNativeWebContentsAndroid, windowAndroid);
        WindowEventObserverManager.from(this).onWindowAndroidChanged(windowAndroid);
        if (mObserverProxy != null) mObserverProxy.onTopLevelNativeWindowChanged(windowAndroid);
    }

    @Override
    public @Nullable ViewAndroidDelegate getViewAndroidDelegate() {
        // TODO(agrieve): I suspect this never returns null...
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

        if (mNativeWebContentsAndroid != null) {
            WebContentsImplJni.get().destroyWebContents(mNativeWebContentsAndroid);

            if (mNativeWebContentsAndroid != null) {
                // Normally the native object would have been destroyed by clearNativePtr() being
                // called from destroyWebContents(). However, if JNI is mocked it will not have been
                // invoked so invoke it explicitly here.
                clearNativePtr();
            }
        }
    }

    @Override
    public boolean isBeingCaptured() {
        return mNativeWebContentsAndroid != null
                && WebContentsImplJni.get().isBeingCaptured(mNativeWebContentsAndroid);
    }

    @Override
    @EnsuresNonNullIf(value = "mNativeWebContentsAndroid", result = false)
    public boolean isDestroyed() {
        return mNativeWebContentsAndroid == null
                || WebContentsImplJni.get().isBeingDestroyed(mNativeWebContentsAndroid);
    }

    @Override
    public void clearNativeReference() {
        if (mNativeWebContentsAndroid != null) {
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
        if (mNativeWebContentsAndroid == null) return null;
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
    public void discard(Runnable onDiscarded) {
        checkNotDestroyed();
        assert ContentFeatureMap.isEnabled(ContentFeatures.WEB_CONTENTS_DISCARD);
        WebContentsImplJni.get().discard(mNativeWebContentsAndroid, onDiscarded);
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
        if (mNativeWebContentsAndroid == null) return;
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

    private @Nullable SelectionPopupControllerImpl getSelectionPopupController() {
        return SelectionPopupControllerImpl.fromWebContents(this);
    }

    @Override
    public void setPrimaryPageImportance(
            @ChildProcessImportance int mainFrameImportance,
            @ChildProcessImportance int subframeImportance) {
        checkNotDestroyed();
        assert mainFrameImportance >= subframeImportance;
        WebContentsImplJni.get()
                .setPrimaryPageImportance(
                        mNativeWebContentsAndroid, mainFrameImportance, subframeImportance);
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
                        assumeNonNull(mNativeWebContentsAndroid),
                        startAdjust,
                        endAdjust,
                        showSelectionMenu);
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
    public void evaluateJavaScript(String script, @Nullable JavaScriptCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (isDestroyed() || script == null) return;
        WebContentsImplJni.get().evaluateJavaScript(mNativeWebContentsAndroid, script, callback);
    }

    @Override
    public void evaluateJavaScriptForTests(String script, @Nullable JavaScriptCallback callback) {
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
            @Nullable String sourceOrigin,
            String targetOrigin,
            MessagePort @Nullable [] ports) {
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
                        assumeNonNull(mNativeWebContentsAndroid),
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
        y = y - (int) coordinateSpace.getContentOffsetTopPix();
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
    public void setStylusWritingHandler(@Nullable StylusWritingHandler stylusWritingHandler) {
        mStylusWritingHandler = stylusWritingHandler;
        if (mNativeWebContentsAndroid == null) return;
        WebContentsImplJni.get()
                .setStylusHandwritingEnabled(
                        mNativeWebContentsAndroid, mStylusWritingHandler != null);
    }

    @Override
    public @Nullable StylusWritingImeCallback getStylusWritingImeCallback() {
        ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(this);
        if (imeAdapter == null) return null;
        return imeAdapter.getStylusWritingImeCallback();
    }

    public @Nullable StylusWritingHandler getStylusWritingHandler() {
        return mStylusWritingHandler;
    }

    @Override
    public EventForwarder getEventForwarder() {
        assert mNativeWebContentsAndroid != null;
        if (mEventForwarder == null) {
            checkNotDestroyed();
            mEventForwarder =
                    WebContentsImplJni.get().getOrCreateEventForwarder(mNativeWebContentsAndroid);
            mEventForwarder.setStylusWritingDelegate(
                    new EventForwarder.StylusWritingDelegate() {
                        @Override
                        public boolean handleTouchEvent(MotionEvent motionEvent) {
                            ViewAndroidDelegate viewAndroidDelegate = getViewAndroidDelegate();
                            return mStylusWritingHandler != null
                                    && viewAndroidDelegate != null
                                    && viewAndroidDelegate.getContainerView() != null
                                    && mStylusWritingHandler.handleTouchEvent(
                                            motionEvent, viewAndroidDelegate.getContainerView());
                        }

                        @Override
                        public void handleHoverEvent(MotionEvent motionEvent) {
                            ViewAndroidDelegate viewAndroidDelegate = getViewAndroidDelegate();
                            if (mStylusWritingHandler != null
                                    && viewAndroidDelegate != null
                                    && viewAndroidDelegate.getContainerView() != null) {
                                mStylusWritingHandler.handleHoverEvent(
                                        motionEvent, viewAndroidDelegate.getContainerView());
                            }
                        }
                    });
        }
        return mEventForwarder;
    }

    @Override
    public void addObserver(WebContentsObserver observer) {
        assert mNativeWebContentsAndroid != null;
        if (mObserverProxy == null) mObserverProxy = new WebContentsObserverProxy(this);
        mObserverProxy.addObserver(observer);
    }

    @Override
    public void removeObserver(WebContentsObserver observer) {
        if (mObserverProxy == null) return;
        mObserverProxy.removeObserver(observer);
    }

    @Override
    public void setOverscrollRefreshHandler(@Nullable OverscrollRefreshHandler handler) {
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

    @Override
    public <T extends UserData> @Nullable T getOrSetUserData(
            Class<T> key, @Nullable UserDataFactory<T> userDataFactory) {
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
        }
        mInternalsHolder.set(internals);
        mInitialized = true;
    }

    /** Convenience method to set user data. Only use for testing. */
    public <T extends UserData> void setUserDataForTesting(Class<T> key, T userData) {
        // Be sure to call initializeForTesting() first.
        assert mInitialized;

        WebContentsInternalsImpl internals =
                assumeNonNull((WebContentsInternalsImpl) mInternalsHolder.get());
        internals.userDataHost.setUserData(key, userData);
    }

    @Override
    public <T extends UserData> void removeUserData(Class<T> key) {
        UserDataHost userDataHost = getUserDataHost();
        if (userDataHost == null) return;
        userDataHost.removeUserData(key);
    }

    /**
     * @return {@code UserDataHost} that contains internal user data. {@code null} if
     *         it is already gc'ed.
     */
    private @Nullable UserDataHost getUserDataHost() {
        if (mInternalsHolder == null) return null;
        WebContentsInternals internals = mInternalsHolder.get();
        if (internals == null) return null;
        return ((WebContentsInternalsImpl) internals).userDataHost;
    }

    // WindowEventObserver

    @Override
    public void onRotationChanged(int rotation) {
        if (mNativeWebContentsAndroid == null) return;
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
        if (mNativeWebContentsAndroid == null) return;
        mRenderCoordinates.setDeviceScaleFactor(dipScale);
        WebContentsImplJni.get().onScaleFactorChanged(mNativeWebContentsAndroid);
    }

    @Override
    public void setFocus(boolean hasFocus) {
        if (mNativeWebContentsAndroid == null) return;
        WebContentsImplJni.get().setFocus(mNativeWebContentsAndroid, hasFocus);
    }

    @Override
    public void setDisplayCutoutSafeArea(Rect insets) {
        if (mNativeWebContentsAndroid == null) return;
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
        if (mNativeWebContentsAndroid == null) return;
        WebContentsImplJni.get().notifyRendererPreferenceUpdate(mNativeWebContentsAndroid);
    }

    @Override
    public void notifyBrowserControlsHeightChanged() {
        if (mNativeWebContentsAndroid == null) return;
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
        if (mNativeWebContentsAndroid == null) return false;
        return WebContentsImplJni.get()
                .needToFireBeforeUnloadOrUnloadEvents(mNativeWebContentsAndroid);
    }

    @Override
    public boolean needToFireBeforeUnload() {
        if (mNativeWebContentsAndroid == null) return false;
        return WebContentsImplJni.get().needToFireBeforeUnload(mNativeWebContentsAndroid);
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
    public @Nullable BackForwardTransitionAnimationManager
            getBackForwardTransitionAnimationManager() {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .getBackForwardTransitionAnimationManager(mNativeWebContentsAndroid);
    }

    @Override
    public void setLongPressLinkSelectText(boolean enabled) {
        checkNotDestroyed();
        WebContentsImplJni.get().setLongPressLinkSelectText(mNativeWebContentsAndroid, enabled);
    }

    @Override
    public void setCanAcceptLoadDrops(boolean enabled) {
        checkNotDestroyed();
        WebContentsImplJni.get().setCanAcceptLoadDrops(mNativeWebContentsAndroid, enabled);
    }

    @Override
    public boolean getCanAcceptLoadDropsForTesting() {
        checkNotDestroyed();
        return WebContentsImplJni.get()
                .getCanAcceptLoadDropsForTesting(mNativeWebContentsAndroid); // IN-TEST
    }

    @Override
    public void updateOffsetTagDefinitions(
            BrowserControlsOffsetTagDefinitions offsetTagDefinitions) {
        if (mNativeWebContentsAndroid == null) return;
        WebContentsImplJni.get()
                .updateOffsetTagDefinitions(mNativeWebContentsAndroid, offsetTagDefinitions);
    }

    @EnsuresNonNull("mNativeWebContentsAndroid")
    private void checkNotDestroyed() {
        if (mNativeWebContentsAndroid != null) return;
        throw new IllegalStateException(
                "Native WebContents already destroyed", mNativeDestroyThrowable);
    }

    @Override
    public void captureContentAsBitmapForTesting(Callback<Bitmap> callback) {
        WebContentsImplJni.get()
                .captureContentAsBitmapForTesting(
                        assumeNonNull(mNativeWebContentsAndroid), callback);
    }

    @Override
    public void setSupportsForwardTransitionAnimation(boolean supports) {
        WebContentsImplJni.get()
                .setSupportsForwardTransitionAnimation(
                        assumeNonNull(mNativeWebContentsAndroid), supports);
    }

    @Override
    public boolean hasOpener() {
        return WebContentsImplJni.get().hasOpener(assumeNonNull(mNativeWebContentsAndroid));
    }

    @Override
    public int getOriginalWindowOpenDisposition() {
        return WebContentsImplJni.get()
                .getOriginalWindowOpenDisposition(assumeNonNull(mNativeWebContentsAndroid));
    }

    @Override
    public void updateWindowControlsOverlay(Rect rect) {
        WebContentsImplJni.get()
                .updateWindowControlsOverlay(
                        assumeNonNull(mNativeWebContentsAndroid),
                        rect.left,
                        rect.top,
                        rect.right,
                        rect.bottom);
    }

    @Override
    public void setSupportsDraggableRegions(boolean supportsDraggableRegions) {
        WebContentsImplJni.get()
                .setSupportsDraggableRegions(
                        assumeNonNull(mNativeWebContentsAndroid), supportsDraggableRegions);
    }

    @Override
    public @Nullable WebContents getDocumentPictureInPictureOpener() {
        return WebContentsImplJni.get()
                .getDocumentPictureInPictureOpener(assumeNonNull(mNativeWebContentsAndroid));
    }

    /*package*/ @Nullable WebContentsObserverProxy getWebContentsObserverProxy() {
        return mObserverProxy;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {

        // This is static to avoid exposing a public destroy method on the native side of this
        // class.
        void destroyWebContents(JniPtr<NativeWebContentsAndroid> webContentsAndroid);

        void clearNativeReference(JniPtr<NativeWebContentsAndroid> self);

        @Nullable WindowAndroid getTopLevelNativeWindow(JniPtr<NativeWebContentsAndroid> self);

        void setTopLevelNativeWindow(
                JniPtr<NativeWebContentsAndroid> self, @Nullable WindowAndroid windowAndroid);

        RenderFrameHost getMainFrame(JniPtr<NativeWebContentsAndroid> self);

        RenderFrameHost getFocusedFrame(JniPtr<NativeWebContentsAndroid> self);

        boolean isFocusedElementEditable(JniPtr<NativeWebContentsAndroid> self);

        RenderFrameHost getRenderFrameHostFromId(
                JniPtr<NativeWebContentsAndroid> self, int renderProcessId, int renderFrameId);

        RenderFrameHost[] getAllRenderFrameHosts(JniPtr<NativeWebContentsAndroid> self);

        RenderWidgetHostViewImpl getRenderWidgetHostView(JniPtr<NativeWebContentsAndroid> self);

        @Visibility
        int getVisibility(JniPtr<NativeWebContentsAndroid> self);

        void updateWebContentsVisibility(JniPtr<NativeWebContentsAndroid> self, int visibility);

        String getTitle(JniPtr<NativeWebContentsAndroid> self);

        GURL getVisibleURL(JniPtr<NativeWebContentsAndroid> self);

        int getVirtualKeyboardMode(JniPtr<NativeWebContentsAndroid> self);

        String getEncoding(JniPtr<NativeWebContentsAndroid> self);

        void discard(
                JniPtr<NativeWebContentsAndroid> self,
                @JniType("base::OnceClosure") Runnable onDiscarded);

        boolean isLoading(JniPtr<NativeWebContentsAndroid> self);

        boolean shouldShowLoadingUI(JniPtr<NativeWebContentsAndroid> self);

        boolean hasUncommittedNavigationInPrimaryMainFrame(JniPtr<NativeWebContentsAndroid> self);

        void dispatchBeforeUnload(JniPtr<NativeWebContentsAndroid> self, boolean autoCancel);

        void stop(JniPtr<NativeWebContentsAndroid> self);

        void cut(JniPtr<NativeWebContentsAndroid> self);

        void copy(JniPtr<NativeWebContentsAndroid> self);

        void paste(JniPtr<NativeWebContentsAndroid> self);

        void pasteAsPlainText(JniPtr<NativeWebContentsAndroid> self);

        void replace(JniPtr<NativeWebContentsAndroid> self, String word);

        void selectAll(JniPtr<NativeWebContentsAndroid> self);

        void collapseSelection(JniPtr<NativeWebContentsAndroid> self);

        void setPrimaryPageImportance(
                JniPtr<NativeWebContentsAndroid> self,
                int mainFrameImportance,
                int subframeImportance);

        void suspendAllMediaPlayers(JniPtr<NativeWebContentsAndroid> self);

        void setAudioMuted(JniPtr<NativeWebContentsAndroid> self, boolean mute);

        boolean isAudioMuted(JniPtr<NativeWebContentsAndroid> self);

        boolean focusLocationBarByDefault(JniPtr<NativeWebContentsAndroid> self);

        boolean isFullscreenForCurrentTab(JniPtr<NativeWebContentsAndroid> self);

        void exitFullscreen(JniPtr<NativeWebContentsAndroid> self);

        void scrollFocusedEditableNodeIntoView(JniPtr<NativeWebContentsAndroid> self);

        void selectAroundCaret(
                JniPtr<NativeWebContentsAndroid> self,
                int granularity,
                boolean shouldShowHandle,
                boolean shouldShowContextMenu,
                int startOffset,
                int endOffset,
                int surroundingTextLength);

        void adjustSelectionByCharacterOffset(
                JniPtr<NativeWebContentsAndroid> self,
                int startAdjust,
                int endAdjust,
                boolean showSelectionMenu);

        GURL getLastCommittedURL(JniPtr<NativeWebContentsAndroid> self);

        boolean isIncognito(JniPtr<NativeWebContentsAndroid> self);

        void resumeLoadingCreatedWebContents(JniPtr<NativeWebContentsAndroid> self);

        void evaluateJavaScript(
                JniPtr<NativeWebContentsAndroid> self,
                String script,
                @Nullable JavaScriptCallback callback);

        void evaluateJavaScriptForTests(
                JniPtr<NativeWebContentsAndroid> self,
                String script,
                @Nullable JavaScriptCallback callback);

        void addMessageToDevToolsConsole(
                JniPtr<NativeWebContentsAndroid> self, int level, String message);

        void postMessageToMainFrame(
                JniPtr<NativeWebContentsAndroid> self,
                MessagePayload payload,
                @Nullable String sourceOrigin,
                String targetOrigin,
                MessagePort @Nullable [] ports);

        boolean hasAccessedInitialDocument(JniPtr<NativeWebContentsAndroid> self);

        boolean hasViewTransitionOptIn(JniPtr<NativeWebContentsAndroid> self);

        int getThemeColor(JniPtr<NativeWebContentsAndroid> self);

        int getBackgroundColor(JniPtr<NativeWebContentsAndroid> self);

        float getLoadProgress(JniPtr<NativeWebContentsAndroid> self);

        void requestSmartClipExtract(
                JniPtr<NativeWebContentsAndroid> self,
                SmartClipCallback callback,
                int x,
                int y,
                int width,
                int height);

        void requestAccessibilitySnapshot(
                JniPtr<NativeWebContentsAndroid> self,
                ViewStructure viewStructureRoot,
                ViewStructureBuilder viewStructureBuilder,
                @JniType("base::OnceClosure") Runnable doneCallback);

        void setOverscrollRefreshHandler(
                JniPtr<NativeWebContentsAndroid> self,
                @Nullable OverscrollRefreshHandler nativeOverscrollRefreshHandler);

        void setSpatialNavigationDisabled(JniPtr<NativeWebContentsAndroid> self, boolean disabled);

        void setStylusHandwritingEnabled(JniPtr<NativeWebContentsAndroid> self, boolean enabled);

        int downloadImage(
                JniPtr<NativeWebContentsAndroid> self,
                GURL url,
                boolean isFavicon,
                int maxBitmapSize,
                boolean bypassCache,
                ImageDownloadCallback callback);

        void setHasPersistentVideo(JniPtr<NativeWebContentsAndroid> self, boolean value);

        boolean hasActiveEffectivelyFullscreenVideo(JniPtr<NativeWebContentsAndroid> self);

        boolean isPictureInPictureAllowedForFullscreenVideo(JniPtr<NativeWebContentsAndroid> self);

        Rect getFullscreenVideoSize(JniPtr<NativeWebContentsAndroid> self);

        void setSize(JniPtr<NativeWebContentsAndroid> self, int width, int height);

        int getWidth(JniPtr<NativeWebContentsAndroid> self);

        int getHeight(JniPtr<NativeWebContentsAndroid> self);

        boolean isBeingCaptured(JniPtr<NativeWebContentsAndroid> self);

        EventForwarder getOrCreateEventForwarder(JniPtr<NativeWebContentsAndroid> self);

        void setViewAndroidDelegate(
                JniPtr<NativeWebContentsAndroid> self, ViewAndroidDelegate viewDelegate);

        void sendOrientationChangeEvent(JniPtr<NativeWebContentsAndroid> self, int orientation);

        void onScaleFactorChanged(JniPtr<NativeWebContentsAndroid> self);

        void setFocus(JniPtr<NativeWebContentsAndroid> self, boolean focused);

        void setDisplayCutoutSafeArea(
                JniPtr<NativeWebContentsAndroid> self, int top, int left, int bottom, int right);

        void notifyRendererPreferenceUpdate(JniPtr<NativeWebContentsAndroid> self);

        void notifyBrowserControlsHeightChanged(JniPtr<NativeWebContentsAndroid> self);

        boolean isBeingDestroyed(JniPtr<NativeWebContentsAndroid> self);

        boolean needToFireBeforeUnloadOrUnloadEvents(JniPtr<NativeWebContentsAndroid> self);

        boolean needToFireBeforeUnload(JniPtr<NativeWebContentsAndroid> self);

        @JniType("content::BackForwardTransitionAnimationManagerAndroid*")
        @Nullable BackForwardTransitionAnimationManager getBackForwardTransitionAnimationManager(
                JniPtr<NativeWebContentsAndroid> self);

        void setLongPressLinkSelectText(JniPtr<NativeWebContentsAndroid> self, boolean enabled);

        void setCanAcceptLoadDrops(JniPtr<NativeWebContentsAndroid> self, boolean enabled);

        boolean getCanAcceptLoadDropsForTesting(JniPtr<NativeWebContentsAndroid> self);

        void updateOffsetTagDefinitions(
                JniPtr<NativeWebContentsAndroid> self,
                BrowserControlsOffsetTagDefinitions offsetTagDefinitions);

        void captureContentAsBitmapForTesting(
                JniPtr<NativeWebContentsAndroid> self, Callback<Bitmap> callback);

        void setSupportsForwardTransitionAnimation(
                JniPtr<NativeWebContentsAndroid> self, boolean enabled);

        boolean hasOpener(JniPtr<NativeWebContentsAndroid> self);

        WebContents getDocumentPictureInPictureOpener(JniPtr<NativeWebContentsAndroid> self);

        int getOriginalWindowOpenDisposition(JniPtr<NativeWebContentsAndroid> self);

        void updateWindowControlsOverlay(
                JniPtr<NativeWebContentsAndroid> self, int left, int top, int right, int bottom);

        void setSupportsDraggableRegions(
                JniPtr<NativeWebContentsAndroid> self, boolean supportsDraggableRegions);
    }
}
