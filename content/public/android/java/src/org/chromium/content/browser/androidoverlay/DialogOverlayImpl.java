// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import android.content.Context;
import android.os.IBinder;
import android.view.Surface;
import android.view.View;
import android.view.ViewTreeObserver;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.gfx.mojom.Rect;
import org.chromium.media.mojom.AndroidOverlay;
import org.chromium.media.mojom.AndroidOverlayClient;
import org.chromium.media.mojom.AndroidOverlayConfig;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;

/**
 * Default AndroidOverlay impl.  Uses a separate (shared) overlay thread to own a Dialog instance,
 * probably via a separate object that operates only on that thread.  We will post messages to /
 * from that thread from the UI thread.
 */
@JNINamespace("content")
public class DialogOverlayImpl
        implements AndroidOverlay, DialogOverlayCore.Host, ViewTreeObserver.OnPreDrawListener {
    private static final String TAG = "DialogOverlayImpl";

    private AndroidOverlayClient mClient;
    // Runnable that we'll run when the overlay notifies us that it's been released.
    private Runnable mReleasedRunnable;

    private DialogOverlayCore mDialogCore;

    private long mNativeHandle;

    // If nonzero, then we have registered a surface with this ID.
    private int mSurfaceId;

    // Has close() been run yet?
    private boolean mClosed;

    // The last rect passed to scheduleLayout().
    private Rect mLastRect;

    // Observes the container view to update our location.
    private ViewTreeObserver mContainerViewViewTreeObserver;

    private final AndroidOverlayConfig mConfig;
    private final boolean mAsPanel;

    // The handler will be notified when the surface will be destroyed soon. We'll
    // notify the client to cleanup tasks on the surface, because the surface may be
    // destroyed before SurfaceHolder.Callback2.surfaceDestroyed returns.
    private final Runnable mTearDownDialogOverlaysHandler = this::onOverlayDestroyed;
    private WebContentsImpl mWebContents;

    /**
     * @param client Mojo client interface.
     * @param config initial overlay configuration.
     * @param provider the overlay provider that owns us.
     * @param asPanel the overlay should be a panel, above the compositor.  This is for testing.
     */
    public DialogOverlayImpl(
            AndroidOverlayClient client,
            final AndroidOverlayConfig config,
            Runnable releasedRunnable,
            final boolean asPanel) {
        ThreadUtils.assertOnUiThread();

        mClient = client;
        mReleasedRunnable = releasedRunnable;
        mLastRect = copyRect(config.rect);
        mConfig = config;
        mAsPanel = asPanel;

        // Register to get token updates.  Note that this may not call us back directly, since
        // |mDialogCore| hasn't been initialized yet.
        mNativeHandle =
                DialogOverlayImplJni.get()
                        .init(
                                DialogOverlayImpl.this,
                                config.routingToken.high,
                                config.routingToken.low,
                                config.powerEfficient);

        if (mNativeHandle == 0) {
            notifyDestroyed();
            cleanup();
            return;
        }

        DialogOverlayImplJni.get()
                .getCompositorOffset(mNativeHandle, DialogOverlayImpl.this, config.rect);
        DialogOverlayImplJni.get().completeInit(mNativeHandle, DialogOverlayImpl.this);
    }

    // AndroidOverlay impl.
    // Client is done with this overlay.
    @Override
    public void close() {
        ThreadUtils.assertOnUiThread();

        if (mClosed) return;

        mClosed = true;

        // TODO(liberato): verify that this actually works, else add an explicit shutdown and hope
        // that the client calls it.

        // Notify |mDialogCore| that it has been released.
        if (mDialogCore != null) {
            mDialogCore.release();

            // Note that we might get messagaes from |mDialogCore| after this, since they might be
            // dispatched before |r| arrives.  Clearing |mDialogCore| causes us to ignore them.
            cleanup();
        }

        // Notify the provider that we've been released by the client.  Note that the surface might
        // not have been destroyed yet, but that's okay.  We could wait for a callback from the
        // dialog core before proceeding, but this makes it easier for the client to destroy and
        // re-create an overlay without worrying about an intermittent failure due to having too
        // many overlays open at once.
        mReleasedRunnable.run();
    }

    // AndroidOverlay impl.
    @Override
    public void onConnectionError(MojoException e) {
        ThreadUtils.assertOnUiThread();

        close();
    }

    // AndroidOverlay impl.
    @Override
    public void scheduleLayout(final Rect rect) {
        ThreadUtils.assertOnUiThread();

        mLastRect = copyRect(rect);

        if (mDialogCore == null) return;

        // |rect| is relative to the compositor surface.  Convert it to be relative to the screen.
        DialogOverlayImplJni.get().getCompositorOffset(mNativeHandle, DialogOverlayImpl.this, rect);
        mDialogCore.layoutSurface(rect);
    }

    // Receive the compositor offset, as part of scheduleLayout.  Adjust the layout position.
    @CalledByNative
    private static void receiveCompositorOffset(Rect rect, int x, int y) {
        rect.x += x;
        rect.y += y;
    }

    // DialogOverlayCore.Host impl.
    // |surface| is now ready.  Register it with the surface tracker, and notify the client.
    @Override
    public void onSurfaceReady(Surface surface) {
        ThreadUtils.assertOnUiThread();

        if (mDialogCore == null || mClient == null) return;

        mSurfaceId = DialogOverlayImplJni.get().registerSurface(surface);
        mClient.onSurfaceReady(mSurfaceId);
    }

    // DialogOverlayCore.Host impl.
    @Override
    public void onOverlayDestroyed() {
        ThreadUtils.assertOnUiThread();

        if (mDialogCore == null) return;

        // Notify the client that the overlay is gone.
        notifyDestroyed();

        // Also clear out |mDialogCore| to prevent us from sending useless messages to it.
        cleanup();

        // Note that we don't notify |mReleasedRunnable| yet, though we could.  We wait for the
        // client to close their connection first.
    }

    // ViewTreeObserver.OnPreDrawListener implementation.
    @Override
    public boolean onPreDraw() {
        scheduleLayout(mLastRect);
        return true;
    }

    /** Callback from native that the window has changed. */
    @CalledByNative
    public void onWindowAndroid(final WindowAndroid window) {
        ThreadUtils.assertOnUiThread();

        if (mDialogCore == null) {
            initializeDialogCore(window);
            return;
        }

        // Forward this change.
        // Note that if we don't have a window token, then we could wait until we do, simply by
        // skipping sending null if we haven't sent any non-null token yet.  If we're transitioning
        // between windows, that might make the client's job easier. It wouldn't have to guess when
        // a new token is available.
        IBinder token = window != null ? window.getWindowToken() : null;
        mDialogCore.onWindowToken(token);
    }

    @CalledByNative
    private void observeContainerView(View containerView) {
        if (mContainerViewViewTreeObserver != null && mContainerViewViewTreeObserver.isAlive()) {
            mContainerViewViewTreeObserver.removeOnPreDrawListener(this);
        }
        mContainerViewViewTreeObserver = null;

        if (containerView != null) {
            mContainerViewViewTreeObserver = containerView.getViewTreeObserver();
            mContainerViewViewTreeObserver.addOnPreDrawListener(this);
        }
    }

    /** Callback from native that we will be getting no additional tokens. */
    @CalledByNative
    public void onDismissed() {
        ThreadUtils.assertOnUiThread();

        // Notify the client that the overlay is going away.
        notifyDestroyed();

        // Notify |mDialogCore| that it lost the token, if it had one.
        if (mDialogCore != null) mDialogCore.onWindowToken(null);

        cleanup();
    }

    /** Callback from native to tell us that the power-efficient state has changed. */
    @CalledByNative
    private void onPowerEfficientState(boolean isPowerEfficient) {
        ThreadUtils.assertOnUiThread();
        if (mDialogCore == null) return;
        if (mClient == null) return;
        mClient.onPowerEfficientState(isPowerEfficient);
    }

    /**
     * Callback from the native to provide the WebContents. It should be called inside completeInit.
     */
    @CalledByNative
    private void onWebContents(WebContentsImpl webContents) {
        assert mWebContents == null;
        assert webContents != null;

        mWebContents = webContents;
        mWebContents.addTearDownDialogOverlaysHandler(mTearDownDialogOverlaysHandler);
    }

    /** Initialize |mDialogCore| when the window is available. */
    private void initializeDialogCore(WindowAndroid window) {
        ThreadUtils.assertOnUiThread();

        if (window == null) return;

        Context context = window.getContext().get();
        if (ContextUtils.activityFromContext(context) == null) return;

        mDialogCore = new DialogOverlayCore();
        mDialogCore.initialize(context, mConfig, DialogOverlayImpl.this, mAsPanel);
        mDialogCore.onWindowToken(window.getWindowToken());
    }

    /**
     * Unregister for callbacks, unregister any surface that we have, and forget about
     * |mDialogCore|.  Multiple calls are okay.
     */
    private void cleanup() {
        ThreadUtils.assertOnUiThread();

        if (mSurfaceId != 0) {
            DialogOverlayImplJni.get().unregisterSurface(mSurfaceId);
            mSurfaceId = 0;
        }

        // Note that we might not be registered for a token.
        if (mNativeHandle != 0) {
            DialogOverlayImplJni.get().destroy(mNativeHandle, DialogOverlayImpl.this);
            mNativeHandle = 0;
        }

        // Also clear out |mDialogCore| to prevent us from sending useless messages to it.  Note
        // that we might have already sent useless messages to it, and it should be robust against
        // that sort of thing.
        mDialogCore = null;

        // If we wanted to send any message to |mClient|, we should have done so already.
        // We close |mClient| first to prevent leaking the mojo router object.
        if (mClient != null) mClient.close();
        mClient = null;

        // Native should have cleaned up the container view before we reach this.
        assert mContainerViewViewTreeObserver == null;

        if (mWebContents != null) {
            mWebContents.removeTearDownDialogOverlaysHandler(mTearDownDialogOverlaysHandler);
            mWebContents = null;
        }
    }

    private void notifyDestroyed() {
        if (mClient == null) return;

        // This is the last message to the client.
        final AndroidOverlayClient client = mClient;
        mClient = null;

        // If we've not provided a surface, then we don't need to wait for a reply.  This happens,
        // for example, if we fail immediately.
        if (mSurfaceId == 0) {
            client.onDestroyed();
            return;
        }

        // Notify the client that the overlay is gone, synchronously.  We have to do this once we
        // have a Surface, since we could get a surfaceDestroyed from Android at any time.  If we
        // signal async destruction, then get surfaceDestroyed, then we're stuck.  So, clean up
        // synchronously even if Android is not waiting for us right now.

        // Don't try this at home.  It's hacky.  All of DialogOverlay is deprecated.  It will be
        // removed once Android O is no longer supported.
        final AndroidOverlayClient.Proxy proxy = (AndroidOverlayClient.Proxy) client;
        final MessagePipeHandle handle = proxy.getProxyHandler().passHandle();
        final long nativeHandle = handle.releaseNativeHandle();
        DialogOverlayImplJni.get().notifyDestroyedSynchronously(nativeHandle);
    }

    /** Creates a copy of |rect| and returns it. */
    private static Rect copyRect(Rect rect) {
        Rect copy = new Rect();
        copy.x = rect.x;
        copy.y = rect.y;
        copy.width = rect.width;
        copy.height = rect.height;
        return copy;
    }

    @NativeMethods
    interface Natives {
        /**
         * Initializes native side.  Will register for onWindowToken callbacks on |this|.  Returns a
         * handle that should be provided to nativeDestroy.  This will not call back with a window
         * token immediately.  Call nativeCompleteInit() for the initial token.
         */
        long init(DialogOverlayImpl caller, long high, long low, boolean isPowerEfficient);

        void completeInit(long nativeDialogOverlayImpl, DialogOverlayImpl caller);

        /** Stops native side and deallocates |handle|. */
        void destroy(long nativeDialogOverlayImpl, DialogOverlayImpl caller);

        /**
         * Calls back ReceiveCompositorOffset with the screen location (in the
         * View.getLocationOnScreen sense) of the compositor for our WebContents.  Sends |rect|
         * along verbatim.
         */
        void getCompositorOffset(long nativeDialogOverlayImpl, DialogOverlayImpl caller, Rect rect);

        /**
         * Register a surface and return the surface id for it.
         * @param surface Surface that we should register.
         * @return surface id that we associated with |surface|.
         */
        int registerSurface(Surface surface);

        /**
         * Unregister a surface.
         * @param surfaceId Id that was returned by registerSurface.
         */
        void unregisterSurface(int surfaceId);

        /**
         * Look up and return a surface.
         * @param surfaceId Id that was returned by registerSurface.
         */
        Surface lookupSurfaceForTesting(int surfaceId);

        /**
         * Send a synchronous OnDestroyed message to the client. Closes the message pipe.
         *
         * @param messagePipe Mojo message pipe ID.
         * @param version Mojo interface version.
         */
        void notifyDestroyedSynchronously(long messagePipeHandle);
    }
}
