// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_browsertests_apk;

import android.os.Bundle;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_shell.browsertests.ContentShellBrowserTestActivity;

import java.io.File;

/**
 * Android activity for running content_public.browser.tests
 */
public class ContentBrowserTestsActivity extends ContentShellBrowserTestActivity {
    private static final String TAG = "native_test";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appendCommandLineFlags(
                "--remote-debugging-socket-name content_browsertests_devtools_remote");
    }

    @Override
    protected File getPrivateDataDirectory() {
        // TODO(agrieve): We should not be touching the side-loaded test data directory.
        //     https://crbug.com/617734
        return new File(UrlUtils.getIsolatedTestRoot(),
                ContentBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    protected int getTestActivityViewId() {
        return R.layout.test_activity;
    }

    @Override
    protected int getShellManagerViewId() {
        return R.id.shell_container;
    }

}
