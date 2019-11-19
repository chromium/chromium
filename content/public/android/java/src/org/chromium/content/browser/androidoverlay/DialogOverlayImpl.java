// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import android.content.Context;
import android.os.Handler;
import android.os.IBinder;
import android.view.Surface;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.gfx.mojom.Rect;
import org.chromium.media.mojom.AndroidOverlay;
import org.chromium.media.mojom.AndroidOverlayClient;
import org.chromium.media.mojom.AndroidOverlayConfig;
import org.chromium.mojo.system.MojoException;

/**
 * Default AndroidOverlay impl.  Uses a separate (shared) overlay thread to own a Dialog instance,
 * probably via a separate object that operates only on that thread.  We will post messages to /
 * from that thread from the UI thread.
 */
@JNINamespace("content")
public class DialogOverlayImpl implements AndroidOverlay, DialogOverlayCore.Host {
    private static final String TAG = "DialogOverlayImpl";

    private AndroidOverlayClient mClient;
    private Handler mOverlayHandler;
    // Runnable that we'll run when the overlay notifies us that it's been released.
    private Runnable mReleasedRunnable;

    // Runnable that will release |mDialogCore| when posted to mOverlayHandler.  We keep this
    // separately from mDialogCore itself so that we can call it after we've discarded the latter.
    private Runnable mReleaseCoreRunnable;

    private final ThreadHoppingHost mHoppingHost;

    private DialogOverlayCore mDialogCore;

    private long mNativeHandle;

    // If nonzero, then we have registered a surface with this ID.
    private int mSurfaceId;

    // Has close() been run yet?
    private boolean mClosed;

    // Temporary, so we don't need to keep allocating arrays.
    private final int[] mCompositorOffset = new int[2];

