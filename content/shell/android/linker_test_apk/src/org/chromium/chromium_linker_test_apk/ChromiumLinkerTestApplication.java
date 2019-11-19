// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import android.app.Application;
import android.content.Context;

import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;
import org.chromium.ui.base.ResourceBundle;

/**
 * Application for testing the Chromium Linker
 */
public class ChromiumLinkerTestApplication extends Application {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromium_linker_test";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        if (BuildConfig.IS_MULTIDEX_ENABLED) {
            ChromiumMultiDexInstaller.install(this);
        }
        ContextUtils.initApplicationContext(this);
        ResourceBundle.setNoAvailableLocalePaks();
    }

    @Override
    public void onCreate() {
        super.onCreate();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
