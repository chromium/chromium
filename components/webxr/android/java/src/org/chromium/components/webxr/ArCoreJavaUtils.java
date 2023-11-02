// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.Context;
import android.view.Surface;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides ARCore classes access to java-related app functionality.
 *
 * <p>This class provides static methods called by ArDelegateImpl via ArDelegateProvider,
 * and provides JNI interfaces to/from the C++ AR code.</p>
 */
@JNINamespace("webxr")
public class ArCoreJavaUtils {
    private static final String TAG = "ArCoreJavaUtils";
    private static final boolean DEBUG_LOGS = false;

    private long mNativeArCoreJavaUtils;

    // The native ArCoreDevice runtime creates a ArCoreJavaUtils instance in its constructor,
    // and keeps a strong reference to it for the lifetime of the device. It creates and
    // owns an ArImmersiveOverlay for the duration of an immersive-ar session, which in
    // turn contains a reference to ArCoreJavaUtils for making JNI calls back to the device.
    private ArImmersiveOverlay mArImmersiveOverlay;

    // ArDelegateImpl needs to know if there's an active immersive session so that it can handle
    // back button presses from ChromeActivity's onBackPressed(). It's only set while a session is
    // in progress, and reset to null on session end. The ArImmersiveOverlay member has a strong
    // reference to the ChromeActivity, and that shouldn't be retained beyond the duration of a
    // session.
    private static ArCoreJavaUtils sActiveSessionInstance;

    /** Whether there is a non-null valid {@link #sActiveSessionInstance}. */
    private static ObservableSupplierImpl<Boolean> sActiveSessionAvailableSupplier =
            new ObservableSupplierImpl<>();

    // Helper, obtains android Activity out of passed in WebContents instance.
    // Equivalent to ChromeActivity.fromWebContents(), but does not require that
    // the resulting instance is a ChromeActivity.
    public static Activity getActivity(final WebContents webContents) {
        if (webContents == null) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getActivity().get();
    }

    @CalledByNative
    private static ArCoreJavaUtils create(long nativeArCoreJavaUtils) {
        ThreadUtils.assertOnUiThread();
        return new ArCoreJavaUtils(nativeArCoreJavaUtils);
    }

    @CalledByNative
    private static String getArCoreShimLibraryPath() {
        return BundleUtils.getNativeLibraryPath("arcore_sdk_c");
    }

    /**
     * Gets the current application context.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    private ArCoreJavaUtils(long nativeArCoreJavaUtils) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor, nativeArCoreJavaUtils=" + nativeArCoreJavaUtils);
        mNativeArCoreJavaUtils = nativeArCoreJavaUtils;
    }

    @CalledByNative
    private void startSession(final ArCompositorDelegateProvider compositorDelegateProvider,
            final WebContents webContents, boolean useOverlay, boolean canRenderDomContent) {
        if (DEBUG_LOGS) Log.i(TAG, "startSession");
        mArImmersiveOverlay = new ArImmersiveOverlay();
        sActiveSessionInstance = this;
        sActiveSessionAvailableSupplier.set(true);
        mArImmersiveOverlay.show(compositorDelegateProvider.create(webContents), webContents, this,
                useOverlay, canRenderDomContent);
    }

    @CalledByNative
    private void endSession() {
        if (DEBUG_LOGS) Log.i(TAG, "endSession");
        if (mArImmersiveOverlay == null) return;

        mArImmersiveOverlay.cleanupAndExit();
        mArImmersiveOverlay = null;
        sActiveSessionInstance = null;
        sActiveSessionAvailableSupplier.set(false);
    }

    // Called from ArDelegateImpl
    public static boolean onBackPressed() {
        if (DEBUG_LOGS) Log.i(TAG, "onBackPressed");
        // If there's an active immersive session, consume the "back" press and shut down the
        // session.
        if (sActiveSessionInstance != null) {
            sActiveSessionInstance.endSession();
            return true;
        }
        return false;
    }

    public static boolean hasActiveArSession() {
        return sActiveSessionInstance != null;
    }

    public static ObservableSupplier<Boolean> hasActiveArSessionSupplier() {
        return sActiveSessionAvailableSupplier;
    }

    public void onDrawingSurfaceReady(
            Surface surface, WindowAndroid rootWindow, int rotation, int width, int height) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceReady");
        if (mNativeArCoreJavaUtils == 0) return;
        ArCoreJavaUtilsJni.get().onDrawingSurfaceReady(mNativeArCoreJavaUtils, ArCoreJavaUtils.this,
                surface, rootWindow, rotation, width, height);
    }

    public void onDrawingSurfaceTouch(
            boolean isPrimary, boolean isTouching, int pointerId, float x, float y) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceTouch");
        if (mNativeArCoreJavaUtils == 0) return;
        ArCoreJavaUtilsJni.get().onDrawingSurfaceTouch(mNativeArCoreJavaUtils, ArCoreJavaUtils.this,
                isPrimary, isTouching, pointerId, x, y);
    }

    public void onDrawingSurfaceDestroyed() {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceDestroyed");
        if (mNativeArCoreJavaUtils == 0) return;
        ArCoreJavaUtilsJni.get().onDrawingSurfaceDestroyed(
                mNativeArCoreJavaUtils, ArCoreJavaUtils.this);
    }

    @CalledByNative
    private void onNativeDestroy() {
        // ArCoreDevice's destructor ends sessions before destroying its native ArCoreSessionUtils
        // object.
        assert sActiveSessionInstance == null : "unexpected active session in onNativeDestroy";

        mNativeArCoreJavaUtils = 0;
    }

    @NativeMethods
    interface Natives {
        void onDrawingSurfaceReady(long nativeArCoreJavaUtils, ArCoreJavaUtils caller,
                Surface surface, WindowAndroid rootWindow, int rotation, int width, int height);
        void onDrawingSurfaceTouch(long nativeArCoreJavaUtils, ArCoreJavaUtils caller,
                boolean primary, boolean touching, int pointerId, float x, float y);
        void onDrawingSurfaceDestroyed(long nativeArCoreJavaUtils, ArCoreJavaUtils caller);
    }
}
