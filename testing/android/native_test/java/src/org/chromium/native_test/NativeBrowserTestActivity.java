// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.os.Bundle;

import androidx.fragment.app.FragmentActivity;

import java.io.File;

/** An {@link android.app.Activity} for running native browser tests. */
public abstract class NativeBrowserTestActivity extends FragmentActivity {
    private static final String TAG = "NativeTest";

    private NativeTest mTest = new NativeTest();
    private boolean mStarted;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mTest.preCreate(this);
        super.onCreate(savedInstanceState);
        mTest.postCreate(this);
        for (String flag : NativeBrowserTest.BROWSER_TESTS_FLAGS) {
            appendCommandLineFlags(flag);
        }

        String userDataDirSwitch = getUserDataDirectoryCommandLineSwitch();
        if (!userDataDirSwitch.isEmpty()) {
            String userDataDirFlag = "--" + userDataDirSwitch + "=" + getPrivateDataDirectory();
            appendCommandLineFlags(userDataDirFlag);
        }
    }

    @Override
    public void onStart() {
        super.onStart();

        // onStart can be called any number of times see:
        // https://developer.android.com/guide/components/activities/activity-lifecycle#onstart
        // We only want to run the test once (or bad things can happen) so bail out if we've
        // already started.
        if (mStarted) return;

        mStarted = true;
        NativeBrowserTest.deletePrivateDataDirectory(getPrivateDataDirectory());
        initializeBrowserProcess();
    }

    protected void runTests() {
        mTest.postStart(this, false);
    }

    public void appendCommandLineFlags(String flags) {
        mTest.appendCommandLineFlags(flags);
    }

    /** Returns the test suite's private data directory. */
    protected abstract File getPrivateDataDirectory();

    /**
     * Returns the command line switch used to specify the user data directory.
     *
     * <p>The default implementation returns an empty string, which means no user data directory. If
     * this method returns a non-empty value, the user data directory will be overridden to be the
     * private data directory, which is cleared at the beginning of each test run. NOTE: The switch
     * should not start with "--". TODO(crbug.com/40471984): Solve this problem holistically for
     * Java and C++ at the level of DIR_ANDROID_APP_DATA and eliminate the need for this solution.
     */
    protected String getUserDataDirectoryCommandLineSwitch() {
        return "";
    }

    /** Initializes the browser process.
     *
     *  This generally includes loading native libraries and switching to the native command line,
     *  among other things.
     */
    protected abstract void initializeBrowserProcess();
}
