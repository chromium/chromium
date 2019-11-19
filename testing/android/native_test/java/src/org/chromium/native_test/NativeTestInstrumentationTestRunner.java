// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Process;
import android.util.SparseArray;

import org.chromium.base.Log;
import org.chromium.test.reporter.TestStatusReceiver;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Queue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 *  An Instrumentation that runs tests based on NativeTest.
 */
public class NativeTestInstrumentationTestRunner extends Instrumentation {

    public static final String EXTRA_NATIVE_TEST_ACTIVITY =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity";
    public static final String EXTRA_SHARD_NANO_TIMEOUT =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout";
    public static final String EXTRA_SHARD_SIZE_LIMIT =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardSizeLimit";
    public static final String EXTRA_STDOUT_FILE =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile";
    public static final String EXTRA_TEST_LIST_FILE =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.TestList";
    public static final String EXTRA_TEST =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.Test";

    private static final String TAG = "NativeTest";

    private static final long DEFAULT_SHARD_NANO_TIMEOUT = 60 * 1000000000L;
    // Default to no size limit.
    private static final int DEFAULT_SHARD_SIZE_LIMIT = 0;
    private static final String DEFAULT_NATIVE_TEST_ACTIVITY =
            "org.chromium.native_test.NativeUnitTestActivity";

    private Handler mHandler = new Handler();
    private Bundle mLogBundle = new Bundle();
    private SparseArray<ShardMonitor> mMonitors = new SparseArray<ShardMonitor>();
    private String mNativeTestActivity;
    private TestStatusReceiver mReceiver;
    private Queue<ArrayList<String>> mShards = new ArrayDeque<ArrayList<String>>();
    private long mShardNanoTimeout = DEFAULT_SHARD_NANO_TIMEOUT;
    private int mShardSizeLimit = DEFAULT_SHARD_SIZE_LIMIT;
    private File mStdoutFile;
    private Bundle mTransparentArguments;

    @Override
    public void onCreate(Bundle arguments) {
        mTransparentArguments = new Bundle(arguments);

        mNativeTestActivity = arguments.getString(EXTRA_NATIVE_TEST_ACTIVITY);
        if (mNativeTestActivity == null) mNativeTestActivity = DEFAULT_NATIVE_TEST_ACTIVITY;
        mTransparentArguments.remove(EXTRA_NATIVE_TEST_ACTIVITY);

        String shardNanoTimeout = arguments.getString(EXTRA_SHARD_NANO_TIMEOUT);
        if (shardNanoTimeout != null) mShardNanoTimeout = Long.parseLong(shardNanoTimeout);
        mTransparentArguments.remove(EXTRA_SHARD_NANO_TIMEOUT);

        String shardSizeLimit = arguments.getString(EXTRA_SHARD_SIZE_LIMIT);
        if (shardSizeLimit != null) mShardSizeLimit = Integer.parseInt(shardSizeLimit);
        mTransparentArguments.remove(EXTRA_SHARD_SIZE_LIMIT);

        String stdoutFile = arguments.getString(EXTRA_STDOUT_FILE);
        if (stdoutFile != null) {
            mStdoutFile = new File(stdoutFile);
        } else {
            try {
                mStdoutFile = File.createTempFile(
                        ".temp_stdout_", ".txt", Environment.getExternalStorageDirectory());
                Log.i(TAG, "stdout file created: %s", mStdoutFile.getAbsolutePath());
            } catch (IOException e) {
                Log.e(TAG, "Unable to create temporary stdout file.", e);
                finish(Activity.RESULT_CANCELED, new Bundle());
                return;
            }
        }

        mTransparentArguments.remove(EXTRA_STDOUT_FILE);

        String singleTest = arguments.getString(EXTRA_TEST);
        if (singleTest != null) {
            ArrayList<String> shard = new ArrayList<>(1);
            shard.add(singleTest);
            mShards.add(shard);
        }

        String testListFilePath = arguments.getString(EXTRA_TEST_LIST_FILE);
        if (testListFilePath != null) {
            File testListFile = new File(testListFilePath);
            try {
                BufferedReader testListFileReader =
                        new BufferedReader(new FileReader(testListFile));

                String test;
                ArrayList<String> workingShard = new ArrayList<String>();
                while ((test = testListFileReader.readLine()) != null) {
                    workingShard.add(test);
                    if (workingShard.size() == mShardSizeLimit) {
                        mShards.add(workingShard);
                        workingShard = new ArrayList<String>();
                    }
                }

                if (!workingShard.isEmpty()) {
                    mShards.add(workingShard);
                }

                testListFileReader.close();
            } catch (IOException e) {
                Log.e(TAG, "Error reading %s", testListFile.getAbsolutePath(), e);
            }
        }
        mTransparentArguments.remove(EXTRA_TEST_LIST_FILE);

        start();
    }

