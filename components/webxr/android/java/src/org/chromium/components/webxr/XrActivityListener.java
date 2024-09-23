// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.app.Application;
import android.app.Application.ActivityLifecycleCallbacks;
import android.os.Bundle;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class is intended to listen to the ActivityLifecycle callbacks of a specific Activity and
 * notify the native observers upon its resume. TODO(crbug.com/40929409): Consider reconciling this
 * and the ResumeListener in ArCoreInstallUtils.
 */
@JNINamespace("webxr")
public class XrActivityListener implements ActivityLifecycleCallbacks {
    private static final String TAG = "XrActivityListener";
    private static final boolean DEBUG_LOGS = false;

    private long mNativeXrActivityListener;
    private ImmutableWeakReference<Activity> mWeakActivity;
    private ImmutableWeakReference<Application> mWeakApplication;

    /**
     * Constructs a new XrActivityListener. This listener will listen for events on the Activity
     * to which the provided webContents belongs.
     */
    @CalledByNative
    private XrActivityListener(long nativeXrActivityListener, final WebContents webContents) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "constructor, nativeXrActivityListener=" + nativeXrActivityListener);
        }

        ThreadUtils.assertOnUiThread();

        assert webContents != null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        assert window != null;
        Activity activity = window.getActivity().get();
        assert activity != null;

        mNativeXrActivityListener = nativeXrActivityListener;
        mWeakActivity = new ImmutableWeakReference<Activity>(activity);

        Application application = activity.getApplication();
        mWeakApplication = new ImmutableWeakReference<Application>(application);

        application.registerActivityLifecycleCallbacks(this);
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeXrActivityListener = 0;

        // If we cannot get the application, then we don't have a need to unregister the
        // callbacks.
        final Application application = mWeakApplication.get();
        if (application == null) return;
        application.unregisterActivityLifecycleCallbacks(this);
    }

    @Override
    public void onActivityResumed(Activity activity) {
        if (mWeakActivity.get() != activity || mNativeXrActivityListener == 0) return;

        XrActivityListenerJni.get().onActivityResumed(mNativeXrActivityListener, this);
    }

    // Unfortunately, ActivityLifecycleCallbacks force us to implement all of the methods, but
    // we only really care about onActivityResumed for our purposes.
    @Override
    public void onActivityCreated(final Activity activity, Bundle savedInstanceState) {}

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
        void onActivityResumed(long nativeXrActivityListener, XrActivityListener caller);
    }
}
