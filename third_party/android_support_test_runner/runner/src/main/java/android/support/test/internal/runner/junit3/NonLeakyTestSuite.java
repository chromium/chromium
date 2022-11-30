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

import junit.framework.Test;
import junit.framework.TestResult;
import junit.framework.TestSuite;
import org.junit.Ignore;
import org.junit.runner.Describable;
import org.junit.runner.Description;

/**
 * A {@link TestSuite} that discards references to included tests when execution is complete.
 * Done so tests can be garbage collected and memory freed.
 */
@Ignore
public class NonLeakyTestSuite extends TestSuite {
    public NonLeakyTestSuite(Class<?> testClass) {
        super(testClass);
    }

    @Override
    public void addTest(Test test) {
        super.addTest(new NonLeakyTest(test));
    }

    private static class NonLeakyTest implements Test, Describable {
        private Test mDelegate;
        private final Description mDesc;

        NonLeakyTest(Test delegate) {
            this.mDelegate = delegate;
            // cache description so it's available after execution
            this.mDesc = JUnit38ClassRunner.makeDescription(mDelegate);
        }

        @Override
        public int countTestCases() {
            if (mDelegate != null) {
                return mDelegate.countTestCases();
            } else {
                return 0;
            }
        }

        @Override
        public void run(TestResult result) {
            mDelegate.run(result);
            mDelegate = null;
        }

        @Override
        public Description getDescription() {
            return mDesc;
        }

        @Override
        public String toString() {
            if (mDelegate != null) {
                return mDelegate.toString();
            } else {
                return mDesc.toString();
            }
        }
    }
}
