// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.Application;
import android.app.Application.ActivityLifecycleCallbacks;
import android.os.Bundle;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class is intended to listen to the ActivityLifecycle callbacks of a specific Activity and
 * notify the native observers upon its resume. TODO(crbug.com/40929409): Consider reconciling this
 * and the ResumeListener in ArCoreInstallUtils.
 */
@JNINamespace("webxr")
@NullMarked
public class XrActivityListener implements ActivityLifecycleCallbacks {
    @JniType("::webxr::XrActivityListener")
    interface NativeXrActivityListener extends JniTypeToken {}

    private static final String TAG = "XrActivityListener";
    private static final boolean DEBUG_LOGS = false;

    private @Nullable JniRawPtr<NativeXrActivityListener> mNativeXrActivityListener;
    private final ImmutableWeakReference<Activity> mWeakActivity;
    private final ImmutableWeakReference<Application> mWeakApplication;

    /**
     * Constructs a new XrActivityListener. This listener will listen for events on the Activity
     * to which the provided webContents belongs.
     */
    @CalledByNative
    XrActivityListener(
            JniRawPtr<NativeXrActivityListener> nativeXrActivityListener,
            final WebContents webContents) {
        try {
            if (DEBUG_LOGS) {
                Log.i(TAG, "constructor, nativeXrActivityListener=" + nativeXrActivityListener);
            }

            ThreadUtils.assertOnUiThread();

            assert webContents != null;
            WindowAndroid window = webContents.getTopLevelNativeWindow();
            assert window != null;
            Activity activity = window.getActivity().get();
            assert activity != null;

            mWeakActivity = new ImmutableWeakReference<Activity>(activity);

            Application application = activity.getApplication();
            mWeakApplication = new ImmutableWeakReference<Application>(application);

            application.registerActivityLifecycleCallbacks(this);
            mNativeXrActivityListener = nativeXrActivityListener;
        } catch (Throwable t) {
            nativeXrActivityListener.release();
            throw t;
        }
    }

    @CalledByNative
    private void onNativeDestroy() {
        if (mNativeXrActivityListener != null) {
            mNativeXrActivityListener.release();
            mNativeXrActivityListener = null;
        }

        // If we cannot get the application, then we don't have a need to unregister the
        // callbacks.
        final Application application = mWeakApplication.get();
        if (application == null) return;
        application.unregisterActivityLifecycleCallbacks(this);
    }

    @Override
    public void onActivityResumed(Activity activity) {
        if (mWeakActivity.get() != activity || mNativeXrActivityListener == null) return;

        XrActivityListenerJni.get().onActivityResumed(assumeNonNull(mNativeXrActivityListener));
    }

    // Unfortunately, ActivityLifecycleCallbacks force us to implement all of the methods, but
    // we only really care about onActivityResumed for our purposes.
    @Override
    public void onActivityCreated(final Activity activity, @Nullable Bundle savedInstanceState) {}

    @Override
    public void onActivityDestroyed(Activity activity) {}

    @Override
    public void onActivityPaused(Activity activity) {}

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle bundle) {}

    @Override
    public void onActivityStarted(Activity activity) {}

    @Override
    public void onActivityStopped(Activity activity) {}

    @NativeMethods
    interface Natives {
        void onActivityResumed(JniPtr<NativeXrActivityListener> self);
    }
}
