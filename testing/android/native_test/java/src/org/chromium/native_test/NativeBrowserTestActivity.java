// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.os.Bundle;
import android.support.v4.app.FragmentActivity;

import java.io.File;

/**
 * An {@link android.app.Activity} for running native browser tests.
 */
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

    /** Initializes the browser process.
     *
     *  This generally includes loading native libraries and switching to the native command line,
     *  among other things.
     */
    protected abstract void initializeBrowserProcess();
}
