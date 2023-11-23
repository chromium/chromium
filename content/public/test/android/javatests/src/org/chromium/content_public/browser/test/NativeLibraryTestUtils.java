// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;

/** Provides test support for loading and dealing with native libraries. */
public class NativeLibraryTestUtils {
    /** Loads the native library on the activity UI thread. */
    public static void loadNativeLibraryNoBrowserProcess() {
        handleNativeInitialization(false);
    }

    /**
     * Loads the native library on the activity UI thread.
     * After loading the library, this will initialize the browser process.
     */
    public static void loadNativeLibraryAndInitBrowserProcess() {
        handleNativeInitialization(true);
    }

    private static void handleNativeInitialization(final boolean initBrowserProcess) {
        // LibraryLoader is not in general multithreaded; as other InstrumentationTestCase code
        // (specifically, ChromeBrowserProvider) uses it from the main thread we must do
        // likewise.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    nativeInitialization(initBrowserProcess);
                });
    }

    private static void nativeInitialization(boolean initBrowserProcess) {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        if (initBrowserProcess) {
            BrowserStartupController.getInstance()
                    .startBrowserProcessesSync(LibraryProcessType.PROCESS_BROWSER, false, false);
        } else {
            LibraryLoader.getInstance().ensureInitialized();
        }
    }
}
