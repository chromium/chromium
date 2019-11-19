// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;

/**
 * An {@link android.app.Application} for running native browser tests.
 */
public abstract class NativeBrowserTestApplication extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        initApplicationContext();
        if (isMainProcess() || isBrowserProcess()) {
            // We need secondary dex in order to run EmbeddedTestServer in a
            // privileged process.
            if (BuildConfig.IS_MULTIDEX_ENABLED) ChromiumMultiDexInstaller.install(this);
        }
        if (isBrowserProcess()) {
            CommandLine.init(new String[] {});
            ApplicationStatus.initialize(this);
        }
    }

    /**
     * Initializes the application context. Subclasses may want to override this if the
     * application context is initialized elsewhere.
     */
    protected void initApplicationContext() {
        ContextUtils.initApplicationContext(this);
    }

    protected static boolean isMainProcess() {
        // The test harness runs in the main process, and browser in :test_process.
        return !ContextUtils.getProcessName().contains(":");
    }

    protected static boolean isBrowserProcess() {
        // The test harness runs in the main process, and browser in :test_process.
        return ContextUtils.getProcessName().contains(":test");
    }
}