    @Override
    @SuppressLint("DefaultLocale")
    public void onStart() {
        super.onStart();

        mReceiver = new TestStatusReceiver();
        mReceiver.register(getContext());
        mReceiver.registerCallback(new TestStatusReceiver.TestRunCallback() {
            @Override
            public void testRunStarted(int pid) {
                if (pid != Process.myPid()) {
                    ShardMonitor m = new ShardMonitor(
                            pid, System.nanoTime() + mShardNanoTimeout);
                    mMonitors.put(pid, m);
                    mHandler.post(m);
                }
            }

            @Override
            public void testRunFinished(int pid) {
                ShardMonitor m = mMonitors.get(pid);
                if (m != null) {
                    m.stopped();
                    mMonitors.remove(pid);
                }
                mHandler.post(new ShardEnder(pid));
            }

            @Override
            public void uncaughtException(int pid, String stackTrace) {
                mLogBundle.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                        String.format("Uncaught exception in test process (pid: %d)%n%s%n",
                                pid, stackTrace));
                sendStatus(0, mLogBundle);
            }
        });

        mHandler.post(new ShardStarter());
    }

    /** Monitors a test shard's execution. */
    private class ShardMonitor implements Runnable {
        private static final int MONITOR_FREQUENCY_MS = 1000;

        private long mExpirationNanoTime;
        private int mPid;
        private AtomicBoolean mStopped;

        public ShardMonitor(int pid, long expirationNanoTime) {
            mPid = pid;
            mExpirationNanoTime = expirationNanoTime;
            mStopped = new AtomicBoolean(false);
        }

        public void stopped() {
            mStopped.set(true);
        }

        @Override
        public void run() {
            if (mStopped.get()) {
                return;
            }

            if (isAppProcessAlive(getContext(), mPid)) {
                if (System.nanoTime() > mExpirationNanoTime) {
                    Log.e(TAG, "Test process %d timed out.", mPid);
                    mHandler.post(new ShardEnder(mPid));
                    return;
                } else {
                    mHandler.postDelayed(this, MONITOR_FREQUENCY_MS);
                    return;
                }
            }

            Log.e(TAG, "Test process %d died unexpectedly.", mPid);
            mHandler.post(new ShardEnder(mPid));
        }

    }

    private static boolean isAppProcessAlive(Context context, int pid) {
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningAppProcessInfo processInfo :
                activityManager.getRunningAppProcesses()) {
            if (processInfo.pid == pid) return true;
        }
        return false;
    }

    /** Starts the NativeTest Activity.
     */
    private class ShardStarter implements Runnable {
        @Override
        public void run() {
            Intent i = new Intent(Intent.ACTION_MAIN);
            i.setComponent(new ComponentName(getContext().getPackageName(), mNativeTestActivity));
            i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            i.putExtras(mTransparentArguments);
            if (mShards != null && !mShards.isEmpty()) {
                ArrayList<String> shard = mShards.remove();
                i.putStringArrayListExtra(NativeTest.EXTRA_SHARD, shard);
            }
            i.putExtra(NativeTest.EXTRA_STDOUT_FILE, mStdoutFile.getAbsolutePath());
            getContext().startActivity(i);
        }
    }

    private class ShardEnder implements Runnable {
        private static final int WAIT_FOR_DEATH_MILLIS = 10;

        private int mPid;

        public ShardEnder(int pid) {
            mPid = pid;
        }

        @Override
        public void run() {
            if (mPid != Process.myPid()) {
                Process.killProcess(mPid);
                try {
                    while (isAppProcessAlive(getContext(), mPid)) {
                        Thread.sleep(WAIT_FOR_DEATH_MILLIS);
                    }
                } catch (InterruptedException e) {
                    Log.e(TAG, "%d may still be alive.", mPid, e);
                }
            }
            if (mShards != null && !mShards.isEmpty()) {
                mHandler.post(new ShardStarter());
            } else {
                finish(Activity.RESULT_OK, new Bundle());
            }
        }
    }
}
