/*
 * Copyright (C) 2014 The Android Open Source Project
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

import junit.framework.TestResult;
import junit.framework.TestSuite;
import org.junit.Ignore;

/**
 * A benign {@link TestSuite} that skips test execution.
 */
@Ignore
class NoExecTestSuite extends DelegatingFilterableTestSuite {

    public NoExecTestSuite(Class<?> testClass) {
        this(new TestSuite(testClass));
    }

    public NoExecTestSuite(TestSuite s) {
        super(s);
    }

    @Override
    public void run(TestResult result) {
        // wraps the parent result with a container that skips execution
        super.run(new NoExecTestResult(result));
    }
}
