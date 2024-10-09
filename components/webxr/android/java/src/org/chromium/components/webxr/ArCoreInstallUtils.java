// Copyright 2019 The Chromium Authors
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

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;

/** Installs AR DFM and ArCore runtimes. */
@JNINamespace("webxr")
public class ArCoreInstallUtils {
    /**
     * Helper class to store a reference to the ArCoreInstallUtils instance and activity
     * that requested an install of ArCore, and await that activity being resumed.
     */
    private static class InstallRequest implements ActivityLifecycleCallbacks {
        private ArCoreInstallUtils mInstallInstance;
        private ImmutableWeakReference<Activity> mWeakActivity;
        private ImmutableWeakReference<Application> mWeakApplication;

        public InstallRequest(ArCoreInstallUtils instance, Activity activity) {
            this.mInstallInstance = instance;
            this.mWeakActivity = new ImmutableWeakReference<Activity>(activity);

            Application application = activity.getApplication();
            this.mWeakApplication = new ImmutableWeakReference<Application>(application);

            application.registerActivityLifecycleCallbacks(this);
        }

        public void dispose() {
            // Clear the ArCoreInstallUtils instance and attempt to unregister from
            // lifecycle notifications.
            mInstallInstance = null;

            // If we cannot get the application, then we don't have a need to unregister the
            // callbacks.
            final Application application = mWeakApplication.get();
            if (application == null) return;
            application.unregisterActivityLifecycleCallbacks(this);
        }

        @Override
        public void onActivityResumed(Activity activity) {
            if (mWeakActivity.get() != activity || mInstallInstance == null) return;

            mInstallInstance.onArCoreRequestInstallReturned(activity);
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
    }

    private static final String TAG = "ArCoreInstallUtils";

    private long mNativeArCoreInstallUtils;

    // This tracks the relevant information of the instance that requested installation of ARCore.
    // Should be non-null only if there is a pending request to install ARCore.
    private static InstallRequest sInstallRequest;

    // Cached ArCoreShim instance - valid only after AR module was installed and
    // getArCoreShimInstance() was called.
    private static ArCoreShim sArCoreInstance;

    private static ArCoreShim getArCoreShimInstance() {
        if (sArCoreInstance != null) return sArCoreInstance;

        try {
            sArCoreInstance =
                    (ArCoreShim)
                            Class.forName("org.chromium.components.webxr.ArCoreShimImpl")
                                    .newInstance();
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

        return sArCoreInstance;
    }

    @CalledByNative
    private static ArCoreInstallUtils create(long nativeArCoreInstallUtils) {
        return new ArCoreInstallUtils(nativeArCoreInstallUtils);
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeArCoreInstallUtils = 0;
        if (sInstallRequest != null) {
            sInstallRequest.dispose();
            sInstallRequest = null;
        }
    }

    private ArCoreInstallUtils(long nativeArCoreInstallUtils) {
        mNativeArCoreInstallUtils = nativeArCoreInstallUtils;
    }

    @CalledByNative
    private static @ArCoreAvailability int getArCoreInstallStatus() {
        try {
            return getArCoreShimInstance().checkAvailability(ContextUtils.getApplicationContext());
        } catch (RuntimeException e) {
            Log.w(TAG, "ARCore availability check failed with error: %s", e.toString());
            return ArCoreAvailability.UNSUPPORTED_DEVICE_NOT_CAPABLE;
        }
    }

    @CalledByNative
    private static boolean shouldRequestInstallSupportedArCore() {
        @ArCoreAvailability int availability = getArCoreInstallStatus();
        // Skip ARCore installation if we are certain that it is already installed.
        // In all other cases, we might as well try to install it and handle installation failures.
        return availability != ArCoreAvailability.SUPPORTED_INSTALLED;
    }

    private Activity getActivity(final WebContents webContents) {
        if (webContents == null) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getActivity().get();
    }

    @CalledByNative
    private void requestInstallSupportedArCore(final WebContents webContents) {
        assert shouldRequestInstallSupportedArCore();

        final Activity activity = getActivity(webContents);
        if (activity == null) {
            Log.w(TAG, "Could not get Activity");
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
            return;
        }

        try {
            assert sInstallRequest == null;
            @ArCoreShim.InstallStatus
            int installStatus = getArCoreShimInstance().requestInstall(activity, true);

            if (installStatus == ArCoreShim.InstallStatus.INSTALL_REQUESTED) {
                // Install flow will resume in onArCoreRequestInstallReturned, mark that
                // there is active request. Native code notification will be deferred until
                // our activity gets resumed.
                sInstallRequest = new InstallRequest(ArCoreInstallUtils.this, activity);
            } else if (installStatus == ArCoreShim.InstallStatus.INSTALLED) {
                // No need to install - notify native code.
                maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
            }

        } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
            sInstallRequest = null;
            Log.w(TAG, "ARCore installation request failed with exception: %s", e.toString());

            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        }
    }

    /** Helper used to notify native code about the result of the request to install ARCore. */
    private void maybeNotifyNativeOnRequestInstallSupportedArCoreResult(boolean success) {
        if (mNativeArCoreInstallUtils != 0) {
            ArCoreInstallUtilsJni.get()
                    .onRequestInstallSupportedArCoreResult(mNativeArCoreInstallUtils, success);
        }
    }

    private void onArCoreRequestInstallReturned(Activity activity) {
        assert sInstallRequest != null;
        maybeNotifyNativeOnRequestInstallSupportedArCoreResult(
                getArCoreInstallStatus() == ArCoreAvailability.SUPPORTED_INSTALLED);
        sInstallRequest.dispose();
        sInstallRequest = null;
    }

    @NativeMethods
    /* package */ interface ArInstallHelperNative {
        void onRequestInstallSupportedArCoreResult(long nativeArCoreInstallHelper, boolean success);
    }
}
