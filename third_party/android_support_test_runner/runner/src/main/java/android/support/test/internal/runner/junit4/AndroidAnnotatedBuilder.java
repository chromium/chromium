/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.support.test.internal.runner.junit4;

import android.support.test.internal.util.AndroidRunnerParams;
import android.support.test.runner.AndroidJUnit4;
import org.junit.internal.builders.AnnotatedBuilder;
import org.junit.runner.RunWith;
import org.junit.runner.Runner;
import org.junit.runners.model.RunnerBuilder;

/**
 * A specialized {@link AnnotatedBuilder} that can Android runner specific features
 */
public class AndroidAnnotatedBuilder extends AnnotatedBuilder {
    private static final String LOG_TAG = "AndroidAnnotatedBuilder";

    private final AndroidRunnerParams mAndroidRunnerParams;

    public AndroidAnnotatedBuilder(RunnerBuilder suiteBuilder, AndroidRunnerParams runnerParams) {
        super(suiteBuilder);
        mAndroidRunnerParams = runnerParams;
    }

    @Override
    public Runner runnerForClass(Class<?> testClass) throws Exception {
        RunWith annotation = testClass.getAnnotation(RunWith.class);
        // check if its an Android specific runner otherwise default to AnnotatedBuilder
        if (annotation != null && annotation.value().equals(AndroidJUnit4.class)) {
            Runner runner = buildAndroidRunner(annotation.value(), testClass);
            if (runner != null) {
                return runner;
            }
        }
        // default to AnnotatedBuilder implementation
        return super.runnerForClass(testClass);
    }

    public Runner buildAndroidRunner(Class<? extends Runner> runnerClass,
                                     Class<?> testClass) throws Exception {
        try {
            return runnerClass.getConstructor(Class.class, AndroidRunnerParams.class).newInstance(
                    testClass, mAndroidRunnerParams);
        } catch (NoSuchMethodException e) {
            return null;
        }
    }
}
