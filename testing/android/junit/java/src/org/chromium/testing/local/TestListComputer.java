// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.runner.Computer;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.manipulation.Filter;
import org.junit.runner.manipulation.Filterable;
import org.junit.runner.manipulation.NoTestsRemainException;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.RunnerBuilder;

import java.io.PrintStream;

class TestListComputer extends Computer {
    private final PrintStream mOutputStream;

    TestListComputer(PrintStream mOutputStream) {
        this.mOutputStream = mOutputStream;
    }

    private class TestListRunner extends Runner implements Filterable {
        private final Runner mRunner;

        public TestListRunner(Runner contained) {
            mRunner = contained;
        }

        @Override
        public Description getDescription() {
            return mRunner.getDescription();
        }

        private void printTests(Description description) {
            if (description.getMethodName() != null) {
                mOutputStream.println(
                        "#TEST# " + description.getClassName() + '#' + description.getMethodName());
            }
            for (Description child : description.getChildren()) {
                printTests(child);
            }
        }

        @Override
        public void run(RunNotifier notifier) {
            printTests(mRunner.getDescription());
        }

        @Override
        public void filter(Filter filter) throws NoTestsRemainException {
            if (mRunner instanceof Filterable) {
                ((Filterable) mRunner).filter(filter);
            }
        }
    }

    @Override
    public Runner getSuite(final RunnerBuilder builder, Class<?>[] classes)
            throws InitializationError {
        return super.getSuite(new RunnerBuilder() {
            @Override
            public Runner runnerForClass(Class<?> testClass) throws Throwable {
                return new TestListRunner(builder.runnerForClass(testClass));
            }
        }, classes);
    }
}
