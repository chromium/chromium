// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.JNINamespace;

import java.io.File;

/**
 *  Helper for browser tests running inside a java Activity.
 */
@JNINamespace("testing::android")
public class NativeBrowserTest {
    private static final String TAG = "NativeBrowserTest";

    // Set the command line flags to be passed to the C++ main() method. Each
    // browser tests Activity should ensure these are included.
    public static final String BROWSER_TESTS_FLAGS[] = {
            // switches::kSingleProcessTests
            "--single-process-tests",

            // switches::kUseFakeDeviceForMediaStream
            "--use-fake-device-for-media-stream"};

    /**
     * Deletes a file or directory along with any of its children.
     *
     * Note that, like File.delete(), this returns false if the file or directory couldn't be
     * fully deleted. This means that, in the directory case, some files may be deleted even if
     * the entire directory couldn't be.
     *
     * @param file The file or directory to delete.
     * @return Whether or not the file or directory was deleted.
     */
    private static boolean deleteRecursive(File file) {
        if (file == null) return true;

        File[] children;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            children = file.listFiles();
        }
        if (children != null) {
            for (File child : children) {
                if (!deleteRecursive(child)) {
                    return false;
                }
            }
        }
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return file.delete();
        }
    }

    public static void deletePrivateDataDirectory(File privateDataDirectory) {
        if (!deleteRecursive(privateDataDirectory)) {
            Log.e(TAG, "Failed to remove %s", privateDataDirectory.getAbsolutePath());
        }
    }

    /**
     * To be called when the browser tests Activity has completed any asynchronous
     * initialization and is ready for the test to run. This informs C++ to run the test.
     */
    public static void javaStartupTasksComplete() {
        nativeJavaStartupTasksCompleteForBrowserTests();
    }

    private static native void nativeJavaStartupTasksCompleteForBrowserTests();
}
