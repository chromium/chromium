/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package android.support.test.internal.runner.listener;

import android.util.Log;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;

/**
 * A <a href="http://junit.org/javadoc/latest/org/junit/runner/notification/RunListener.html">
 * <code>RunListener</code></a> that outputs test events to logcat.
 * <p/>
 * Attempts to follow similar format as InstrumentationTestRunner.
 */
public class LogRunListener extends RunListener {

    // use tag consistent with InstrumentationTestRunner
    private static final String TAG = "TestRunner";

    @Override
    public void testRunStarted(Description description) throws Exception {
        Log.i(TAG, String.format("run started: %d tests", description.testCount()));
    }

    @Override
    public void testRunFinished(Result result) throws Exception {
        Log.i(TAG, String.format("run finished: %d tests, %d failed, %d ignored",
                result.getRunCount(), result.getFailureCount(), result.getIgnoreCount()));
    }

    @Override
    public void testStarted(Description description) throws Exception {
        Log.i(TAG, "started: " + description.getDisplayName());
    }

    @Override
    public void testFinished(Description description) throws Exception {
        Log.i(TAG, "finished: " + description.getDisplayName());
    }

    @Override
    public void testFailure(Failure failure) throws Exception {
        Log.i(TAG, "failed: " + failure.getDescription().getDisplayName());
        Log.i(TAG, "----- begin exception -----");
        Log.i(TAG, failure.getTrace());
        Log.i(TAG, "----- end exception -----");
    }

    @Override
    public void testAssumptionFailure(Failure failure) {
        Log.i(TAG, "assumption failed: " + failure.getDescription().getDisplayName());
        Log.i(TAG, "----- begin exception -----");
        Log.i(TAG, failure.getTrace());
        Log.i(TAG, "----- end exception -----");
    }

    @Override
    public void testIgnored(Description description) throws Exception {
        Log.i(TAG, "ignored: " + description.getDisplayName());
    }
}
