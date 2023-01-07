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
import junit.framework.Test;
import junit.framework.TestResult;
import junit.framework.TestSuite;
import org.junit.Ignore;

/**
 * A {@link TestSuite} that delegates all calls to another {@link TestSuite}.
 */
@Ignore
class DelegatingTestSuite extends TestSuite {

    private TestSuite mWrappedSuite;

    public DelegatingTestSuite(TestSuite suiteDelegate) {
        super();
        mWrappedSuite = suiteDelegate;
    }

    /**
     * Return the suite to delegate to
     */
    public TestSuite getDelegateSuite() {
        return mWrappedSuite;
    }

    /**
     * Replace the suite to delegate to
     *
     * @param newSuiteDelegate
     */
    public void setDelegateSuite(TestSuite newSuiteDelegate) {
        mWrappedSuite = newSuiteDelegate;
    }

    @Override
    public void addTest(Test test) {
        mWrappedSuite.addTest(test);
    }

    @Override
    public int countTestCases() {
        return mWrappedSuite.countTestCases();
    }

    @Override
    public String getName() {
        return mWrappedSuite.getName();
    }

    @Override
    public void runTest(Test test, TestResult result) {
        mWrappedSuite.runTest(test, result);
    }

    @Override
    public void setName(String name) {
        mWrappedSuite.setName(name);
    }

    @Override
    public Test testAt(int index) {
        return mWrappedSuite.testAt(index);
    }

    @Override
    public int testCount() {
        return mWrappedSuite.testCount();
    }

    @Override
    public Enumeration<Test> tests() {
        return mWrappedSuite.tests();
    }

    @Override
    public String toString() {
        return mWrappedSuite.toString();
    }

    @Override
    public void run(TestResult result) {
        mWrappedSuite.run(result);
    }
}
