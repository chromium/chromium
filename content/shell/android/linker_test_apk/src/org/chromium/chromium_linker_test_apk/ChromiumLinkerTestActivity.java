// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.Linker;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell.ShellManager;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Test activity used for verifying the different configuration options for the ContentLinker.
 */
public class ChromiumLinkerTestActivity extends Activity {
    private static final String TAG = "LinkerTest";

    private ShellManager mShellManager;
    private ActivityWindowAndroid mWindowAndroid;

    @Override
    public void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Setup the TestRunner class name.
        Linker.setupForTesting(Linker.LINKER_IMPLEMENTATION_LEGACY,
                "org.chromium.chromium_linker_test_apk.LinkerTests");

        // Load the library in the browser process, this will also run the test
        // runner in this process.
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        // Now, start a new renderer process by creating a new view.
        // This will run the test runner in the renderer process.

        LayoutInflater inflater =
                (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.test_activity, null);
        mShellManager = view.findViewById(R.id.shell_container);
        mWindowAndroid = new ActivityWindowAndroid(this, false);
        mShellManager.setWindow(mWindowAndroid);

        mShellManager.setStartupUrl("about:blank");

        BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                .startBrowserProcessesAsync(
                        true, false, new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess() {
                                finishInitialization(savedInstanceState);
                            }

                            @Override
                            public void onFailure() {
                                initializationFailed();
                            }
                        });

        // TODO(digit): Ensure that after the content view is initialized,
        // the program finishes().
    }

    private void finishInitialization(Bundle savedInstanceState) {
        String shellUrl = ShellManager.DEFAULT_SHELL_URL;
        mShellManager.launchShell(shellUrl);
    }

    private void initializationFailed() {
        Log.e(TAG, "ContentView initialization failed.");
        finish();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mWindowAndroid.saveInstanceState(outState);
    }

    @Override
    protected void onStop() {
        super.onStop();

        WebContents webContents = getActiveWebContents();
        if (webContents != null) webContents.onHide();
    }

    @Override
    protected void onStart() {
        super.onStart();

        WebContents webContents = getActiveWebContents();
        if (webContents != null) webContents.onHide();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mWindowAndroid.onActivityResult(requestCode, resultCode, data);
    }

    /**
     * @return The {@link WebContents} owned by the currently visible {@link Shell} or null if
     *         one is not showing.
     */
    public WebContents getActiveWebContents() {
        if (mShellManager == null) return null;
        Shell shell = mShellManager.getActiveShell();
        return shell != null ? shell.getWebContents() : null;
    }
}
