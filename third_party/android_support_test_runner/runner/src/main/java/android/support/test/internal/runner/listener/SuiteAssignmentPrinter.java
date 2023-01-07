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

package android.support.test.internal.runner.listener;

import android.support.test.internal.runner.TestRequestBuilder;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import org.junit.runner.Description;
import org.junit.runner.notification.Failure;

/**
 * This class measures the elapsed run time of each test, and used it to report back to the user
 * which suite ({@link SmallSuite}, {@link MediumSuite}, {@link LargeSuite}) the test should belong
 * to.
 */
public class SuiteAssignmentPrinter extends InstrumentationRunListener {
    /**
     * This constant defines the maximum allowed runtime (in ms) for a test included in the "small"
     * suite. It is used to make an educated guess at what suite an unlabeled test belongs to.
     */
    private static final float SMALL_SUITE_MAX_RUNTIME = 200;

    /**
     * This constant defines the maximum allowed runtime (in ms) for a test included in the "medium"
     * suite. It is used to make an educated guess at what suite an unlabeled test belongs to.
     */
    private static final float MEDIUM_SUITE_MAX_RUNTIME = 1000;

    private long mStartTime;
    private boolean mTimingValid;

    @Override
    public void testStarted(Description description) throws Exception {
        mTimingValid = true;
        mStartTime = System.currentTimeMillis();
    }

    @Override
    public void testFinished(Description description) throws Exception {
        long runTime;
        String assignmentSuite;
        long endTime = System.currentTimeMillis();

        if (!mTimingValid || mStartTime < 0) {
            assignmentSuite = "NA";
            runTime = -1;
            sendString("F");
            Log.d("SuiteAssignmentPrinter", String.format(
                    "%s#%s: skipping suite assignment due to test failure\n", description.getClassName(),
                    description.getMethodName()));
        } else {
            runTime = endTime - mStartTime;
            if (runTime < SMALL_SUITE_MAX_RUNTIME) {
                assignmentSuite = TestRequestBuilder.SMALL_SIZE;
            } else if (runTime < MEDIUM_SUITE_MAX_RUNTIME) {
                assignmentSuite = TestRequestBuilder.MEDIUM_SIZE;
            } else {
                assignmentSuite = TestRequestBuilder.LARGE_SIZE;
            }

            String currentSize = getTestSize(description);
            if (!assignmentSuite.equals(currentSize)) {
                // test size != runtime
                sendString(String.format("\n%s#%s: current size: %s. suggested: %s runTime: %d ms\n",
                        description.getClassName(), description.getMethodName(), currentSize,
                        assignmentSuite, runTime));
            } else {
                sendString(".");
                Log.d("SuiteAssignmentPrinter", String.format(
                        "%s#%s assigned correctly as %s. runTime: %d ms\n", description.getClassName(),
                        description.getMethodName(), assignmentSuite, runTime));
            }
        }
        // Clear mStartTime so that we can verify that it gets set next time.
        mStartTime = -1;

    }

    private String getTestSize(Description description) {
        String testSize = getTestSizeFromMethod(description);
        if (testSize != null) {
            return testSize;
        }
        return getTestSizeFromClass(description);
    }

    String getTestSizeFromMethod(Description desc) {
        if (desc.getAnnotation(SmallTest.class) != null) {
            return TestRequestBuilder.SMALL_SIZE;
        } else if (desc.getAnnotation(MediumTest.class) != null) {
            return TestRequestBuilder.MEDIUM_SIZE;
        } else if (desc.getAnnotation(LargeTest.class) != null) {
            return TestRequestBuilder.LARGE_SIZE;
        }
        return null;

    }

    String getTestSizeFromClass(Description desc) {
        Class<?> testClass = desc.getTestClass();
        if (testClass == null) {
            return null;
        }
        if (testClass.isAnnotationPresent(SmallTest.class)) {
            return TestRequestBuilder.SMALL_SIZE;
        } else if (testClass.isAnnotationPresent(MediumTest.class)) {
            return TestRequestBuilder.MEDIUM_SIZE;
        } else if (testClass.isAnnotationPresent(LargeTest.class)) {
            return TestRequestBuilder.LARGE_SIZE;
        }
        return null;
    }

    @Override
    public void testFailure(Failure failure) throws Exception {
        mTimingValid = false;
    }

    @Override
    public void testAssumptionFailure(Failure failure) {
        mTimingValid = false;
    }

    @Override
    public void testIgnored(Description description) throws Exception {
        mTimingValid = false;
    }
}
