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
package android.support.test.internal.runner.junit4;

import android.support.test.internal.util.AndroidRunnerParams;
import org.junit.Test;
import org.junit.internal.runners.statements.FailOnTimeout;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;


/**
 * A specialized {@link BlockJUnit4ClassRunner} that can handle timeouts
 */
public class AndroidJUnit4ClassRunner extends BlockJUnit4ClassRunner {

    private static final String LOG_TAG = "AndroidJUnit4ClassRunner";
    private final AndroidRunnerParams mAndroidRunnerParams;

    public AndroidJUnit4ClassRunner(Class<?> klass, AndroidRunnerParams runnerParams)
            throws InitializationError {
        super(klass);
        mAndroidRunnerParams = runnerParams;
    }

    /**
     * Default to <a href="http://junit.org/javadoc/latest/org/junit/Test.html">
     * <code>Test</code></a> level timeout if set. Otherwise, set the timeout that was passed to the
     * instrumentation via argument
     */
    @Override
    protected Statement withPotentialTimeout(FrameworkMethod method, Object test, Statement next) {
        long timeout = getTimeout(method.getAnnotation(Test.class));
        if (timeout > 0) {
            return new FailOnTimeout(next, timeout);
        } else if (mAndroidRunnerParams.getPerTestTimeout() > 0) {
            return new FailOnTimeout(next, mAndroidRunnerParams.getPerTestTimeout());
        }
        return next;
    }

    private long getTimeout(Test annotation) {
        if (annotation == null) {
            return 0;
        }
        return annotation.timeout();
    }
}
