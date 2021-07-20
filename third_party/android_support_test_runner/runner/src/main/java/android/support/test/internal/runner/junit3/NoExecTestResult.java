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

import junit.framework.TestCase;
import junit.framework.TestResult;

/**
 * A benign test result that does no actually test execution, just runs
 * through the motions
 */
class NoExecTestResult extends DelegatingTestResult {

    NoExecTestResult(TestResult wrappedResult) {
        super(wrappedResult);
    }

    /**
     * Override parent to just inform listeners of test,
     * and skip test execution.
     */
    @Override
    protected void run(final TestCase test) {
        startTest(test);
        endTest(test);
    }
}
