// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.Surface;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/**
 * Provides static methods called by the XrDelegateImpl as well as JNI methods to the C/C++ code
 * in order to interact with the various bits of the Java side of a session. This includes the
 * responsibility to standup/create any needed overlays/SurfaceViews and forwarding events both
 * from them and elsewhere within Chrome (forwarded/registered for via XrDelegate). This class is
 * also responsible for ensuring that there is only one active session at a time and answering
 * questions about that session; mainly via communication of its static members.
 */
@JNINamespace("webxr")
public class XrSessionCoordinator {
    private static final String TAG = "XrSessionCoordinator";
    private static final boolean DEBUG_LOGS = false;

    @IntDef({SessionType.NONE, SessionType.AR, SessionType.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SessionType {
        int NONE = 0;
        int AR = 1;
        int VR = 2;
    }

    // ArDelegateImpl needs to know if there's an active immersive session so that it can handle
    // back button presses from ChromeActivity's onBackPressed(). It's only set while a session is
    // in progress, and reset to null on session end. The XrImmersiveOverlay member has a strong
    // reference to the ChromeActivity, and that shouldn't be retained beyond the duration of a
    // session.
    private static XrSessionCoordinator sActiveSessionInstance;

    /** Whether there is a non-null valid {@link #sActiveSessionInstance}. */
    private static XrSessionTypeSupplier sActiveSessionAvailableSupplier =
            new XrSessionTypeSupplier(SessionType.NONE);

    private long mNativeXrSessionCoordinator;

    // The native ArCoreDevice runtime creates a XrSessionCoordinator instance in its constructor,
    // and keeps a strong reference to it for the lifetime of the device. It creates and
    // owns an XrImmersiveOverlay for the duration of an immersive session, which in
    // turn contains a reference to XrSessionCoordinator for making JNI calls back to the device.
    private XrImmersiveOverlay mImmersiveOverlay;

    private @SessionType int mActiveSessionType = SessionType.NONE;

    // The WebContents that triggered the currently active session.
    private WebContents mWebContents;

    private WeakReference<Activity> mXrHostActivity;

    // Helper, obtains android Activity out of passed in WebContents instance.
    // Equivalent to ChromeActivity.fromWebContents(), but does not require that
    // the resulting instance is a ChromeActivity.
    @CalledByNative
    public static Activity getActivity(final WebContents webContents) {
        if (webContents == null) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getActivity().get();
    }

    @CalledByNative
    private static XrSessionCoordinator create(long nativeXrSessionCoordinator) {
        ThreadUtils.assertOnUiThread();
        return new XrSessionCoordinator(nativeXrSessionCoordinator);
    }

    /**
     * Gets the Activity of the currently active Session as a Context. Will return null if not
     * called between startSession and endSession.
     *
     * @return Context The current activity as a Context.
     */
    @CalledByNative
    private static Context getCurrentActivityContext() {
        if (sActiveSessionInstance == null || sActiveSessionInstance.mWebContents == null) {
            return null;
        }

        return getActivity(sActiveSessionInstance.mWebContents);
    }

    /**
     * Gets the current application context. May not correspond to an activity, and would not be
     * suitable for calls requiring such.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    private XrSessionCoordinator(long nativeXrSessionCoordinator) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "constructor, nativeXrSessionCoordinator=" + nativeXrSessionCoordinator);
        }
        mNativeXrSessionCoordinator = nativeXrSessionCoordinator;
    }

    private void startSession(
            @SessionType int sessionType,
            XrImmersiveOverlay.Delegate overlayDelegate,
            final WebContents webContents) {
        assert (sActiveSessionInstance == null);
        assert (sessionType != SessionType.NONE);

        mImmersiveOverlay = new XrImmersiveOverlay();
        mImmersiveOverlay.show(overlayDelegate, webContents, this);

        mWebContents = webContents;
        mActiveSessionType = sessionType;
        sActiveSessionInstance = this;
        sActiveSessionAvailableSupplier.set(sessionType);
    }

    @CalledByNative
    private void startArSession(
            final ArCompositorDelegateProvider compositorDelegateProvider,
            final WebContents webContents,
            boolean useOverlay,
            boolean canRenderDomContent) {
        if (DEBUG_LOGS) Log.i(TAG, "startArSession");
        // The higher levels should have guaranteed that we're only called if there isn't any other
        // active session going on.
        assert (sActiveSessionInstance == null);

        XrImmersiveOverlay.Delegate overlayDelegate =
                ArClassProvider.getOverlayDelegate(
                        compositorDelegateProvider.create(webContents),
                        webContents,
                        useOverlay,
                        canRenderDomContent);
        startSession(SessionType.AR, overlayDelegate, webContents);
    }

    @CalledByNative
    private void startVrSession(
            final VrCompositorDelegateProvider compositorDelegateProvider,
            final WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "startVrSession");
        // The higher levels should have guaranteed that we're only called if there isn't any other
        // active session going on.
        assert (sActiveSessionInstance == null);

        XrImmersiveOverlay.Delegate overlayDelegate =
                CardboardClassProvider.getOverlayDelegate(
                        compositorDelegateProvider.create(webContents), getActivity(webContents));
        startSession(SessionType.VR, overlayDelegate, webContents);
    }

    @CalledByNative
    private void startXrSession() {
        if (DEBUG_LOGS) Log.i(TAG, "startXrSession");
        // The higher levels should have guaranteed that we're only called if there isn't any other
        // active session going on.
        assert (sActiveSessionInstance == null);

        // The active session must be set before creating the host activity, since it will be
        // notified once the activity is ready.
        sActiveSessionInstance = this;
        mActiveSessionType = SessionType.VR;
        sActiveSessionAvailableSupplier.set(SessionType.VR);

        Intent intent = XrHostActivity.createIntent(getApplicationContext());
        getApplicationContext().startActivity(intent);
    }

    private void endSessionFromXrHost() {
        if (DEBUG_LOGS) Log.i(TAG, "endSessionFromXrHost");

        if (sActiveSessionInstance == null) return;
        assert (sActiveSessionInstance == this);

        // Since the XrHostActivity is removing us we don't need to clean it up, so null it out now.
        mXrHostActivity = null;
        endSession();
    }

    @CalledByNative
    private void endSession() {
        if (DEBUG_LOGS) Log.i(TAG, "endSession");

        if (sActiveSessionInstance == null) return;
        assert (sActiveSessionInstance == this);

        if (mImmersiveOverlay != null) {
            mImmersiveOverlay.cleanupAndExit();
            mImmersiveOverlay = null;
        } else {
            onJavaShutdown();
        }

        mActiveSessionType = SessionType.NONE;
        mWebContents = null;
        sActiveSessionInstance = null;
        sActiveSessionAvailableSupplier.set(SessionType.NONE);
        if (mXrHostActivity != null && mXrHostActivity.get() != null) {
            mXrHostActivity.get().finish();
            mXrHostActivity = null;
        }
    }

    // Called from XrDelegateImpl and XRHostActivity
    public static boolean endActiveSession() {
        if (DEBUG_LOGS) Log.i(TAG, "endActiveSession");
        // If there's an active immersive session shut it down and return true so that the caller
        // can take appropriate action, such as consuming a back gesture.
        if (sActiveSessionInstance != null) {
            sActiveSessionInstance.endSession();
            return true;
        }
        return false;
    }

    // Called from XrDelegateImpl and XRHostActivity
    public static boolean endActiveSessionFromXrHost() {
        if (DEBUG_LOGS) Log.i(TAG, "endActiveSessionFromXrHost");
        // If there's an active immersive session shut it down and return true so that the caller
        // can take appropriate action, such as consuming a back gesture.
        if (sActiveSessionInstance != null) {
            sActiveSessionInstance.endSessionFromXrHost();
            return true;
        }
        return false;
    }

    public static boolean hasActiveSession() {
        return sActiveSessionInstance != null;
    }

    public static boolean hasActiveArSession() {
        return sActiveSessionInstance.mActiveSessionType == SessionType.AR;
    }

    public static XrSessionTypeSupplier getActiveSessionTypeSupplier() {
        return sActiveSessionAvailableSupplier;
    }

    public static void onActiveXrSessionButtonTouched() {
        sActiveSessionInstance.onXrSessionButtonTouched();
    }

    public void onDrawingSurfaceReady(
            Surface surface, WindowAndroid rootWindow, int rotation, int width, int height) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceReady");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get()
                .onDrawingSurfaceReady(
                        mNativeXrSessionCoordinator,
                        XrSessionCoordinator.this,
                        surface,
                        rootWindow,
                        rotation,
                        width,
                        height);
    }

    public static XrSessionCoordinator getActiveInstanceForTesting() {
        return sActiveSessionInstance;
    }

    public void onDrawingSurfaceTouch(
            boolean isPrimary, boolean isTouching, int pointerId, float x, float y) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceTouch");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get()
                .onDrawingSurfaceTouch(
                        mNativeXrSessionCoordinator,
                        XrSessionCoordinator.this,
                        isPrimary,
                        isTouching,
                        pointerId,
                        x,
                        y);
    }

    public void onDrawingSurfaceDestroyed() {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceDestroyed");
        onJavaShutdown();
    }

    private void onJavaShutdown() {
        if (DEBUG_LOGS) Log.i(TAG, "onJavaShutdown");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get()
                .onJavaShutdown(mNativeXrSessionCoordinator, XrSessionCoordinator.this);
    }

    public void onXrSessionButtonTouched() {
        if (DEBUG_LOGS) Log.i(TAG, "onXrSessionButtonTouched");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get()
                .onXrSessionButtonTouched(mNativeXrSessionCoordinator, XrSessionCoordinator.this);
    }

    /**
     * Called when an XrHostActivity has started and is ready to be passed as an argument to
     * xrCreateInstance().
     *
     * @return True if an active session was notified that the activity is ready.
     */
    public static boolean onXrHostActivityReady(Activity activity) {
        if (DEBUG_LOGS) Log.i(TAG, "onXrHostActivityReady");
        if (sActiveSessionInstance != null) {
            sActiveSessionInstance.handleXrHostActivityReady(activity);
            return true;
        }
        return false;
    }

    private void handleXrHostActivityReady(Activity activity) {
        if (mNativeXrSessionCoordinator == 0) return;
        mXrHostActivity = new WeakReference(activity);
        XrSessionCoordinatorJni.get()
                .onXrHostActivityReady(
                        mNativeXrSessionCoordinator, XrSessionCoordinator.this, activity);
    }

    @CalledByNative
    private void onNativeDestroy() {
        // Native destructors should end sessions before destroying the native XrSessionCoordinator
        // object.
        assert sActiveSessionInstance != this : "unexpected active session in onNativeDestroy";

        mNativeXrSessionCoordinator = 0;
    }

    @NativeMethods
    interface Natives {
        void onDrawingSurfaceReady(
                long nativeXrSessionCoordinator,
                XrSessionCoordinator caller,
                Surface surface,
                WindowAndroid rootWindow,
                int rotation,
                int width,
                int height);

        void onDrawingSurfaceTouch(
                long nativeXrSessionCoordinator,
                XrSessionCoordinator caller,
                boolean primary,
                boolean touching,
                int pointerId,
                float x,
                float y);

        void onJavaShutdown(long nativeXrSessionCoordinator, XrSessionCoordinator caller);

        void onXrSessionButtonTouched(long nativeXrSessionCoordinator, XrSessionCoordinator caller);

        void onXrHostActivityReady(
                long nativeXrSessionCoordinator, XrSessionCoordinator caller, Activity activity);
    }
}
