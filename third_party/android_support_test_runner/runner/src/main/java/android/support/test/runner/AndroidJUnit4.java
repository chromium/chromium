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

package android.support.test.runner;

import android.support.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import android.support.test.internal.util.AndroidRunnerParams;
import org.junit.runners.model.InitializationError;

/**
 * Aliases the current default Android JUnit 4 class runner, for future-proofing. If
 * future versions of JUnit change the default Runner class, they will also
 * change the definition of this class. Developers wanting to explicitly tag a
 * class as an Android JUnit 4 class should use {@code @RunWith(AndroidJUnit4.class)}
 */
public final class AndroidJUnit4 extends AndroidJUnit4ClassRunner {
    /**
     * Constructs a new instance of the default runner
     */
    public AndroidJUnit4(Class<?> klass, AndroidRunnerParams runnerParams)
            throws InitializationError {
        super(klass, runnerParams);
    }
}
