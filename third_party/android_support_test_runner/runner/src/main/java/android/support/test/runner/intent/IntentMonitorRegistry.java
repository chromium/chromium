/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.support.test.runner.intent;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Exposes an implementation of {@link IntentMonitor} to users.
 */
public final class IntentMonitorRegistry {

    private static final AtomicReference<IntentMonitor> mMonitorRef =
            new AtomicReference<IntentMonitor>(null);

    /**
     * Returns the IntentMonitor. This monitor is not guaranteed to be present under all'
     * instrumentations.
     *
     * @throws IllegalStateException if no monitor has been registered.
     */
    public static IntentMonitor getInstance() {
        IntentMonitor instance = mMonitorRef.get();
        if (null == instance) {
            throw new IllegalStateException(
                    "No intent monitor registered! Are you running under an "
                            + "Instrumentation which registers intent monitors?");
        }
        return instance;
    }

    /**
     * Stores the given {@link IntentMonitor} instance in the registry. Passing null removes the
     * monitor from the registry.
     */
    public static void registerInstance(IntentMonitor monitor) {
        mMonitorRef.set(monitor);
    }

    private IntentMonitorRegistry() {
        // no instance
    }
}
