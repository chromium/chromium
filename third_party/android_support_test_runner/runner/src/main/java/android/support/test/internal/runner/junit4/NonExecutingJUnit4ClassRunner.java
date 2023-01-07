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

import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

/**
 * A specialized {@link BlockJUnit4ClassRunner} that will generate test results, by skipping test
 * execution and loading.
 */
class NonExecutingJUnit4ClassRunner extends BlockJUnit4ClassRunner {

    private static final Statement NON_EXECUTING_STATEMENT = new Statement() {
        @Override
        public void evaluate() throws Throwable {
            // do nothing
        }
    };

    public NonExecutingJUnit4ClassRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }

    /**
     * Override parent to generate an non executing statement
     */
    @Override
    protected Statement methodBlock(FrameworkMethod method) {
        return NON_EXECUTING_STATEMENT;
    }
}
