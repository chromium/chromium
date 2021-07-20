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

import android.app.Instrumentation;
import android.os.Bundle;
import android.test.AndroidTestCase;
import android.test.InstrumentationTestCase;
import java.util.concurrent.TimeoutException;
import junit.framework.AssertionFailedError;
import junit.framework.Protectable;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestResult;

/**
 * A specialized {@link TestResult} that injects Android constructs into the test if necessary.
 */
class AndroidTestResult extends DelegatingTestResult {

    private final Instrumentation mInstr;
    private final Bundle mBundle;

    private long mTimeout;

    AndroidTestResult(Bundle bundle, Instrumentation instr, TestResult result) {
        super(result);
        mBundle = bundle;
        mInstr = instr;
    }

    @Override
    protected void run(final TestCase test) {
        if (test instanceof AndroidTestCase) {
            ((AndroidTestCase)test).setContext(mInstr.getTargetContext());
        }
        if (test instanceof InstrumentationTestCase) {
            ((InstrumentationTestCase)test).injectInstrumentation(mInstr);
        }
        super.run(test);
    }

    /**
     * Save the timeout value to be able to report a more user friendly error in case a timed
     * out test.
     *
     * @param timeout the timeout value
     * @see #runProtected(Test, Protectable)
     */
    void setCurrentTimeout(long timeout) {
        mTimeout = timeout;
    }

    /**
     * Timeout aware copy of {@link TestResult#runProtected(Test, Protectable)}. In case of a timed
     * out test an {@link InterruptedException} will be thrown and handled to report a more user
     * friendly error back to the user.
     */
    @Override
    public void runProtected(final Test test, Protectable p) {
        try {
            p.protect();
        }
        catch (AssertionFailedError e) {
            super.addFailure(test, e);
        }
        catch (ThreadDeath e) { // don't catch ThreadDeath by accident
            throw e;
        }
        catch (InterruptedException e) {
            super.addError(test, new TimeoutException(String.format(
                    "Test timed out after %d milliseconds", mTimeout)));
        }
        catch (Throwable e) {
            super.addError(test, e);
        }
    }
}
