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
import junit.framework.TestSuite;
import org.junit.Ignore;
import org.junit.runner.Description;
import org.junit.runner.manipulation.Filter;
import org.junit.runner.manipulation.Filterable;
import org.junit.runner.manipulation.NoTestsRemainException;

/**
 * A {@link DelegatingTestSuite} that is {@link Filterable}.
 */
@Ignore
class DelegatingFilterableTestSuite extends DelegatingTestSuite implements Filterable {

    public DelegatingFilterableTestSuite(TestSuite suiteDelegate) {
        super(suiteDelegate);
    }

    @Override
    public void filter(Filter filter) throws NoTestsRemainException {
        TestSuite suite = getDelegateSuite();
        TestSuite filtered = new TestSuite(suite.getName());
        int n = suite.testCount();
        for (int i = 0; i < n; i++) {
            Test test = suite.testAt(i);
            if (filter.shouldRun(makeDescription(test))) {
                filtered.addTest(test);
            }
        }
        setDelegateSuite(filtered);
        if (filtered.testCount() == 0) {
            throw new NoTestsRemainException();
        }
    }

    private static Description makeDescription(Test test) {
        // delegate to JUnit38ClassRunner copy.
        return JUnit38ClassRunner.makeDescription(test);
    }
}
