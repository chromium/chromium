// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.PowerMonitor;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.NativeLibraries;

/** A helper for running native unit tests (i.e., not browser tests) */
public class NativeUnitTest extends NativeTest {
    private static final String TAG = "NativeTest";
    // This key is only used by Cronet in AOSP to run tests instead of
    // NativeLibraries.LIBRARIES constant.
    //
    // The reason for that is that Cronet translates GN build rules to
    // AOSP Soong modules, the translation layer is incapable currently
    // of replicating the logic embedded into GN to replace the temporary
    // NativeLibraries with the real NativeLibraries at the root of the build
    // graph, hence we depend on the value of this key to carry the name
    // of the native library to load.
    //
    // Assumption: The code below assumes that the value is exactly a single
    // library. There is no support for loading multiple libraries through this
    // key at the moment.
    private static final String LIBRARY_UNDER_TEST_NAME =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.LibraryUnderTest";

    private static class NativeUnitTestLibraryLoader extends LibraryLoader {
        static void setLibrariesLoaded() {
            LibraryLoader.setLibrariesLoadedForNativeTests();
        }
    }

    @Override
    public void preCreate(Activity activity) {
        super.preCreate(activity);
        // Necessary because NativeUnitTestActivity uses BaseChromiumApplication which does not
        // initialize ContextUtils.
        ContextUtils.initApplicationContext(activity.getApplicationContext());

        // Necessary because BaseChromiumApplication no longer automatically initializes application
        // tracking.
        ApplicationStatus.initialize(activity.getApplication());

        // Needed by path_utils_unittest.cc
        PathUtils.setPrivateDataDirectorySuffix("chrome");

        // Needed by system_monitor_unittest.cc
        PowerMonitor.createForTests();

        // For NativeActivity based tests, dependency libraries must be loaded before
        // NativeActivity::OnCreate, otherwise loading android.app.lib_name will fail
        String libraryToLoad = activity.getIntent().getStringExtra(LIBRARY_UNDER_TEST_NAME);
        loadLibraries(
                libraryToLoad != null ? new String[] {libraryToLoad} : NativeLibraries.LIBRARIES);
    }

    private void loadLibraries(String[] librariesToLoad) {
        LibraryLoader.setEnvForNative();
        for (String library : librariesToLoad) {
            // Do not load this library early so that
            // |LibunwindstackUnwinderAndroidTest.ReparsesMapsOnNewDynamicLibraryLoad| test can
            // observe the change in /proc/self/maps before and after loading the library.
            if (library.equals("base_profiler_reparsing_test_support_library")) {
                continue;
            }
            Log.i(TAG, "loading: %s", library);
            System.loadLibrary(library);
            Log.i(TAG, "loaded: %s", library);
        }
        NativeUnitTestLibraryLoader.setLibrariesLoaded();
    }
}
