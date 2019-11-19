// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell.browsertests;

import android.content.Context;
import android.net.Uri;
import android.support.v4.content.FileProvider;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.content_shell.ShellManager;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeBrowserTestActivity;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;

/** An Activity base class for running browser tests against ContentShell. */
public abstract class ContentShellBrowserTestActivity extends NativeBrowserTestActivity {
    private static final String TAG = "native_test";

    private ShellManager mShellManager;
    private WindowAndroid mWindowAndroid;

    private static class FileProviderHelper implements ContentUriUtils.FileProviderUtil {
        // Keep this variable in sync with the value defined in file_paths.xml.
        private static final String API_AUTHORITY_SUFFIX = ".FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }
    /**
     * Initializes the browser process.
     *
     * This generally includes loading native libraries and switching to the native command line,
     * among other things.
     */
    @Override
    protected void initializeBrowserProcess() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        }

        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
        setContentView(getTestActivityViewId());
        mShellManager = (ShellManager) findViewById(getShellManagerViewId());
        mWindowAndroid = new ActivityWindowAndroid(this);
        mShellManager.setWindow(mWindowAndroid);

        Window wind = this.getWindow();
        wind.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        wind.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        wind.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

        BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                .setContentMainCallbackForTests(() -> {
                    // This jumps into C++ to set up and run the test harness. The test harness runs
                    // ContentMain()-equivalent code, and then waits for javaStartupTasksComplete()
                    // to be called.
                    runTests();
                });
        BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                .startBrowserProcessesAsync(false, false, new StartupCallback() {
                    @Override
                    public void onSuccess() {
                        // The C++ test harness is running thanks to runTests() above, but it
                        // waits for Java initialization to complete. This tells C++ that it may
                        // continue now to finish running the tests.
                        NativeBrowserTest.javaStartupTasksComplete();
                    }
                    @Override
                    public void onFailure() {
                        throw new RuntimeException("Failed to startBrowserProcessesAsync()");
                    }
                });
    }

    protected abstract int getTestActivityViewId();

    protected abstract int getShellManagerViewId();
}
