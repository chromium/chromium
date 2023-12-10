// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.ui.base.ResourceBundle;

/**
 * Entry point for the content shell application.  Handles initialization of information that needs
 * to be shared across the main activity and the child services created.
 */
public class ContentShellApplication extends Application {
    public static final String COMMAND_LINE_FILE = "/data/local/tmp/content-shell-command-line";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        boolean isBrowserProcess = !ContextUtils.getProcessName().contains(":");
        ContextUtils.initApplicationContext(this);
        ResourceBundle.setNoAvailableLocalePaks();
        LibraryLoader.getInstance()
                .setLibraryProcessType(
                        isBrowserProcess
                                ? LibraryProcessType.PROCESS_BROWSER
                                : LibraryProcessType.PROCESS_CHILD);
        if (isBrowserProcess) {
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            ApplicationStatus.initialize(this);
        }
    }

    public void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }
    }
}
