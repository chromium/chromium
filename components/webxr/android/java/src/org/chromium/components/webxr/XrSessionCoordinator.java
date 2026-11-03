// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;
import android.view.Surface;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.InputMethodManagerWrapper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.ref.WeakReference;

/**
 * Provides static methods called by the XrDelegateImpl as well as JNI methods to the C/C++ code in
 * order to interact with the various bits of the Java side of a session. This includes the
 * responsibility to standup/create any needed overlays/SurfaceViews and forwarding events both from
 * them and elsewhere within Chrome (forwarded/registered for via XrDelegate). This class is also
 * responsible for ensuring that there is only one active session at a time and answering questions
 * about that session; mainly via communication of its static members.
 */
@JNINamespace("webxr")
@NullMarked
public class XrSessionCoordinator {
    @JniType("::webxr::XrSessionCoordinator")
    interface NativeXrSessionCoordinator extends JniTypeToken {}

    private static final String TAG = "XrSessionCoordinator";
    private static final boolean DEBUG_LOGS = false;

    @IntDef({SessionType.NONE, SessionType.AR, SessionType.VR})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
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
    private static @Nullable XrSessionCoordinator sActiveSessionInstance;

    /** Whether there is a non-null valid {@link #sActiveSessionInstance}. */
    private static final SettableNonNullObservableSupplier<@SessionType Integer>
            sActiveSessionAvailableSupplier = ObservableSuppliers.createNonNull(SessionType.NONE);

    private @Nullable JniRawPtr<NativeXrSessionCoordinator> mNativeXrSessionCoordinator;

    // The native ArCoreDevice runtime creates a XrSessionCoordinator instance in its constructor,
    // and keeps a strong reference to it for the lifetime of the device. It creates and
    // owns an XrImmersiveOverlay for the duration of an immersive session, which in
    // turn contains a reference to XrSessionCoordinator for making JNI calls back to the device.
    private @Nullable XrImmersiveOverlay mImmersiveOverlay;

    private @SessionType int mActiveSessionType = SessionType.NONE;

    // The WebContents that triggered the currently active session.
    private @Nullable WebContents mWebContents;

    private @Nullable InputMethodManagerWrapper mOriginalInputMethodManagerWrapper;

    private @Nullable XrInputMethodManagerWrapper mXrInputMethodManagerWrapper;

    private @Nullable WeakReference<Activity> mXrHostActivity;

    // Helper, obtains android Activity out of passed in WebContents instance.
    // Equivalent to ChromeActivity.fromWebContents(), but does not require that
    // the resulting instance is a ChromeActivity.
    @CalledByNative
    public static @Nullable Activity getActivity(final WebContents webContents) {
        if (webContents == null) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getActivity().get();
    }