    /**
     * @param client Mojo client interface.
     * @param config initial overlay configuration.
     * @param handler handler that posts to the overlay thread.  This is the android UI thread that
     * the dialog uses, not the browser UI thread.
     * @param provider the overlay provider that owns us.
     * @param asPanel the overlay should be a panel, above the compositor.  This is for testing.
     */
    public DialogOverlayImpl(AndroidOverlayClient client, final AndroidOverlayConfig config,
            Handler overlayHandler, Runnable releasedRunnable, final boolean asPanel) {
        ThreadUtils.assertOnUiThread();

        mClient = client;
        mReleasedRunnable = releasedRunnable;
        mOverlayHandler = overlayHandler;

        mDialogCore = new DialogOverlayCore();
        mHoppingHost = new ThreadHoppingHost(this);

        // Register to get token updates.  Note that this may not call us back directly, since
        // |mDialogCore| hasn't been initialized yet.
        mNativeHandle = DialogOverlayImplJni.get().init(DialogOverlayImpl.this,
                config.routingToken.high, config.routingToken.low, config.powerEfficient);

        if (mNativeHandle == 0) {
            mClient.onDestroyed();
            cleanup();
            return;
        }

        // Post init to the overlay thread.
        final DialogOverlayCore dialogCore = mDialogCore;
        final Context context = ContextUtils.getApplicationContext();
        DialogOverlayImplJni.get().getCompositorOffset(
                mNativeHandle, DialogOverlayImpl.this, config.rect);
        mOverlayHandler.post(new Runnable() {
            @Override
            public void run() {
                dialogCore.initialize(context, config, mHoppingHost, asPanel);
                // Now that |mDialogCore| has been initialized, we are ready for token callbacks.
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                    @Override
                    public void run() {
                        if (mNativeHandle != 0) {
                            DialogOverlayImplJni.get().completeInit(
                                    mNativeHandle, DialogOverlayImpl.this);
                        }
                    }
                });
            }
        });

        mReleaseCoreRunnable = new Runnable() {
            @Override
            public void run() {
                dialogCore.release();
            }
        };
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

        // Allow surfaceDestroyed to proceed, if it's waiting.
        mHoppingHost.onClose();

        // Notify |mDialogCore| that it has been released.
        if (mReleaseCoreRunnable != null) {
            mOverlayHandler.post(mReleaseCoreRunnable);
            mReleaseCoreRunnable = null;

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

        if (mDialogCore == null) return;

        // |rect| is relative to the compositor surface.  Convert it to be relative to the screen.
        DialogOverlayImplJni.get().getCompositorOffset(mNativeHandle, DialogOverlayImpl.this, rect);

        final DialogOverlayCore dialogCore = mDialogCore;
        mOverlayHandler.post(new Runnable() {
            @Override
            public void run() {
                dialogCore.layoutSurface(rect);
            }
        });
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
        if (mClient != null) mClient.onDestroyed();

        // Also clear out |mDialogCore| to prevent us from sending useless messages to it.  Note
        // that we might have already sent useless messages to it, and it should be robust against
        // that sort of thing.
        cleanup();

        // Note that we don't notify |mReleasedRunnable| yet, though we could.  We wait for the
        // client to close their connection first.
    }

    // DialogOverlayCore.Host impl.
    // Due to threading issues, |mHoppingHost| doesn't forward this.
    @Override
    public void waitForClose() {
        assert false : "Not reached";
    }

    // DialogOverlayCore.Host impl
    @Override
    public void enforceClose() {
        // Pretend that the client closed us, even if they didn't.  It's okay if this is called more
        // than once.  The client might have already called it, or might call it later.
        close();
    }

    /**
     * Send |token| to the |mDialogCore| on the overlay thread.
     */
    private void sendWindowTokenToCore(final IBinder token) {
        ThreadUtils.assertOnUiThread();

        if (mDialogCore != null) {
            final DialogOverlayCore dialogCore = mDialogCore;
            mOverlayHandler.post(new Runnable() {
                @Override
                public void run() {
                    dialogCore.onWindowToken(token);
                }
            });
        }
    }

    /**
     * Callback from native that the window token has changed.
     */
    @CalledByNative
    public void onWindowToken(final IBinder token) {
        ThreadUtils.assertOnUiThread();

        if (mDialogCore == null) return;

        // Forward this change.
        // Note that if we don't have a window token, then we could wait until we do, simply by
        // skipping sending null if we haven't sent any non-null token yet.  If we're transitioning
        // between windows, that might make the client's job easier. It wouldn't have to guess when
        // a new token is available.
        sendWindowTokenToCore(token);
    }

    /**
     * Callback from native that we will be getting no additional tokens.
     */
    @CalledByNative
    public void onDismissed() {
        ThreadUtils.assertOnUiThread();

        // Notify the client that the overlay is going away.
        if (mClient != null) mClient.onDestroyed();

        // Notify |mDialogCore| that it lost the token, if it had one.
        sendWindowTokenToCore(null);

        cleanup();
    }

    /**
     * Callback from native to tell us that the power-efficient state has changed.
     */
    @CalledByNative
    private void onPowerEfficientState(boolean isPowerEfficient) {
        ThreadUtils.assertOnUiThread();
        if (mDialogCore == null) return;
        if (mClient == null) return;
        mClient.onPowerEfficientState(isPowerEfficient);
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
    }

    /**
     * Notify the native side that we are ready for token / dismissed callbacks.  This may result in
     * a callback before it returns.
     */

    @NativeMethods
    interface Natives {
        /**
         * Initializes native side.  Will register for onWindowToken callbacks on |this|.  Returns a
         * handle that should be provided to nativeDestroy.  This will not call back with a window
         * token immediately.  Call nativeCompleteInit() for the initial token.
         */
        long init(DialogOverlayImpl caller, long high, long low, boolean isPowerEfficient);

        void completeInit(long nativeDialogOverlayImpl, DialogOverlayImpl caller);
        /**
         * Stops native side and deallocates |handle|.
         */
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
    }
}
