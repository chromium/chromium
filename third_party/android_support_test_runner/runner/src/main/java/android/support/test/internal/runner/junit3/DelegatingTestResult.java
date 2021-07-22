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

import java.util.Enumeration;
import junit.framework.AssertionFailedError;
import junit.framework.Protectable;
import junit.framework.Test;
import junit.framework.TestFailure;
import junit.framework.TestListener;
import junit.framework.TestResult;

/**
 * A {@link TestResult} that delegates all calls to another {@link TestResult}.
 */
class DelegatingTestResult extends TestResult {

    private TestResult mWrappedResult;

    DelegatingTestResult(TestResult wrappedResult) {
        mWrappedResult = wrappedResult;
    }

    @Override
    public void addError(Test test, Throwable t) {
        mWrappedResult.addError(test, t);
    }

    @Override
    public void addFailure(Test test, AssertionFailedError t) {
        mWrappedResult.addFailure(test, t);
    }

    @Override
    public void addListener(TestListener listener) {
        mWrappedResult.addListener(listener);
    }

    @Override
    public void removeListener(TestListener listener) {
        mWrappedResult.removeListener(listener);
    }

    @Override
    public void endTest(Test test) {
        mWrappedResult.endTest(test);
    }

    @Override
    public int errorCount() {
        return mWrappedResult.errorCount();
    }

    @Override
    public Enumeration<TestFailure> errors() {
        return mWrappedResult.errors();
    }

    @Override
    public int failureCount() {
        return mWrappedResult.failureCount();
    }

    @Override
    public Enumeration<TestFailure> failures() {
        return mWrappedResult.failures();
    }

    @Override
    public int runCount() {
        return mWrappedResult.runCount();
    }

    @Override
    public void runProtected(final Test test, Protectable p) {
        mWrappedResult.runProtected(test, p);
    }

    @Override
    public boolean shouldStop() {
        return mWrappedResult.shouldStop();
    }

    @Override
    public void startTest(Test test) {
        mWrappedResult.startTest(test);
    }

    @Override
    public void stop() {
        mWrappedResult.stop();
    }

    @Override
    public boolean wasSuccessful() {
        return mWrappedResult.wasSuccessful();
    }
}
