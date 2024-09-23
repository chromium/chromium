// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.app;

import android.annotation.SuppressLint;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.build.BuildConfig;

/**
 * Class used in android:zygotePreloadName attribute of manifest.
 * Code in this class runs in the zygote. It runs in a limited environment
 * (eg no application) and cannot communicate with any other app process,
 * so care must be taken when writing code in this class. Eg it should not
 * create any thread.
 */
@RequiresApi(Build.VERSION_CODES.Q)
public class ZygotePreload implements android.app.ZygotePreload {
    private static final String TAG = "ZygotePreload";

    @SuppressLint("Override")
    @Override
    public void doPreload(ApplicationInfo appInfo) {
        // APKs targeting pre-Q releases, like ChromePublic, have ZygotePreload in their manifests.
        // Running Chrome from these APKs on Q+ creates the app zygote and performs the preload. In
        // order to load the native library with the correct name prefix, the "linker
        // implementation" is chosen by the LibraryLoader itself, and not overwritten here.
        doPreloadCommon(appInfo);
    }

    protected final void doPreloadCommon(ApplicationInfo appInfo) {
        Log.i(
                TAG,
                "version=%s (%s) minSdkVersion=%s isBundle=%s",
                VersionConstants.PRODUCT_VERSION,
                BuildConfig.VERSION_CODE,
                BuildConfig.MIN_SDK_VERSION,
                BuildConfig.IS_BUNDLE);
        try {
            // The current thread time is the best approximation we have of the zygote start time
            // since Process.getStartUptimeMillis() is not reliable in the zygote process. This will
            // be the total CPU time the current thread has been running, and is reset on fork so
            // should give an accurate measurement of zygote process startup time.
            ChildProcessService.setZygoteInfo(
                    Process.myPid(), SystemClock.currentThreadTimeMillis());
            LibraryLoader.getInstance().getMediator().initInAppZygote();
            LibraryLoader.getInstance().loadNowInZygote(appInfo);
        } catch (Throwable e) {
            // Ignore any exception. Child service can continue loading.
            Log.w(TAG, "Exception in zygote", e);
        }
    }
}
