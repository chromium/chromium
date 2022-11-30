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

import android.support.test.internal.util.AndroidRunnerParams;
import android.util.Log;
import junit.framework.TestCase;
import org.junit.internal.builders.JUnit3Builder;
import org.junit.runner.Runner;
import org.junit.runners.model.RunnerBuilder;

/**
 * A {@link RunnerBuilder} that will build customized runners needed for specialized Android
 * {@link TestCase}s and to support {@link android.test.suitebuilder.annotation}s.
 */
public class AndroidJUnit3Builder extends JUnit3Builder {

    private static final String LOG_TAG = "AndroidJUnit3Builder";
    private final AndroidRunnerParams mAndroidRunnerParams;

    /**
     * @param runnerParams {@link AndroidRunnerParams} that stores common runner parameters
     */
    public AndroidJUnit3Builder(AndroidRunnerParams runnerParams) {
        mAndroidRunnerParams = runnerParams;
    }

    @Override
    public Runner runnerForClass(Class<?> testClass) throws Throwable {
        try {
            if (isJUnit3Test(testClass)) {
                if (mAndroidRunnerParams.isSkipExecution()) {
                    return new JUnit38ClassRunner(new NoExecTestSuite(testClass));
                } else {
                    JUnit38ClassRunner runner = new JUnit38ClassRunner(
                            new AndroidTestSuite(testClass, mAndroidRunnerParams));
                    return runner;
                }
            }
        } catch (Throwable e) {
            // log error message including stack trace before throwing to help with debugging.
            Log.e(LOG_TAG, "Error constructing runner", e);
            throw e;
        }
        return null;
    }

    boolean isJUnit3Test(Class<?> testClass) {
        return junit.framework.TestCase.class.isAssignableFrom(testClass);
    }
}
