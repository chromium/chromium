// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.test.reporter;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/** Receives test status broadcasts send from
    {@link org.chromium.test.reporter.TestStatusReporter}.
 */
public class TestStatusReceiver extends BroadcastReceiver {

    private static final String TAG = "test_reporter";

    private final List<TestRunCallback> mTestRunCallbacks = new ArrayList<TestRunCallback>();

    /** An IntentFilter that matches the intents that this class can receive. */
    private static final IntentFilter INTENT_FILTER;
    static {
        IntentFilter filter = new IntentFilter();
        filter.addAction(TestStatusReporter.ACTION_TEST_RUN_STARTED);
        filter.addAction(TestStatusReporter.ACTION_TEST_RUN_FINISHED);
        filter.addAction(TestStatusReporter.ACTION_UNCAUGHT_EXCEPTION);
        try {
            filter.addDataType(TestStatusReporter.DATA_TYPE_RESULT);
        } catch (IntentFilter.MalformedMimeTypeException e) {
            Log.wtf(TAG, "Invalid MIME type", e);
        }
        INTENT_FILTER = filter;
    }

    /** A callback used when a test run has started or finished. */
    public interface TestRunCallback {
        void testRunStarted(int pid);
        void testRunFinished(int pid);
        void uncaughtException(int pid, String stackTrace);
    }

    /** Register a callback for when a test run has started or finished. */
    public void registerCallback(TestRunCallback c) {
        mTestRunCallbacks.add(c);
    }

    /** Register this receiver using the provided context. */
    public void register(Context c) {
        c.registerReceiver(this, INTENT_FILTER);
    }

    /** Receive a broadcast intent.
     *
     * @param context The Context in which the receiver is running.
     * @param intent The intent received.
     */
    @Override
    public void onReceive(Context context, Intent intent) {
        int pid = intent.getIntExtra(TestStatusReporter.EXTRA_PID, 0);
        String stackTrace = intent.getStringExtra(TestStatusReporter.EXTRA_STACK_TRACE);

        switch (intent.getAction()) {
            case TestStatusReporter.ACTION_TEST_RUN_STARTED:
                for (TestRunCallback c: mTestRunCallbacks) {
                    c.testRunStarted(pid);
                }
                break;
            case TestStatusReporter.ACTION_TEST_RUN_FINISHED:
                for (TestRunCallback c: mTestRunCallbacks) {
                    c.testRunFinished(pid);
                }
                break;
            case TestStatusReporter.ACTION_UNCAUGHT_EXCEPTION:
                for (TestRunCallback c: mTestRunCallbacks) {
                    c.uncaughtException(pid, stackTrace);
                }
                break;
            default:
                Log.e(TAG, "Unrecognized intent received: %s", intent.toString());
                break;
        }
    }

}

