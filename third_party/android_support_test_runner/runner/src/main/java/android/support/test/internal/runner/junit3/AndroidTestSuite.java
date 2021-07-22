/*
 * Copyright (C) 2012 The Android Open Source Project
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
package android.support.test.internal.runner.junit3;

import android.os.Looper;
import android.support.test.internal.util.AndroidRunnerParams;
import android.util.Log;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import junit.framework.Test;
import junit.framework.TestResult;
import junit.framework.TestSuite;
import org.junit.Ignore;

/**
 * An extension of {@link TestSuite} that supports Android construct injection into test cases,
 * and properly supports test timeouts and annotation filtering of test cases.
 * <p/>
 * Also tries to use {@link NonLeakyTestSuite} where possible to save memory.
 */
@Ignore
class AndroidTestSuite extends DelegatingFilterableTestSuite {
    private static final String TAG = "AndroidTestSuite";

    private final AndroidRunnerParams mAndroidRunnerParams;

    public AndroidTestSuite(Class<?> testClass, AndroidRunnerParams runnerParams) {
        this(new NonLeakyTestSuite(testClass), runnerParams);
    }

    public AndroidTestSuite(TestSuite s, AndroidRunnerParams runnerParams) {
        super(s);
        mAndroidRunnerParams = runnerParams;
    }

    @Override
    public void run(TestResult result) {
        // wrap the result in a new AndroidTestResult to do the bundle and instrumentation injection
        AndroidTestResult androidTestResult = new AndroidTestResult(
                mAndroidRunnerParams.getBundle(),
                mAndroidRunnerParams.getInstrumentation(),
                result);

        long timeout = mAndroidRunnerParams.getPerTestTimeout();
        if (timeout > 0) {
            runTestsWithTimeout(timeout, androidTestResult);
        } else {
            super.run(androidTestResult);
        }
    }

    /**
     * Executes all tests within a {@link junit.framework.TestSuite} individually on a separate
     * thread with a specified timeout.
     */
    private void runTestsWithTimeout(long timeout, AndroidTestResult result) {
        int suiteSize = testCount();
        for (int i = 0; i < suiteSize; i++) {
            Test test = testAt(i);
            runTestWithTimeout(test, result, timeout);
        }
    }

    /**
     * Executes {@link junit.framework.Test} on a separate thread with a specified timeout.
     */
    private void runTestWithTimeout(final Test test,
                                    final AndroidTestResult androidTestResult,
                                    final long timeout) {

        // Create a new thread to execute the test on
        ExecutorService threadExecutor = Executors.newSingleThreadExecutor();
        // Wraps test execution in a Runnable so that it can be passed to the new thread
        final Runnable execRunnable = new Runnable() {
            @Override
            public void run() {
                test.run(androidTestResult);
            }
        };

        androidTestResult.setCurrentTimeout(timeout);

        Future<?> result = threadExecutor.submit(execRunnable);
        // Run the test by initiating an orderly shutdown in which previously submitted tasks
        // are executed, but no new tasks will be accepted.
        threadExecutor.shutdown();
        try {
            boolean isTerminated = threadExecutor.awaitTermination(timeout, TimeUnit.MILLISECONDS);
            if (!isTerminated) {
                // The test didn't finish executing within the given timeout, stop the test by
                // sending Thread.interrupt() to the executing task.
                threadExecutor.shutdownNow();
                // Block for at most 1 min to ensure the test execution test is stopped.
                isTerminated = threadExecutor.awaitTermination(1, TimeUnit.MINUTES);
                if (!isTerminated) {
                    Log.e(TAG, "Failed to to stop test execution thread, the correctness of the " +
                            "test runner is at risk. Abort all execution!");

                    try {
                        // throws the exception if one occurred during the invocation
                        result.get(0, TimeUnit.MILLISECONDS);
                    } catch (ExecutionException e) {
                        Log.e(TAG, "Exception from the execution thread", e.getCause());
                    } catch (TimeoutException e) {
                        Log.e(TAG, "Exception from the execution thread", e);
                    }

                    terminateAllRunnerExecution(new IllegalStateException(String.format("Test " +
                            "timed out after %d milliseconds but execution thread failed to " +
                            "terminate\nDumping instr and main threads:\n%s", timeout,
                            getStackTraces())));
                }
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "The correctness of the test runner is at risk. Abort all execution!");
            terminateAllRunnerExecution(new IllegalStateException(String.format("Test execution " +
                    "thread got interrupted:\n%s\nDumping instr and main threads:\n%s", e,
                    getStackTraces())));
        }
    }

    /**
     * Crash runner process to abort all execution.
     *
     * @param exception Descriptive runtime exception termination reason
     */
    private void terminateAllRunnerExecution(final RuntimeException exception) {
        Runnable r = new Runnable() {
            @Override
            public void run() {
                throw exception;
            }
        };
        Thread t = new Thread(r, "Terminator");
        t.start();
        try {
            // Blocks the current Thread (Thread.currentThread()) until the receiver finishes its
            // execution and dies.
            t.join();
        } catch (InterruptedException e) {
            // ignore, about to crash anyway, I will not be back!
        }
    }

    /**
     * Gets instrumentation and main thread's stack traces.
     *
     * @return string of instrumentation and main thread's stack traces
     */
    private String getStackTraces() {
        StringBuilder sb = new StringBuilder();
        Thread t = Thread.currentThread();
        sb.append(t.toString()).append('\n');
        for (StackTraceElement ste : t.getStackTrace()) {
            sb.append("\tat ").append(ste.toString()).append('\n');
        }
        sb.append('\n');
        t = Looper.getMainLooper().getThread();
        sb.append(t.toString()).append('\n');
        for (StackTraceElement ste : t.getStackTrace()) {
            sb.append("\tat ").append(ste.toString()).append('\n');
        }
        sb.append('\n');
        return sb.toString();
    }
}
