// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.test.reporter;

import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Broadcasts test status to any listening {@link org.chromium.test.reporter.TestStatusReceiver}.
 */
public class TestStatusReporter {

    public static final String ACTION_TEST_RUN_STARTED =
            "org.chromium.test.reporter.TestStatusReporter.TEST_RUN_STARTED";
    public static final String ACTION_TEST_RUN_FINISHED =
            "org.chromium.test.reporter.TestStatusReporter.TEST_RUN_FINISHED";
    public static final String ACTION_UNCAUGHT_EXCEPTION =
            "org.chromium.test.reporter.TestStatusReporter.UNCAUGHT_EXCEPTION";
    public static final String DATA_TYPE_RESULT = "org.chromium.test.reporter/result";
    public static final String EXTRA_PID =
            "org.chromium.test.reporter.TestStatusReporter.PID";
    public static final String EXTRA_STACK_TRACE =
            "org.chromium.test.reporter.TestStatusReporter.STACK_TRACE";

    private final Context mContext;

    public TestStatusReporter(Context c) {
        mContext = c;
    }

    public void testRunStarted(int pid) {
        sendTestRunBroadcast(ACTION_TEST_RUN_STARTED, pid);
    }

    public void testRunFinished(int pid) {
        sendTestRunBroadcast(ACTION_TEST_RUN_FINISHED, pid);
    }

    private void sendTestRunBroadcast(String action, int pid) {
        Intent i = new Intent(action);
        i.setType(DATA_TYPE_RESULT);
        i.putExtra(EXTRA_PID, pid);
        mContext.sendBroadcast(i);
    }

    public void uncaughtException(int pid, Throwable ex) {
        Intent i = new Intent(ACTION_UNCAUGHT_EXCEPTION);
        i.setType(DATA_TYPE_RESULT);
        i.putExtra(EXTRA_PID, pid);
        i.putExtra(EXTRA_STACK_TRACE, Log.getStackTraceString(ex));
        mContext.sendBroadcast(i);
    }
}
