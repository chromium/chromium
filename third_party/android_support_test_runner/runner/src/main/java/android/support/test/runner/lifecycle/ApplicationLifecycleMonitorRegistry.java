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

package android.support.test.runner.lifecycle;

import java.util.concurrent.atomic.AtomicReference;

/**
 * An exposed registry instance to make it easy for callers to find the application lifecycle
 * monitor for their application.
 */
public final class ApplicationLifecycleMonitorRegistry {

    private static final AtomicReference<ApplicationLifecycleMonitor> sLifecycleMonitor =
            new AtomicReference<ApplicationLifecycleMonitor>(null);

    // singleton - disallow creation
    private ApplicationLifecycleMonitorRegistry() { }

    /**
     * Returns the ActivityLifecycleMonitor.
     *
     * This monitor is not guaranteed to be present under all instrumentations.
     *
     * @return ActivityLifecycleMonitor the monitor for this application.
     * @throws IllegalStateException if no monitor has been registered.
     */
    public static ApplicationLifecycleMonitor getInstance() {
        ApplicationLifecycleMonitor instance = sLifecycleMonitor.get();
        if (null == instance) {
            throw new IllegalStateException("No lifecycle monitor registered! Are you running "
                    + "under an Instrumentation which registers lifecycle monitors?");
        }
        return instance;
    }

    /**
     * Stores a lifecycle monitor in the registry.
     * <p>
     * This is a global registry - so be aware of the impact of calling this method!
     * </p>
     *
     * @param monitor the monitor for this application. Null deregisters any existing monitor.
     */
    public static void registerInstance(ApplicationLifecycleMonitor monitor) {
        sLifecycleMonitor.set(monitor);
    }
}
