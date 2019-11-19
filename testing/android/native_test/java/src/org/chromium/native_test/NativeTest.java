// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Process;
import android.system.Os;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.test.reporter.TestStatusReporter;

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;

/**
 *  Helper to run tests inside Activity or NativeActivity.
 */
@JNINamespace("testing::android")
public class NativeTest {
    public static final String EXTRA_COMMAND_LINE_FILE =
            "org.chromium.native_test.NativeTest.CommandLineFile";
    public static final String EXTRA_COMMAND_LINE_FLAGS =
            "org.chromium.native_test.NativeTest.CommandLineFlags";
    public static final String EXTRA_RUN_IN_SUB_THREAD =
            "org.chromium.native_test.NativeTest.RunInSubThread";
    public static final String EXTRA_SHARD =
            "org.chromium.native_test.NativeTest.Shard";
    public static final String EXTRA_STDOUT_FILE =
            "org.chromium.native_test.NativeTest.StdoutFile";
    public static final String EXTRA_COVERAGE_DEVICE_FILE =
            "org.chromium.native_test.NativeTest.CoverageDeviceFile";

    private static final String TAG = "NativeTest";

    private String mCommandLineFilePath;
    private StringBuilder mCommandLineFlags = new StringBuilder();
    private TestStatusReporter mReporter;
    private boolean mRunInSubThread;
    private String mStdoutFilePath;

    private static class ReportingUncaughtExceptionHandler
            implements Thread.UncaughtExceptionHandler {

        private TestStatusReporter mReporter;
        private Thread.UncaughtExceptionHandler mWrappedHandler;

        public ReportingUncaughtExceptionHandler(TestStatusReporter reporter,
                Thread.UncaughtExceptionHandler wrappedHandler) {
            mReporter = reporter;
            mWrappedHandler = wrappedHandler;
        }

        @Override
        public void uncaughtException(Thread thread, Throwable ex) {
            mReporter.uncaughtException(Process.myPid(), ex);
            if (mWrappedHandler != null) mWrappedHandler.uncaughtException(thread, ex);
        }
    }

    public void preCreate(Activity activity) {
        String coverageDeviceFile = activity.getIntent().getStringExtra(EXTRA_COVERAGE_DEVICE_FILE);
        if (coverageDeviceFile != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                Os.setenv("LLVM_PROFILE_FILE", coverageDeviceFile, true);
            } catch (Exception e) {
                Log.w(TAG, "failed to set LLVM_PROFILE_FILE", e);
            }
        }
    }

    public void postCreate(Activity activity) {
        parseArgumentsFromIntent(activity, activity.getIntent());
        mReporter = new TestStatusReporter(activity);
        mReporter.testRunStarted(Process.myPid());
        Thread.setDefaultUncaughtExceptionHandler(
                new ReportingUncaughtExceptionHandler(mReporter,
                        Thread.getDefaultUncaughtExceptionHandler()));
    }

    private void parseArgumentsFromIntent(Activity activity, Intent intent) {
        Log.i(TAG, "Extras:");
        Bundle extras = intent.getExtras();
        if (extras != null) {
            for (String s : extras.keySet()) {
                Log.i(TAG, "  %s", s);
            }
        }

        mCommandLineFilePath = intent.getStringExtra(EXTRA_COMMAND_LINE_FILE);
        if (mCommandLineFilePath == null) {
            mCommandLineFilePath = "";
        } else {
            File commandLineFile = new File(mCommandLineFilePath);
            if (!commandLineFile.isAbsolute()) {
                mCommandLineFilePath = Environment.getExternalStorageDirectory() + "/"
                        + mCommandLineFilePath;
            }
            Log.i(TAG, "command line file path: %s", mCommandLineFilePath);
        }

        String commandLineFlags = intent.getStringExtra(EXTRA_COMMAND_LINE_FLAGS);
        if (commandLineFlags != null) mCommandLineFlags.append(commandLineFlags);

        mRunInSubThread = intent.hasExtra(EXTRA_RUN_IN_SUB_THREAD);

        ArrayList<String> shard = intent.getStringArrayListExtra(EXTRA_SHARD);
        if (shard != null) {
            StringBuilder filterFlag = new StringBuilder();
            filterFlag.append("--gtest_filter=");
            for (Iterator<String> test_iter = shard.iterator(); test_iter.hasNext();) {
                filterFlag.append(test_iter.next());
                if (test_iter.hasNext()) {
                    filterFlag.append(":");
                }
            }
            appendCommandLineFlags(filterFlag.toString());
        }

        mStdoutFilePath = intent.getStringExtra(EXTRA_STDOUT_FILE);
    }

    public void appendCommandLineFlags(String flags) {
        mCommandLineFlags.append(" ").append(flags);
    }

    public void postStart(final Activity activity, boolean forceRunInSubThread) {
        final Runnable runTestsTask = new Runnable() {
            @Override
            public void run() {
                runTests(activity);
            }
        };

        if (mRunInSubThread || forceRunInSubThread) {
            // Post a task that posts a task that creates a new thread and runs tests on it.

            // On L and M, the system posts a task to the main thread that prints to stdout
            // from android::Layout (https://goo.gl/vZA38p). Chaining the subthread creation
            // through multiple tasks executed on the main thread ensures that this task
            // runs before we start running tests s.t. its output doesn't interfere with
            // the test output. See crbug.com/678146 for additional context.

            final Handler handler = new Handler();
            final Runnable startTestThreadTask = new Runnable() {
                @Override
                public void run() {
                    new Thread(runTestsTask).start();
                }
            };
            final Runnable postTestStarterTask = new Runnable() {
                @Override
                public void run() {
                    handler.post(startTestThreadTask);
                }
            };
            handler.post(postTestStarterTask);
        } else {
            // Post a task to run the tests. This allows us to not block
            // onCreate and still run tests on the main thread.
            new Handler().post(runTestsTask);
        }
    }

    private void runTests(Activity activity) {
        nativeRunTests(mCommandLineFlags.toString(), mCommandLineFilePath, mStdoutFilePath,
                activity.getApplicationContext(), UrlUtils.getIsolatedTestRoot());
        activity.finish();
        mReporter.testRunFinished(Process.myPid());
    }

    // Signal a failure of the native test loader to python scripts
    // which run tests.  For example, we look for
    // RUNNER_FAILED build/android/test_package.py.
    private void nativeTestFailed() {
        Log.e(TAG, "[ RUNNER_FAILED ] could not load native library");
    }

    private native void nativeRunTests(String commandLineFlags, String commandLineFilePath,
            String stdoutFilePath, Context appContext, String testDataDir);
}