    @CalledByNative
    static XrSessionCoordinator create(
            JniRawPtr<NativeXrSessionCoordinator> nativeXrSessionCoordinator) {
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
    private static @Nullable Context getCurrentActivityContext() {
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

    private XrSessionCoordinator(JniRawPtr<NativeXrSessionCoordinator> nativeXrSessionCoordinator) {
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
        assumeNonNull(overlayDelegate);
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

        Activity activity = getActivity(webContents);
        assumeNonNull(activity);

        XrImmersiveOverlay.Delegate overlayDelegate =
                CardboardClassProvider.getOverlayDelegate(
                        compositorDelegateProvider.create(webContents), activity);
        assumeNonNull(overlayDelegate);
        startSession(SessionType.VR, overlayDelegate, webContents);
    }

    @CalledByNative
    private void startXrSession(final WebContents webContents, boolean needsSeparateActivity) {
        if (DEBUG_LOGS) Log.i(TAG, "startXrSession");
        // The higher levels should have guaranteed that we're only called if there isn't any other
        // active session going on.
        assert (sActiveSessionInstance == null);

        // The active session must be set before creating the host activity, since it will be
        // notified once the activity is ready.
        sActiveSessionInstance = this;
        mWebContents = webContents;
        mActiveSessionType = SessionType.VR;
        sActiveSessionAvailableSupplier.set(SessionType.VR);

        if (needsSeparateActivity) {
            Intent intent = XrHostActivity.createIntent(getApplicationContext());
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            getApplicationContext().startActivity(intent);
        } else {
            XrSessionCoordinatorJni.get()
                    .onXrHostActivityReady(
                            assumeNonNull(mNativeXrSessionCoordinator), getActivity(webContents));
        }
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

        // If we have a host activity, shut it down first. Once it actually enters `onStop` we'll
        // get called again, but this time since our activity is null we'll run the rest of the
        // function.
        if (mXrHostActivity != null && mXrHostActivity.get() != null) {
            mXrHostActivity.get().finish();
            mXrHostActivity = null;
            return;
        }

        restoreBrowserInput();

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
        assumeNonNull(sActiveSessionInstance);
        return sActiveSessionInstance.mActiveSessionType == SessionType.AR;
    }

    public static NonNullObservableSupplier<@SessionType Integer> getActiveSessionTypeSupplier() {
        return sActiveSessionAvailableSupplier;
    }

    public static void onActiveXrSessionButtonTouched() {
        assumeNonNull(sActiveSessionInstance);
        sActiveSessionInstance.onXrSessionButtonTouched();
    }

    public static boolean dispatchKeyEvent(KeyEvent event) {
        if (sActiveSessionInstance == null || sActiveSessionInstance.mWebContents == null) {
            return false;
        }

        Activity activity = getActivity(sActiveSessionInstance.mWebContents);
        if (activity == null) {
            return false;
        }

        return activity.dispatchKeyEvent(event);
    }

    public void onDrawingSurfaceReady(
            Surface surface, WindowAndroid rootWindow, int rotation, int width, int height) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceReady");
        if (mNativeXrSessionCoordinator == null) return;
        XrSessionCoordinatorJni.get()
                .onDrawingSurfaceReady(
                        assumeNonNull(mNativeXrSessionCoordinator),
                        surface,
                        rootWindow,
                        rotation,
                        width,
                        height);
    }

    public static @Nullable XrSessionCoordinator getActiveInstanceForTesting() {
        return sActiveSessionInstance;
    }

    public void onDrawingSurfaceTouch(
            boolean isPrimary, boolean isTouching, int pointerId, float x, float y) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceTouch");
        if (mNativeXrSessionCoordinator == null) return;
        XrSessionCoordinatorJni.get()
                .onDrawingSurfaceTouch(
                        assumeNonNull(mNativeXrSessionCoordinator),
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
        if (mNativeXrSessionCoordinator == null) return;
        XrSessionCoordinatorJni.get().onJavaShutdown(assumeNonNull(mNativeXrSessionCoordinator));
    }

    public void onXrSessionButtonTouched() {
        if (DEBUG_LOGS) Log.i(TAG, "onXrSessionButtonTouched");
        if (mNativeXrSessionCoordinator == null) return;
        XrSessionCoordinatorJni.get()
                .onXrSessionButtonTouched(assumeNonNull(mNativeXrSessionCoordinator));
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
        if (mNativeXrSessionCoordinator == null) return;
        mXrHostActivity = new WeakReference<>(activity);

        // Some test code calls us with the default ChromeActivity, and so we don't need to run this
        // code in that case.
        if (activity instanceof XrHostActivity) {
            setupWebXrInput((XrHostActivity) activity);
        }

        XrSessionCoordinatorJni.get()
                .onXrHostActivityReady(assumeNonNull(mNativeXrSessionCoordinator), activity);
    }

    /**
     * Sets up WebXR input routing by redirecting the browser's soft keyboard interactions to the
     * proxy view in {@link XrHostActivity}. Because WebXR on AndroidXR runs in a separate activity
     * (XrHostActivity) that is at the top of the task stack, the browser's 2D activity and its
     * views lose window focus and cannot receive IME interactions directly. We redirect the IME
     * wrapper to target our proxy view in the active XrHostActivity, which then forwards the
     * connection creation back to the browser's adapter.
     */
    private void setupWebXrInput(XrHostActivity xrActivity) {
        if (mWebContents == null) return;
        if (!WebXrAndroidFeatureMap.isOpenXrAndroidSystemKeyboardEnabled()) return;
        XrHostProxyInputView proxyView = xrActivity.getProxyInputView();
        ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContents);
        if (imeAdapter != null && proxyView != null) {
            assert (mOriginalInputMethodManagerWrapper == null);
            assert (mXrInputMethodManagerWrapper == null);
            proxyView.setImeAdapter(imeAdapter);
            mOriginalInputMethodManagerWrapper = imeAdapter.getInputMethodManagerWrapper();
            mXrInputMethodManagerWrapper =
                    new XrInputMethodManagerWrapper(mOriginalInputMethodManagerWrapper, proxyView);
            imeAdapter.setInputMethodManagerWrapper(mXrInputMethodManagerWrapper);
            // Force focus on WebContents so the renderer believes the page is active
            // and allows IME interactions. This is necessary because our new activity may have
            // caused the WebContents to become unfocused.
            mWebContents.setFocus(true);
        }
    }

    /** Restores the original input routing and focus state when the XR session ends. */
    private void restoreBrowserInput() {
        if (mWebContents != null && mOriginalInputMethodManagerWrapper != null) {
            ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContents);
            if (imeAdapter != null) {
                assert (imeAdapter.getInputMethodManagerWrapper() == mXrInputMethodManagerWrapper);
                imeAdapter.setInputMethodManagerWrapper(mOriginalInputMethodManagerWrapper);
            }
            android.view.View containerView =
                    mWebContents.getViewAndroidDelegate() != null
                            ? mWebContents.getViewAndroidDelegate().getContainerView()
                            : null;
            boolean hasRealFocus = containerView != null && containerView.hasWindowFocus();
            mWebContents.setFocus(hasRealFocus);
        }
        mOriginalInputMethodManagerWrapper = null;
        mXrInputMethodManagerWrapper = null;
    }

    @CalledByNative
    private void onNativeDestroy() {
        // The native object is in a bad state, we need to clean ourselves up, but we shouldn't call
        // back into it, so clear it then end any session we may have.
        if (mNativeXrSessionCoordinator != null) {
            mNativeXrSessionCoordinator.release();
            mNativeXrSessionCoordinator = null;
        }
        if (sActiveSessionInstance == this) {
            endSession();
        }
    }

    @NativeMethods
    interface Natives {
        void onDrawingSurfaceReady(
                JniPtr<NativeXrSessionCoordinator> self,
                Surface surface,
                WindowAndroid rootWindow,
                int rotation,
                int width,
                int height);

        void onDrawingSurfaceTouch(
                JniPtr<NativeXrSessionCoordinator> self,
                boolean primary,
                boolean touching,
                int pointerId,
                float x,
                float y);

        void onJavaShutdown(JniPtr<NativeXrSessionCoordinator> self);

        void onXrSessionButtonTouched(JniPtr<NativeXrSessionCoordinator> self);

        void onXrHostActivityReady(
                JniPtr<NativeXrSessionCoordinator> self, @Nullable Activity activity);
    }
}
